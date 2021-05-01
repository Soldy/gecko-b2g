/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["DownloadManager", "DownloadObject"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { DOMRequestIpcHelper } = ChromeUtils.import(
  "resource://gre/modules/DOMRequestHelper.jsm"
);
const { DownloadsIPC } = ChromeUtils.import(
  "resource://gre/modules/DownloadsIPC.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "volumeService",
  "@mozilla.org/telephony/volume-service;1",
  "nsIVolumeService"
);

/**
 * The content process implementations of navigator.b2g.downloadManager and its
 * DownloadObject download objects.  Uses DownloadsIPC.jsm to communicate with
 * DownloadManager.jsm in the parent process.
 */

const DEBUG = Services.prefs.getBoolPref("dom.downloads.debug", false);
function debug(aStr) {
  dump("-*- DownloadManager.js : " + aStr + "\n");
}

function DownloadManager() {
  DEBUG && debug("DownloadManager constructor");
}

DownloadManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  // nsIDOMGlobalPropertyInitializer implementation
  init(aWindow) {
    DEBUG && debug("DownloadsManager init");
    this.initDOMRequestHelper(aWindow, [
      "Downloads:Added",
      "Downloads:Removed",
    ]);
  },

  uninit() {
    DEBUG && debug("uninit");
    downloadsCache.evict(this._window);
  },

  set ondownloadstart(aHandler) {
    this.__DOM_IMPL__.setEventHandler("ondownloadstart", aHandler);
  },

  get ondownloadstart() {
    return this.__DOM_IMPL__.getEventHandler("ondownloadstart");
  },

  getDownloads() {
    DEBUG && debug("getDownloads");

    return this.createPromise(
      function(aResolve, aReject) {
        DownloadsIPC.getDownloads().then(
          function(aDownloads) {
            // Turn the list of download objects into DOM objects and
            // send them.
            let array = new this._window.Array();
            for (let id in aDownloads) {
              let dom = createDownloadObject(this._window, aDownloads[id]);
              array.push(this._prepareForContent(dom));
            }
            aResolve(array);
          }.bind(this),
          function() {
            aReject("GetDownloadsError");
          }
        );
      }.bind(this)
    );
  },

  clearAllDone() {
    DEBUG && debug("clearAllDone");
    // This is a void function; we just kick it off.  No promises, etc.
    DownloadsIPC.clearAllDone();
  },

  remove(aDownload) {
    DEBUG && debug("remove " + aDownload.url + " " + aDownload.id);
    return this.createPromise(
      function(aResolve, aReject) {
        if (!downloadsCache.has(this._window, aDownload.id)) {
          DEBUG && debug("no download " + aDownload.id);
          aReject("InvalidDownload");
          return;
        }

        DownloadsIPC.remove(aDownload.id).then(
          function(aResult) {
            let dom = createDownloadObject(this._window, aResult);
            // Change the state right away to not race against the update message.
            dom.wrappedJSObject.state = "finalized";
            aResolve(this._prepareForContent(dom));
          }.bind(this),
          function() {
            aReject("RemoveError");
          }
        );
      }.bind(this)
    );
  },

  adoptDownload(aAdoptDownloadDict) {
    // Our AdoptDownloadDict only includes simple types, which WebIDL enforces.
    // We have no object/any types so we do not need to worry about invoking
    // JSON.stringify (and it inheriting our security privileges).
    DEBUG && debug("adoptDownload");
    return this.createPromise(
      function(aResolve, aReject) {
        if (!aAdoptDownloadDict) {
          debug("DownloadObject dictionary is required!");
          aReject("InvalidDownload");
          return;
        }
        if (
          !aAdoptDownloadDict.storageName ||
          !aAdoptDownloadDict.storagePath ||
          !aAdoptDownloadDict.contentType
        ) {
          debug("Missing one of: storageName, storagePath, contentType");
          aReject("InvalidDownload");
          return;
        }

        // Convert storageName/storagePath to a local filesystem path.
        let volume;
        // getVolumeByName throws if you give it something it doesn't like
        // because XPConnect converts the NS_ERROR_NOT_AVAILABLE to an
        // exception.  So catch it.
        try {
          volume = volumeService.getVolumeByName(
            aAdoptDownloadDict.storageName
          );
        } catch (ex) {}
        if (!volume) {
          debug("Invalid storage name: " + aAdoptDownloadDict.storageName);
          aReject("InvalidDownload");
          return;
        }
        let computedPath =
          volume.mountPoint + "/" + aAdoptDownloadDict.storagePath;
        // We validate that there is actually a file at the given path in the
        // parent process in DownloadManager.js because that's where the file
        // access would actually occur either way.

        // Create a DownloadManager.jsm 'jsonDownload' style representation.
        let jsonDownload = {
          url: aAdoptDownloadDict.url,
          path: computedPath,
          contentType: aAdoptDownloadDict.contentType,
          startTime: aAdoptDownloadDict.startTime.valueOf() || Date.now(),
          sourceAppManifestURL: "",
        };

        DownloadsIPC.adoptDownload(jsonDownload).then(
          function(aResult) {
            let domDownload = createDownloadObject(this._window, aResult);
            aResolve(this._prepareForContent(domDownload));
          }.bind(this),
          function(aResult) {
            // This will be one of: AdoptError (generic catch-all),
            // AdoptNoSuchFile, AdoptFileIsDirectory
            aReject(aResult.error);
          }
        );
      }.bind(this)
    );
  },

  /**
   * Turns a chrome download object into a content accessible one.
   * When we have __DOM_IMPL__ available we just use that, otherwise
   * we run _create() with the wrapped js object.
   */
  _prepareForContent(aChromeObject) {
    if (aChromeObject.__DOM_IMPL__) {
      return aChromeObject.__DOM_IMPL__;
    }
    let res = this._window.DownloadObject._create(
      this._window,
      aChromeObject.wrappedJSObject
    );
    return res;
  },

  receiveMessage(aMessage) {
    let data = aMessage.data;
    switch (aMessage.name) {
      case "Downloads:Added":
        DEBUG && debug("Adding " + uneval(data));
        let event = new this._window.DownloadEvent("downloadstart", {
          download: this._prepareForContent(
            createDownloadObject(this._window, data)
          ),
        });
        this.__DOM_IMPL__.dispatchEvent(event);
        break;
    }
  },

  classDescription: "DownloadManager",
  classID: Components.ID("{6599ff0b-dfcc-4bf6-a2ea-434410cedcb5}"),
  contractID: "@mozilla.org/download/manager;1",
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsISupportsWeakReference,
    Ci.nsIObserver,
    Ci.nsIDOMGlobalPropertyInitializer,
  ]),
};

