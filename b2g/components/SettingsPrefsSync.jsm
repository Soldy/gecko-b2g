/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const isGonk = AppConstants.platform === "gonk";

if (isGonk) {
  XPCOMUtils.defineLazyGetter(this, "libcutils", function() {
    const { libcutils } = ChromeUtils.import(
      "resource://gre/modules/systemlibs.js"
    );
    return libcutils;
  });
}

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gSettingsManager",
  "@mozilla.org/sidl-native/settings;1",
  "nsISettingsManager"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "uuidgen",
  "@mozilla.org/uuid-generator;1",
  "nsIUUIDGenerator"
);

this.EXPORTED_SYMBOLS = ["SettingsPrefsSync"];

// Returns a settings.set(...) callback that just logs errors
// with a custom message.
function settingCallback(message) {
  return {
    resolve: () => {},
    reject: () => {
      console.error(message);
    },
  };
}

const kSettingsToObserve = {
  "apz.overscroll.enabled": true,
  "browser.safebrowsing.enabled": true,
  "browser.safebrowsing.malware.enabled": true,
  "device.storage.writable.name": "sdcard",
  "layers.effect.invert": false,
  "mms.debugging.enabled": false,
  "network.debugging.enabled": false,
  "privacy.donottrackheader.enabled": false,
  "privacy.trackingprotection.enabled": false,
  "ril.debugging.enabled": false,
  "ril.mms.requestReadReport.enabled": {
    prefName: "dom.mms.requestReadReport",
    defaultValue: true,
  },
  "ril.mms.requestStatusReport.enabled": {
    prefName: "dom.mms.requestStatusReport",
    defaultValue: false,
  },
  "ril.mms.retrieval_mode": {
    prefName: "dom.mms.retrieval_mode",
    defaultValue: "manual",
  },
  "ril.sms.requestStatusReport.enabled": {
    prefName: "dom.sms.requestStatusReport",
    defaultValue: false,
  },
  "ril.sms.strict7BitEncoding.enabled": {
    prefName: "dom.sms.strict7BitEncoding",
    defaultValue: false,
  },
  "ril.sms.maxReadAheadEntries": {
    prefName: "dom.sms.maxReadAheadEntries",
    defaultValue: 7,
  },
  "ui.prefers.color-theme": {
    prefName: "ui.systemUsesDarkTheme",
    defaultValue: 0,
    valueMap: { dark: 1 },
  },
  "ui.prefers.text-size": {
    prefName: "ui.prefersTextSizeId",
    defaultValue: 0,
    valueMap: { small: 1, large: 2 },
  },
};

