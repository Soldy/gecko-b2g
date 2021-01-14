/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { IndexedDBHelper } = ChromeUtils.import(
  "resource://gre/modules/IndexedDBHelper.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "ActivitiesServiceFilter",
  "resource://gre/modules/ActivitiesServiceFilter.jsm"
);

this.EXPORTED_SYMBOLS = [];

function debug(aMsg) {
  dump(`ActivitiesService: ${aMsg}\n`);
}

const DB_NAME = "activities";
const DB_VERSION = 1;
const STORE_NAME = "activities";

function ActivitiesDb() {}

ActivitiesDb.prototype = {
  __proto__: IndexedDBHelper.prototype,

  init: function actdb_init() {
    this.initDBHelper(DB_NAME, DB_VERSION, [STORE_NAME]);
  },

  /**
   * Create the initial database schema.
   *
   * The schema of records stored is as follows:
   *
   * {
   *  id:                  String
   *  manifest:            String
   *  name:                String
   *  icon:                String
   *  description:         jsval
   * }
   */
  upgradeSchema: function actdb_upgradeSchema(
    aTransaction,
    aDb,
    aOldVersion,
    aNewVersion
  ) {
    debug("Upgrade schema " + aOldVersion + " -> " + aNewVersion);

    let self = this;

    /**
     * WARNING!! Before upgrading the Activities DB take into account that an
     * OTA unregisters all the activities and reinstalls them during the first
     * run process. Check Bug 1193503.
     */

    function upgrade(currentVersion) {
      let next = upgrade.bind(self, currentVersion + 1);
      switch (currentVersion) {
        case 0:
          self.createSchema(aDb, next);
          break;
      }
    }

    upgrade(aOldVersion);
  },

  createSchema(aDb, aNext) {
    let objectStore = aDb.createObjectStore(STORE_NAME, { keyPath: "id" });

    // indexes
    objectStore.createIndex("name", "name", { unique: false });
    objectStore.createIndex("manifest", "manifest", { unique: false });

    debug("Created object stores and indexes");

    aNext();
  },

  // unique ids made of (uri, action)
  createId: function actdb_createId(aObject) {
    let converter = Cc[
      "@mozilla.org/intl/scriptableunicodeconverter"
    ].createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    let hasher = Cc["@mozilla.org/security/hash;1"].createInstance(
      Ci.nsICryptoHash
    );
    hasher.init(hasher.SHA1);

    // add uri and action to the hash
    ["manifest", "name", "description"].forEach(function(aProp) {
      if (!aObject[aProp]) {
        return;
      }

      let property = aObject[aProp];
      if (aProp == "description") {
        property = JSON.stringify(aObject[aProp]);
      }

      let data = converter.convertToByteArray(property, {});
      hasher.update(data, data.length);
    });

    return hasher.finish(true);
  },

  // Add all the activities carried in the |aObjects| array.
  add: function actdb_add(aObjects, aSuccess, aError) {
    this.newTxn(
      "readwrite",
      STORE_NAME,
      function(txn, store) {
        debug("Object count: " + aObjects.length);
        aObjects.forEach(function(aObject) {
          let object = {
            manifest: aObject.manifest,
            name: aObject.name,
            icon: aObject.icon || "",
            description: aObject.description,
          };
          object.id = this.createId(object);
          debug("Going to add " + JSON.stringify(object));
          store.put(object);
        }, this);
      }.bind(this),
      aSuccess,
      aError
    );
  },

  // Remove all the activities carried in the |aObjects| array.
  remove: function actdb_remove(aObjects) {
    this.newTxn(
      "readwrite",
      STORE_NAME,
      (txn, store) => {
        aObjects.forEach(aObject => {
          let object = {
            manifest: aObject.manifest,
            name: aObject.name,
            description: aObject.description,
          };
          debug("Going to remove " + JSON.stringify(object));
          store.delete(this.createId(object));
        });
      },
      function() {},
      function() {}
    );
  },

  // Remove all activities associated with the given |aManifest| URL.
  removeAll: function actdb_removeAll(aManifest) {
    this.newTxn("readwrite", STORE_NAME, function(txn, store) {
      let index = store.index("manifest");
      let request = index.mozGetAll(aManifest);
      request.onsuccess = function manifestActivities(aEvent) {
        aEvent.target.result.forEach(function(result) {
          debug("Removing activity: " + JSON.stringify(result));
          store.delete(result.id);
        });
      };
    });
  },

  find: function actdb_find(aObject, aSuccess, aError, aMatch) {
    debug("Looking for " + aObject.options.name);

    this.newTxn(
      "readonly",
      STORE_NAME,
      function(txn, store) {
        let index = store.index("name");
        let request = index.mozGetAll(aObject.options.name);
        request.onsuccess = function findSuccess(aEvent) {
          debug(
            "Request successful. Record count: " + aEvent.target.result.length
          );
          if (!txn.result) {
            txn.result = {
              name: aObject.options.name,
              options: [],
            };
          }

          aEvent.target.result.forEach(function(result) {
            if (!aMatch(result)) {
              return;
            }

            txn.result.options.push({
              manifest: result.manifest,
              icon: result.icon,
              description: result.description,
            });
          });
        };
      },
      aSuccess,
      aError
    );
  },
};

