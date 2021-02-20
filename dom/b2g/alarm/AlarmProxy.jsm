/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["AlarmProxy"];

const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const REQUEST_CPU_LOCK_TIMEOUT = 10 * 1000; // 10 seconds.
if (Services.prefs.getBoolPref("dom.alarm.debug", false)) {
  let logger = Log.repository.getLogger("AlarmProxy");
  logger.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));
  logger.level = Log.Level.Debug;

  this.debug = function debug(aStr) {
    logger.debug(aStr);
  };
} else {
  this.debug = function debug(aStr) {};
}

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gPowerManagerService",
  "@mozilla.org/power/powermanagerservice;1",
  "nsIPowerManagerService"
);

/*
 * AlarmProxy is a helper of passing requests from AlarmManager.cpp to
 * AlarmService.jsm, receiving results from AlarmService.jsm and send it
 * back to AlarmManager.cpp.
 */

function AlarmProxy() {
  this.uuid = Cc["@mozilla.org/uuid-generator;1"]
    .getService(Ci.nsIUUIDGenerator)
    .generateUUID()
    .toString();
  debug("AlarmProxy constructor. uuid:" + this.uuid);
}

AlarmProxy.prototype = {
  add: function add(aUrl, aOptions, aCallback) {
    debug("add " + JSON.stringify(aOptions) + " " + aUrl);
    let date = aOptions.date;
    let ignoreTimezone = !!aOptions.ignoreTimezone;
    let data = aOptions.data;

    if (!date) {
      throw Components.Exception("", Cr.NS_ERROR_INVALID_ARG);
    }

    if (data && aUrl) {
      // Run eval() in the sand box with the principal of the calling
      // web page to ensure no cross-origin object is involved. A "Permission
      // Denied" error will be thrown in case of privilege violation.
      // We used JSON.stringify in the past, but the toJSON privileged function is denied now.
      try {
        let sandbox = new Cu.Sandbox(aUrl);
        sandbox.data = data;
        data = Cu.evalInSandbox("eval(data)", sandbox);
      } catch (e) {
        debug(e);
        aCallback.onAdd(
          Cr.NS_ERROR_INVALID_ARG,
          Cu.cloneInto("data examination in sandbox failed", {})
        );
        return;
      }
    }

    let requestId = `Alarm:Add:${this.uuid}`;
    debug("requestId: " + requestId);

    let cpuLock = gPowerManagerService.newWakeLock("cpu");
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

    // Start a timer to prevent from non-responding request.
    timer.initWithCallback(
      () => {
        debug("Request timeout! Release the cpu lock");
        cpuLock.unlock();
      },
      REQUEST_CPU_LOCK_TIMEOUT,
      Ci.nsITimer.TYPE_ONE_SHOT
    );

    Services.cpmm.addMessageListener(requestId, function alarmAdd(aMessage) {
      debug("Receive " + JSON.stringify(aMessage));
      Services.cpmm.removeMessageListener(requestId, alarmAdd);
      timer.cancel();
      cpuLock.unlock();

      let json = aMessage.json ? aMessage.json : {};
      if (json.success) {
        aCallback.onAdd(Cr.NS_OK, Cu.cloneInto(json.id, {}));
      } else {
        aCallback.onAdd(Cr.NS_ERROR_FAILURE, Cu.cloneInto(json.errorMsg, {}));
      }
    });

    Services.cpmm.sendAsyncMessage("Alarm:Add", {
      requestId,
      date,
      ignoreTimezone,
      data,
      pageURL: aUrl,
      manifestURL: aUrl,
    });
  },

  remove: function remove(aUrl, aId) {
    debug("remove " + aId + " " + aUrl);
    Services.cpmm.sendAsyncMessage("Alarm:Remove", {
      id: aId,
      manifestURL: aUrl,
    });
  },

  getAll: function getAll(aUrl, aCallback) {
    debug("getAll " + aUrl);

    let requestId = `Alarm:GetAll:${this.uuid}`;
    debug("requestId: " + requestId);

    Services.cpmm.addMessageListener(requestId, function alarmGetAll(aMessage) {
      Services.cpmm.removeMessageListener(requestId, alarmGetAll);

      let json = aMessage.json ? aMessage.json : {};
      if (json.success) {
        let alarms = [];
        json.alarms.forEach(function trimAlarmInfo(aAlarm) {
          let alarm = {
            id: aAlarm.id,
            date: aAlarm.date,
            ignoreTimezone: aAlarm.ignoreTimezone,
            data: aAlarm.data,
          };
          alarms.push(alarm);
        });

        aCallback.onGetAll(Cr.NS_OK, Cu.cloneInto(alarms, {}));
      } else {
        aCallback.onGetAll(
          Cr.NS_ERROR_FAILURE,
          Cu.cloneInto(json.errorMsg, {})
        );
      }
    });

    Services.cpmm.sendAsyncMessage("Alarm:GetAll", {
      requestId,
      manifestURL: aUrl,
    });
  },

  contractID: "@mozilla.org/alarm/proxy;1",

  classID: Components.ID("{736b46dc-9bea-4f75-a567-0bec41980f0c}"),

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIAlarmProxy,
    Ci.nsISupportsWeakReference,
  ]),
};