this.SettingsPrefsSync = {
  // Initialize the synchronization and returns a promise that
  // resolves once we have done the early stage sync.
  start(aWindow) {
    // The full set of settings observers that were added, and will be
    // removed in the xpcom-shutdown phase.
    this.window = aWindow;
    this.settingsObservers = [];

    Services.obs.addObserver(this, "xpcom-shutdown");

    return new Promise(resolve => {
      this.getSettingWithDefault("language.current", "en-US").then(setting => {
        this.updateLanguage(setting.value, true);
        resolve();
      });
    });
  },

  // Update the setting "language.current" to Services.locale.requestedLocales and
  // "intl.accept_languages" preference.
  updateLanguage(value, forceSetPref) {
    // We only request a single locale, but need an array.
    Services.locale.requestedLocales = [value];

    let prefName = "intl.accept_languages";
    let intl = [];
    try {
      intl = Services.prefs
        .getComplexValue(prefName, Ci.nsIPrefLocalizedString)
        .data.split(",")
        .map(item => item.trim(" "));
    } catch (e) {}

    if (!intl.length) {
      Services.prefs.setCharPref(prefName, value);
    } else if (intl[0] != value) {
      // Insert value at the first position, and remove possible duplicated value.
      Services.prefs.setCharPref(
        prefName,
        [value, intl.filter(item => item != value)].join(", ")
      );
    } else if (forceSetPref) {
      // Bug 830782 - Homescreen is in English instead of selected locale after
      // the first run experience.
      // In order to ensure the current intl value is reflected on the child
      // process let's always write a user value, even if this one match the
      // current localized pref value.
      Services.prefs.setCharPref(prefName, intl.join(", "));
    }
  },

  // For nsIObserver
  observe(subject, topic, data) {
    if (topic !== "xpcom-shutdown") {
      console.error(
        `SettingsPrefsSync: unexpected observer notification '${topic}'`
      );
      return;
    }

    Services.obs.removeObserver(this, "xpcom-shutdown");

    this.settingsObservers.forEach(item => {
      gSettingsManager.removeObserver(
        item.key,
        item.observer,
        settingCallback(`Failed to add observer for ${item.key}`)
      );
    });
  },

  // Helper to add a setting observer and register it for later cleanup.
  addSettingsObserver(key, observer, message) {
    gSettingsManager.addObserver(key, observer, settingCallback(message));
    this.settingsObservers.push({ key, observer });
  },

  // Helper that resolves with the setting value or the default one, and
  // takes care of hiding the JSON stringification.
  getSettingWithDefault(name, defaultValue) {
    return new Promise(resolve => {
      gSettingsManager.get(name, {
        resolve: setting => {
          resolve({ name: setting.name, value: JSON.parse(setting.value) });
        },
        reject: () => {
          resolve({ name, value: defaultValue });
        },
      });
    });
  },

  // Will run all the steps that can be delayed after startup.
  // This module makes no decision about when that should run, letting
  // the caller decide since it can have better knowledge about the system state.
  delayedInit() {
    this.deviceInfoToSettings();
    this.setUpdateTrackingId();
    this.managePrivacy();
    this.setupAccessibility();
    this.synchronizePrefs();
    this.setupLanguageSettingObserver();
  },

  // Returns a Commercial Unit Reference which is vendor dependent.
  // For developer, a pref could be set from device config folder.
  get cuRef() {
    let cuRefStr;

    try {
      cuRefStr =
        Services.prefs.getPrefType("device.commercial.ref") ==
        Ci.nsIPrefBranch.PREF_STRING
          ? Services.prefs.getCharPref("device.commercial.ref")
          : undefined;
      if (!cuRefStr) {
        // TODO: Remove this pref since it will be deprecated in master and v2.5. See Bug-35271.
        cuRefStr =
          Services.prefs.getPrefType("device.cuRef.default") ==
          Ci.nsIPrefBranch.PREF_STRING
            ? Services.prefs.getCharPref("device.cuRef.default")
            : undefined;
      }
    } catch (e) {
      console.error(`get Commercial Unit Reference error: ${e}`);
    }
    return cuRefStr;
  },

  deviceInfoToSettings() {
    // MOZ_B2G_VERSION is set in b2g/confvars.sh, and is output as a #define value
    // from configure.in, defaults to 1.0.0 if this value is not exist.
    let os_version = AppConstants.MOZ_B2G_VERSION;
    let os_name = AppConstants.MOZ_B2G_OS_NAME;

    // Get the hardware info and firmware revision from device properties.
    let hardware_info = null;
    let firmware_revision = null;
    let product_manufacturer = null;
    let product_model = null;
    let product_device = null;
    let build_number = null;
    let sar_info = null;
    let version_tag = null;
    let base_version = null;
    let product_fota = null;
    let cuRefStr = null;
    if (isGonk) {
      hardware_info =
        libcutils.property_get("ro.product.model.name") ||
        libcutils.property_get("ro.hardware");
      firmware_revision = libcutils.property_get("ro.firmware_revision");
      product_manufacturer = libcutils.property_get("ro.product.manufacturer");
      product_model = libcutils.property_get("ro.product.model");
      product_device = libcutils.property_get("ro.product.device");
      build_number = libcutils.property_get("ro.build.version.incremental");
      sar_info = libcutils.property_get("ro.product.sar_value", "0");
      version_tag = libcutils.property_get("ro.product.version_tag");
      base_version = libcutils.property_get("ro.product.base_version");
      product_fota = libcutils.property_get("ro.product.fota");
      cuRefStr = this.cuRef || null;
    }

    // Populate deviceinfo settings,
    // copying any existing deviceinfo.os into deviceinfo.previous_os
    this.getSettingWithDefault("deviceinfo.os", null).then(setting => {
      let previous_os = setting.value || "";
      let software = os_name + " " + os_version;
      let deviceInfo = {
        build_number,
        os: os_version,
        previous_os,
        software,
        platform_version: Services.appinfo.platformVersion,
        platform_build_id: Services.appinfo.platformBuildID,
        hardware: hardware_info,
        firmware_revision,
        product_manufacturer,
        product_model,
        product_device,
        sar_value: sar_info,
        software_tag: version_tag,
        base_version,
        product_fota,
        // cu means Commercial Unit Reference
        cu: cuRefStr,
      };

      let settingsArray = [];
      for (let name in deviceInfo) {
        settingsArray.push({
          name: `deviceinfo.${name}`,
          // Replace null/undefined values by the empty string.
          // Maybe it would be better to not set them at all?
          value: JSON.stringify(deviceInfo[name] || ""),
        });
      }
      gSettingsManager.set(
        settingsArray,
        settingCallback("Failure saving deviceinfo settings.")
      );
    });
  },

  setUpdateTrackingId() {
    try {
      let dntEnabled = Services.prefs.getBoolPref(
        "privacy.donottrackheader.enabled"
      );

      let dntValue = Services.prefs.getIntPref(
        "privacy.donottrackheader.value",
        0
      );
      // If the user specifically decides to disallow tracking (1), we just bail out.
      if (dntEnabled && dntValue == 1) {
        return;
      }

      let trackingId =
        Services.prefs.getPrefType("app.update.custom") ==
          Ci.nsIPrefBranch.PREF_STRING &&
        Services.prefs.getCharPref("app.update.custom");

      // If there is no previous registered tracking ID, we generate a new one.
      // This should only happen on first usage or after changing the
      // do-not-track value from disallow to allow.
      if (!trackingId) {
        trackingId = uuidgen
          .generateUUID()
          .toString()
          .replace(/[{}]/g, "");
        Services.prefs.setCharPref("app.update.custom", trackingId);
        gSettingsManager.set(
          [{ name: "app.update.custom", value: JSON.stringify(trackingId) }],
          settingCallback("Failure saving app.update.custom setting.")
        );
      }
    } catch (e) {
      dump("Error getting tracking ID " + e + "\n");
    }
  },

  // Observes the privacy.donottrackheader.value setting and sets app.update.custom.
  // TODO: Add support for Tracking Protection.
  managePrivacy() {
    // Not using `self = this;` to not hold the whole object in the observer scope.
    let setUpdateTrackingId = this.setUpdateTrackingId;

    // The setting observer object.
    let observer = {
      observeSetting(info) {
        if (info) {
          let value = JSON.parse(info.value);

          Services.prefs.setIntPref("privacy.donottrackheader.value", value);
          // If the user specifically disallows tracking, we set the value of
          // app.update.custom (update tracking ID) to an empty string.
          if (value == 1) {
            Services.prefs.setCharPref("app.update.custom", "");
            return;
          }
          // Otherwise, we ensure that the update tracking ID exists.
          setUpdateTrackingId();
        }
      },
    };

    this.addSettingsObserver(
      "privacy.donottrackheader.value",
      observer,
      "Failed to add a setting observer for privacy.donottrackheader.value"
    );
  },

  // Observes the accessibility.screenreader setting and attach AccessFu.
  setupAccessibility() {
    let accessibilityScope = {};
    this.addSettingsObserver(
      "accessibility.screenreader",
      {
        observeSetting: info => {
          if (info) {
            let value = JSON.parse(info.value);
            if (!("AccessFu" in accessibilityScope)) {
              ChromeUtils.import(
                "resource://gre/modules/accessibility/AccessFu.jsm",
                accessibilityScope
              );
            }

            if (value) {
              accessibilityScope.AccessFu.attach(this.window);
            } else {
              accessibilityScope.AccessFu.detach();
            }
          }
        },
      },
      "Failed to add a setting observer for accessibility.screenreader"
    );
  },

  // Synchronizes a set of preferences with matching settings.
  synchronizePrefs() {
    for (let key in kSettingsToObserve) {
      let setting = kSettingsToObserve[key];

      // Allow setting to contain flags redefining prefName and defaultValue.
      let prefName = setting.prefName || key;
      let defaultValue = setting.defaultValue;
      if (defaultValue === undefined) {
        defaultValue = setting;
      }

      let prefs = Services.prefs;

      // If requested, reset setting value and defaultValue to the pref value.
      if (setting.resetToPref) {
        switch (prefs.getPrefType(prefName)) {
          case Ci.nsIPrefBranch.PREF_BOOL:
            defaultValue = prefs.getBoolPref(prefName);
            break;

          case Ci.nsIPrefBranch.PREF_INT:
            defaultValue = prefs.getIntPref(prefName);
            break;

          case Ci.nsIPrefBranch.PREF_STRING:
            defaultValue = prefs.getCharPref(prefName);
            break;
        }

        let setting = { name: key };
        setting.value = JSON.stringify(defaultValue);
        gSettingsManager.set(
          [setting],
          settingCallback(`Failed to set setting ${key}`)
        );
      }

      // Figure out the right setter function for this type of pref.
      let setPref;
      switch (typeof defaultValue) {
        case "boolean":
          setPref = prefs.setBoolPref.bind(prefs);
          break;
        case "number":
          setPref = prefs.setIntPref.bind(prefs);
          break;
        case "string":
          setPref = prefs.setCharPref.bind(prefs);
          break;
      }

      // Map value of setting to of prefs.
      let mapValue;
      if (setting.hasOwnProperty("valueMap")) {
        mapValue = v =>
          setting.valueMap.hasOwnProperty(v)
            ? setting.valueMap[v]
            : defaultValue;
      } else {
        mapValue = v => v;
      }

      // Add an observer for this setting.
      this.addSettingsObserver(
        key,
        {
          observeSetting: info => {
            if (!info) {
              return;
            }
            let value = JSON.parse(info.value);
            setPref(prefName, mapValue(value));
          },
        },
        `Failed to add observer for ${key}`
      );
    }
  },

  // Setup setting observer for language
  setupLanguageSettingObserver() {
    this.addSettingsObserver(
      "language.current",
      {
        observeSetting: info => {
          if (!info) {
            return;
          }
          let value = JSON.parse(info.value);
          this.updateLanguage(value, false);
        },
      },
      "Failed to add observer for language.current"
    );
  },
};
