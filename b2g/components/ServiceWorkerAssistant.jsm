/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { setTimeout, clearTimeout } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);

const serviceWorkerManagegr = Cc[
  "@mozilla.org/serviceworkers/manager;1"
].getService(Ci.nsIServiceWorkerManager);

const systemMessageService = Cc[
  "@mozilla.org/systemmessage-service;1"
].getService(Ci.nsISystemMessageService);

this.EXPORTED_SYMBOLS = ["ServiceWorkerAssistant"];

const DEBUG = true;
function debug(aMsg) {
  if (DEBUG) {
    dump(`-*- ServiceWorkerAssistant : ${aMsg}\n`);
  }
}

this.ServiceWorkerAssistant = {
  _timers: [],

  _hasContentReady: false,

  // The list of promises of pending service worker registrations.
  // Once we receive onBootDone, we wait for these to
  // settle before dispatching an observer notification
  // to signal that all service workers are ready.
  _pendingRegistrations: [],
  _waitForRegistrations: false,

  init() {
    Services.obs.addObserver(this, "ipc:first-content-process-created");
  },

  observe(aSubject, aTopic, aData) {
    debug(`Observe ${aTopic}`);
    this._hasContentReady = true;
    for (let timer of this._timers) {
      clearTimeout(timer.id);
      if (!timer.isCallbackExecuted) {
        this._doRegisterServiceWorker(
          timer.principal,
          timer.scope,
          timer.script,
          timer.updateViaCache
        );
      }
    }
    delete this._timers;
    Services.obs.removeObserver(this, "ipc:first-content-process-created");

    // If we need to, wait for pending registrations to settle.
    if (this._waitForRegistrations) {
      this.waitForRegistrations();
    }
  },

  /**
   * Register service worker, system messages, activities for a givien app.
   * Format of serviceworker in b2g_features, "options" is optional.
   * {
   *   "serviceworker": {
   *     "script_url": "script_url",
   *     "options": {
   *       "scope": "scope_of_sw",
   *       "update_via_cache": "value_of_update_via_cache"
   *     }
   *   },
   * }
   *
   */
  register(aManifestURL, aFeatures) {
    let serviceworker = aFeatures.serviceworker;
    if (!serviceworker) {
      if (aFeatures.messages) {
        debug(
          `Skip subscribing system messages for ${aManifestURL} because serviceworker is not defined.`
        );
      }
      if (aFeatures.activities) {
        debug(
          `Skip registering activities for ${aManifestURL} because serviceworker is not defined.`
        );
      }
      return;
    }
    debug(`register ${aManifestURL}`);

    let appURI = Services.io.newURI(aManifestURL);
    let fullpath = function(aPath) {
      return appURI.resolve(aPath ? aPath : "/");
    };
    let getUpdateViaCache = function(aUpdateViaCache) {
      if (!aUpdateViaCache) {
        return Ci.nsIServiceWorkerRegistrationInfo.UPDATE_VIA_CACHE_IMPORTS;
      }
      switch (aUpdateViaCache) {
        case "imports":
          return Ci.nsIServiceWorkerRegistrationInfo.UPDATE_VIA_CACHE_IMPORTS;
        case "all":
          return Ci.nsIServiceWorkerRegistrationInfo.UPDATE_VIA_CACHE_ALL;
        case "none":
          return Ci.nsIServiceWorkerRegistrationInfo.UPDATE_VIA_CACHE_NONE;
        default:
          return Ci.nsIServiceWorkerRegistrationInfo.UPDATE_VIA_CACHE_NONE + 1;
      }
    };

    let script = fullpath(serviceworker.script_url);
    let scope;
    let updateViaCache;
    if (serviceworker.options) {
      scope = serviceworker.options.scope;
      updateViaCache = serviceworker.options.update_via_cache;
    }
    scope = fullpath(scope);
    updateViaCache = getUpdateViaCache(updateViaCache);

    debug(` script: ${script}`);
    debug(` scope: ${scope}`);
    debug(` updateViaCache: ${updateViaCache}`);

    let ssm = Services.scriptSecurityManager;
    let principal = ssm.createContentPrincipal(appURI, {});
    this._subscribeSystemMessages(aFeatures, principal, scope);
    this._registerActivities(aManifestURL, aFeatures, principal, scope);

    if (this._hasContentReady) {
      this._doRegisterServiceWorker(principal, scope, script, updateViaCache);
    } else {
      let last = this._timers.length;
      this._timers[last] = {
        principal,
        scope,
        script,
        updateViaCache,
      };

      this._timers[last].id = setTimeout(() => {
        this._doRegisterServiceWorker(principal, scope, script, updateViaCache);
        this._timers[last].isCallbackExecuted = true;
      }, 15000);
    }
  },

  unregister(aManifestURL) {
    debug(`unregister ${aManifestURL}`);
    let appURI = Services.io.newURI(aManifestURL);
    let ssm = Services.scriptSecurityManager;
    let principal = ssm.createContentPrincipal(appURI, {});

    systemMessageService.unsubscribe(principal);
    Services.cpmm.sendAsyncMessage("Activities:UnregisterAll", aManifestURL);

    const unregisterCallback = {
      unregisterSucceeded() {},
      unregisterFailed() {
        debug(`Failed to unregister service worker for ${aManifestURL}`);
      },
      QueryInterface: ChromeUtils.generateQI([
        "nsIServiceWorkerUnregisterCallback",
      ]),
    };
    let scope = serviceWorkerManagegr.getScopeForUrl(principal, aManifestURL);
    debug(` getScopeForUrl scope: ${scope}`);
    serviceWorkerManagegr.propagateUnregister(
      principal,
      unregisterCallback,
      scope
    );
  },

  update(aManifestURL, aFeatures) {
    debug(`update ${aManifestURL}`);
    this.unregister(aManifestURL);
    this.register(aManifestURL, aFeatures);
  },

  waitForRegistrations() {
    // If we didn't have yet the opportunity to register service workers,
    // wait for the content process to be ready.
    if (!this._hasContentReady) {
      this._waitForRegistrations = true;
      return;
    }

    Promise.allSettled(this._pendingRegistrations).then(() => {
      debug(`waitForRegistration done.`);
      this._pendingRegistrations = [];
      // Note: if this is only used to delay system messages, maybe
      // we should use an explicit api instead.
      Services.obs.notifyObservers(null, "b2g-sw-registration-done");
    });
  },

  _doRegisterServiceWorker(aPrincipal, aScope, aScript, aUpdateViaCache) {
    debug(`_doRegisterServiceWorker: ${aScript}`);
    let promise = serviceWorkerManagegr
      .register(aPrincipal, aScope, aScript, aUpdateViaCache)
      .then(
        swRegInfo => {
          debug(`register ${aScript} success! ${JSON.stringify(swRegInfo)}`);
        },
        err => {
          debug(`register ${aScript} failed for error ${err}`);
        }
      );

    this._pendingRegistrations.push(promise);
  },

  _subscribeSystemMessages(aFeatures, aPrincipal, aScope) {
    debug(`Subscribing system messages...`);
    let messages = aFeatures.messages ? aFeatures.messages : [];
    messages.forEach(message => {
      debug(` message: ${message}`);
      systemMessageService.subscribe(aPrincipal, message, aScope);
    });
  },

  _registerActivities(aManifestURL, aFeatures, aPrincipal, aScope) {
    debug(`Registering as activity handlers ...`);
    let activitiesToRegister = [];
    activitiesToRegister.push.apply(
      activitiesToRegister,
      this._createActivitiesToRegister(aManifestURL, aFeatures)
    );
    if (activitiesToRegister.length) {
      debug(` subscribe activity`);
      systemMessageService.subscribe(aPrincipal, "activity", aScope);
      Services.cpmm.sendAsyncMessage(
        "Activities:Register",
        activitiesToRegister
      );
    }
  },

  _createActivitiesToRegister(aManifestURL, aFeatures) {
    let activitiesToRegister = [];
    let activities = aFeatures.activities;
    if (!activities) {
      return activitiesToRegister;
    }

    // TODO: icon is null because "icons" is not in "b2g_features"
    // Create Bug 105445 to track.
    let appURI = Services.io.newURI(aManifestURL);
    let iconURLForSize = function(aSize) {
      let icons = aFeatures.icons;
      if (!icons) {
        return null;
      }
      let dist = 100000;
      let icon = null;
      for (let size in icons) {
        let iSize = parseInt(size);
        if (Math.abs(iSize - aSize) < dist) {
          icon = appURI.resolve(icons[size]);
          dist = Math.abs(iSize - aSize);
        }
      }
      return icon;
    };

    for (let activity in activities) {
      let entry = activities[activity];
      if (!Array.isArray(entry)) {
        entry = [entry];
      }
      for (let i = 0; i < entry.length; i++) {
        let description = entry[i];
        debug(` create activity name: ${activity}`);
        activitiesToRegister.push({
          manifest: aManifestURL,
          name: activity,
          icon: iconURLForSize(128),
          description,
        });
      }
    }
    return activitiesToRegister;
  },
};

ServiceWorkerAssistant.init();