var Activities = {
  messages: [
    // ActivityProxy.jsm
    "Activity:Start",
    "Activity:Cancel",

    // ActivityRequestHandlerProxy.jsm
    "Activity:Ready",
    "Activity:PostResult",
    "Activity:PostError",

    // ServiceWorkerAssistant.jsm
    "Activities:Register",
    "Activities:Unregister",
    "Activities:UnregisterAll",

    // Not in used for now.
    "Activities:Get",

    "child-process-shutdown",
  ],

  init: function activities_init() {
    this.messages.forEach(function(msgName) {
      Services.ppmm.addMessageListener(msgName, this);
    }, this);

    Services.obs.addObserver(this, "xpcom-shutdown");
    Services.obs.addObserver(this, "service-worker-shutdown");

    this.db = new ActivitiesDb();
    this.db.init();
    this.callers = {};
    this.activityChoiceID = 0;
  },

  observe: function activities_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "xpcom-shutdown":
        this.messages.forEach(function(msgName) {
          Services.ppmm.removeMessageListener(msgName, this);
        }, this);

        if (this.db) {
          this.db.close();
          this.db = null;
        }

        Services.obs.removeObserver(this, "xpcom-shutdown");
        Services.obs.removeObserver(this, "service-worker-shutdown");
        break;
      case "service-worker-shutdown":
        let origin = Services.io.newURI(aData).prePath;
        debug("ServiceWorker service-worker-shutdown origin=" + origin);
        let messages = [];
        for (const [key, value] of Object.entries(this.callers)) {
          if (value.handlerOrigin == origin) {
            debug(
              "ServiceWorker found " + key + " handler=" + value.handlerOrigin
            );
            messages.push(key);
          }
        }
        let self = this;
        messages.forEach(function(id) {
          self.trySendAndCleanup(id, "Activity:FireError", {
            id,
            error: "SERVICE_WORKER_SHUTDOWN",
          });
        });
        break;
    }
  },

  /**
   * Starts an activity by doing:
   * - finds a list of matching activities.
   * - sends the list to web-embedder to get activity choice from user
   * - fire an system message of type "activity" to this app, sending the
   *   activity data as a payload.
   */
  startActivity: function activities_startActivity(aMsg) {
    debug("StartActivity: " + JSON.stringify(aMsg));
    let self = this;
    let successCb = function successCb(aResults) {
      debug(JSON.stringify(aResults));

      // We have no matching activity registered, let's fire an error.
      if (aResults.options.length === 0) {
        self.trySendAndCleanup(aMsg.id, "Activity:FireError", {
          id: aMsg.id,
          error: "NO_PROVIDER",
        });
        return;
      }

      let getActivityChoice = function(aResult) {
        debug("Activity choice: " + aResult);

        // The user has cancelled the choice, fire an error.
        if (aResult === -1) {
          self.trySendAndCleanup(aMsg.id, "Activity:FireError", {
            id: aMsg.id,
            error: "ACTIVITY_CHOICE_CANCELED",
          });
          return;
        }

        let sysmm = Cc["@mozilla.org/systemmessage-service;1"].getService(
          Ci.nsISystemMessageService
        );
        if (!sysmm) {
          self.removeCaller(aMsg.id);
          debug("Error: SystemMessageService is not present.");
          return;
        }

        let result = aResults.options[aResult];
        let origin = Services.io.newURI(result.manifest).prePath;
        debug("Sending system message to " + origin);
        self.callers[aMsg.id].handlerOrigin = origin;

        try {
          sysmm.sendMessage(
            "activity",
            {
              id: aMsg.id,
              payload: aMsg.options,
              returnValue: result.description.returnValue,
            },
            origin
          );
        } catch (e) {
          debug("Error when sending system message: " + e);
        }

        if (!result.description.returnValue) {
          // No need to notify observers, since we don't want the caller
          // to be raised on the foreground that quick.
          self.trySendAndCleanup(aMsg.id, "Activity:FireSuccess", {
            id: aMsg.id,
            result: null,
          });
        }
      };

      let id = "activity-choice" + self.activityChoiceID++;
      let choices = [];
      aResults.options.forEach(function(item) {
        choices.push({ manifest: item.manifest, icon: item.icon });
      });
      let detail = {
        id,
        name: aMsg.options.name,
        choices,
      };
      if (aMsg.options.data && aMsg.options.data.type) {
        detail.activityType = aMsg.options.data.type;
      }

      Services.obs.addObserver(function choiceObsrver(wrappedResult) {
        let result = wrappedResult.wrappedJSObject;
        if (result.id != id) {
          return;
        }

        Services.obs.removeObserver(choiceObsrver, "activity-choice-result");
        getActivityChoice(result.value !== undefined ? result.value : -1);
      }, "activity-choice-result");

      Services.obs.notifyObservers(
        { wrappedJSObject: detail },
        "activity-choice"
      );
    };

    let errorCb = function errorCb(aError) {
      // Something unexpected happened. Should we send an error back?
      debug("Error in startActivity: " + aError + "\n");
    };

    let matchFunc = function matchFunc(aResult) {
      return ActivitiesServiceFilter.match(
        aMsg.options.data,
        aMsg.origin,
        aResult.description
      );
    };

    this.db.find(aMsg, successCb, errorCb, matchFunc);
  },

  trySendAndCleanup: function activities_trySendAndCleanup(
    aId,
    aName,
    aPayload
  ) {
    try {
      this.callers[aId].mm.sendAsyncMessage(aName, aPayload);
    } finally {
      this.removeCaller(aId);
    }
  },

  receiveMessage: function activities_receiveMessage(aMessage) {
    let mm = aMessage.target;
    let msg = aMessage.json;

    let caller;

    if (
      aMessage.name == "Activity:PostResult" ||
      aMessage.name == "Activity:PostError" ||
      aMessage.name == "Activity:Cancel" ||
      aMessage.name == "Activity:Ready"
    ) {
      caller = this.callers[msg.id];
      if (!caller) {
        debug("!! caller is null for msg.id=" + msg.id);
        return;
      }
    }

    switch (aMessage.name) {
      case "Activity:Start":
        // TODO: For ProcessPriorityManager to manage. (Bug 80942)
        // Not in used for now.
        Services.obs.notifyObservers(null, "activity-opened", msg.childID);
        this.callers[msg.id] = {
          mm,
          childID: msg.childID,
          origin: msg.origin,
        };
        this.startActivity(msg);
        break;

      case "Activity:Cancel":
        this.trySendAndCleanup(msg.id, "Activity:FireCancel", msg);
        break;

      case "Activity:Ready":
        caller.handlerMM = mm;
        break;
      case "Activity:PostResult":
        this.trySendAndCleanup(msg.id, "Activity:FireSuccess", msg);
        break;
      case "Activity:PostError":
        this.trySendAndCleanup(msg.id, "Activity:FireError", msg);
        break;

      case "Activities:Register":
        this.db.add(
          msg,
          function onSuccess(aEvent) {
            debug("Activities:Register:OK");
            Services.obs.notifyObservers(
              null,
              "new-activity-registered-success"
            );
            mm.sendAsyncMessage("Activities:Register:OK", null);
          },
          function onError(aEvent) {
            msg.error = "REGISTER_ERROR";
            debug("Activities:Register:KO");
            Services.obs.notifyObservers(
              null,
              "new-activity-registered-failure"
            );
            mm.sendAsyncMessage("Activities:Register:KO", msg);
          }
        );
        break;
      case "Activities:Unregister":
        this.db.remove(msg);
        break;
      case "Activities:UnregisterAll":
        this.db.removeAll(msg);
        break;
      case "child-process-shutdown":
        for (let id in this.callers) {
          if (this.callers[id].handlerMM == mm) {
            debug(
              "child-process-shutdown caller=" +
                this.callers[id].pageURL +
                " handler=" +
                this.callers[id].handlerOrigin
            );
            this.trySendAndCleanup(id, "Activity:FireError", {
              id,
              error: "PROCESS_SHUTDOWN",
            });
            break;
          }
        }
        break;
      case "Activities:Get":
        debug("Activities:Get");
        let obj = {
          options: {
            name: msg.activityName,
          },
        };
        this.db.find(
          obj,
          function onSuccess(aResults) {
            mm.sendAsyncMessage("Activities:Get:OK", {
              results: aResults,
              oid: msg.oid,
              requestID: msg.requestID,
            });
          },
          function onError(aEvent) {
            mm.sendAsyncMessage("Activities:Get:KO", {
              oid: msg.oid,
              requestID: msg.requestID,
            });
          },
          function matchFunc(aResult) {
            return ActivitiesServiceFilter.match(
              obj.options.data,
              aResult.description.filters
            );
          }
        );
        break;
    }
  },

  removeCaller: function activities_removeCaller(id) {
    // TODO: For ProcessPriorityManager to manage. (Bug 80942)
    // Not in used for now.
    Services.obs.notifyObservers(
      null,
      "activity-closed",
      this.callers[id].childID
    );
    debug("removeCaller " + id + " handler=" + this.callers[id].handlerOrigin);
    delete this.callers[id];
  },
};

Activities.init();