/**
 * Keep track of download objects per window.
 */
var downloadsCache = {
  init() {
    this.cache = new WeakMap();
  },

  has(aWindow, aId) {
    let downloads = this.cache.get(aWindow);
    return !!(downloads && downloads[aId]);
  },

  get(aWindow, aDownload) {
    let downloads = this.cache.get(aWindow);
    if (!(downloads && downloads[aDownload.id])) {
      DEBUG && debug("Adding download " + aDownload.id + " to cache.");
      if (!downloads) {
        this.cache.set(aWindow, {});
        downloads = this.cache.get(aWindow);
      }
      // Create the object and add it to the cache.
      let impl = Cc["@mozilla.org/download/object;1"].createInstance(
        Ci.nsISupports
      );
      impl.wrappedJSObject._init(aWindow, aDownload);
      downloads[aDownload.id] = impl;
    }
    return downloads[aDownload.id];
  },

  evict(aWindow) {
    this.cache.delete(aWindow);
  },
};

downloadsCache.init();

/**
 * The DOM facade of a download object.
 */

function createDownloadObject(aWindow, aDownload) {
  return downloadsCache.get(aWindow, aDownload);
}

function DownloadObject() {
  DEBUG && debug("DownloadObject constructor ");

  this.wrappedJSObject = this;
  this.totalBytes = 0;
  this.currentBytes = 0;
  this.url = null;
  this.path = null;
  this.storageName = null;
  this.storagePath = null;
  this.contentType = null;

  /* fields that require getters/setters */
  this._error = null;
  this._startTime = new Date();
  this._state = "stopped";

  /* private fields */
  this.id = null;
}

DownloadObject.prototype = {
  createPromise(aPromiseInit) {
    return new this._window.Promise(aPromiseInit);
  },

  pause() {
    DEBUG && debug("DownloadObject pause " + this.id);
    let id = this.id;
    // We need to wrap the Promise.jsm promise in a "real" DOM promise...
    return this.createPromise(function(aResolve, aReject) {
      DownloadsIPC.pause(id).then(aResolve, aReject);
    });
  },

  resume() {
    DEBUG && debug("DownloadObject resume " + this.id);
    let id = this.id;
    // We need to wrap the Promise.jsm promise in a "real" DOM promise...
    return this.createPromise(function(aResolve, aReject) {
      DownloadsIPC.resume(id).then(aResolve, aReject);
    });
  },

  set onstatechange(aHandler) {
    this.__DOM_IMPL__.setEventHandler("onstatechange", aHandler);
  },

  get onstatechange() {
    return this.__DOM_IMPL__.getEventHandler("onstatechange");
  },

  get error() {
    return this._error;
  },

  set error(aError) {
    this._error = aError;
  },

  get startTime() {
    return this._startTime;
  },

  set startTime(aStartTime) {
    if (aStartTime instanceof Date) {
      this._startTime = aStartTime;
    } else {
      this._startTime = new Date(aStartTime);
    }
  },

  get state() {
    return this._state;
  },

  // We require a setter here to simplify the internals of the Download Manager
  // since we actually pass dummy JSON objects to the child process and update
  // them. This is the case for all other setters for read-only attributes
  // implemented in this object.
  set state(aState) {
    // We need to ensure that XPCOM consumers of this API respect the enum
    // values as well.
    if (["downloading", "stopped", "succeeded", "finalized"].includes(aState)) {
      this._state = aState;
    }
  },

  /**
   * Initialize a DownloadObject instance for the given window using the
   * 'jsonDownload' serialized format of the download encoded by
   * DownloadManager.jsm.
   */
  _init(aWindow, aDownload) {
    this._window = aWindow;
    this.id = aDownload.id;
    this._update(aDownload);
    Services.obs.addObserver(
      this,
      "downloads-state-change-" + this.id,
      /* ownsWeak */ true
    );
    DEBUG && debug("observer set for " + this.id);
  },

  /**
   * Updates the state of the object and fires the statechange event.
   */
  _update(aDownload) {
    DEBUG && debug("update " + uneval(aDownload));
    if (this.id != aDownload.id) {
      return;
    }

    let props = [
      "totalBytes",
      "currentBytes",
      "url",
      "path",
      "storageName",
      "storagePath",
      "state",
      "contentType",
      "startTime",
      "sourceAppManifestURL",
    ];
    let changed = false;
    let changedProps = {};

    props.forEach(prop => {
      if (prop in aDownload && aDownload[prop] != this[prop]) {
        this[prop] = aDownload[prop];
        changedProps[prop] = changed = true;
      }
    });

    // When the path changes, we should update the storage name and
    // storage path used for our downloaded file in case our download
    // was re-targetted to a different storage and/or filename.
    if (changedProps.path) {
      let storages = this._window.navigator.b2g.getDeviceStorages("sdcard");
      let preferredStorageName;
      // Use the first one or the default storage. Just like jsdownloads picks
      // the default / preferred download directory.
      storages.forEach(aStorage => {
        if (aStorage.default || !preferredStorageName) {
          preferredStorageName = aStorage.storageName;
        }
      });
      // Now get the path for this storage area.
      if (preferredStorageName) {
        let volume = volumeService.getVolumeByName(preferredStorageName);
        if (volume) {
          // Finally, create the relative path of the file that can be used
          // later on to retrieve the file via DeviceStorage. Our path
          // needs to omit the starting '/'.
          this.storageName = preferredStorageName;
          this.storagePath = this.path.substring(
            this.path.indexOf(volume.mountPoint) + volume.mountPoint.length + 1
          );
        }
      }
    }

    if (aDownload.error) {
      //
      // When we get a generic error failure back from the js downloads api
      // we will verify the status of device storage to see if we can't provide
      // a better error result value.
      //
      // XXX If these checks expand further, consider moving them into their
      // own function.
      //
      let result = aDownload.error.result;
      let storage = this._window.navigator.b2g.getDeviceStorage("sdcard");

      // If we don't have access to device storage we'll opt out of these
      // extra checks as they are all dependent on the state of the storage.
      if (result == Cr.NS_ERROR_FAILURE && storage) {
        // We will delay sending the notification until we've inferred which
        // error is really happening.
        changed = false;
        DEBUG &&
          debug("Attempting to infer error via device storage sanity checks.");
        // Get device storage and request availability status.
        let available = storage.available();
        available.onsuccess = function() {
          DEBUG && debug("Storage Status = '" + available.result + "'");
          let inferredError = result;
          switch (available.result) {
            case "unavailable":
              inferredError = Cr.NS_ERROR_FILE_NOT_FOUND;
              break;
            case "shared":
              inferredError = Cr.NS_ERROR_FILE_ACCESS_DENIED;
              break;
          }
          this._updateWithError(aDownload, inferredError);
        }.bind(this);
        available.onerror = function() {
          this._updateWithError(aDownload, result);
        }.bind(this);
      }

      // DOMException cannot reflect all possible download nsresult error codes,
      // so we store the error code within its [message] attribute.
      this.error = new this._window.DOMException(
        aDownload.error.result,
        "NS_ERROR_UNEXPECTED"
      );
    } else {
      this.error = null;
    }

    // The visible state has not changed, so no need to fire an event.
    if (!changed) {
      return;
    }

    this._sendStateChange();
  },

  _updateWithError(aDownload, aError) {
    this.error = new this._window.DOMException(aError, "NS_ERROR_UNEXPECTED");
    this._sendStateChange();
  },

  _sendStateChange() {
    // __DOM_IMPL__ may not be available at first update.
    if (this.__DOM_IMPL__) {
      let event = new this._window.DownloadEvent("statechange", {
        download: this.__DOM_IMPL__,
      });
      DEBUG && debug("Dispatching statechange event. state=" + this.state);
      this.__DOM_IMPL__.dispatchEvent(event);
    }
  },

  observe(aSubject, aTopic, aData) {
    DEBUG && debug("DownloadObject observe " + aTopic);
    if (aTopic !== "downloads-state-change-" + this.id) {
      return;
    }

    try {
      let download = JSON.parse(aData);
      // We get the start time as milliseconds, not as a Date object.
      if (download.startTime) {
        download.startTime = new Date(download.startTime);
      }
      this._update(download);
    } catch (e) {}
  },

  classDescription: "DownloadObject",
  classID: Components.ID("{ee82e19f-8ead-406f-ad91-194e77a61549}"),
  contractID: "@mozilla.org/download/object;1",
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ]),
};
