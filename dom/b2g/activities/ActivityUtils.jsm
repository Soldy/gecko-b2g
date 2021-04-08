/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function debug(aMsg) {
  dump(`-*- ActivityUtils: ${aMsg}\n`);
}

this.EXPORTED_SYMBOLS = ["ActivityUtils"];

function ActivityUtils() {
  debug("constructor");
}

ActivityUtils.prototype = {
  // nsIDOMGlobalPropertyInitializer implementation
  init(aWindow) {
    debug("init");
    this._callbacks = {};
    this._window = aWindow;
    this._requestCount = 0;
  },

  getInstalled(aName) {
    debug(`getInstalled ${aName}`);
    let requestID = this.generateRequestID();
    let promise = new this._window.Promise(
      function(aResolve, aReject) {
        this._callbacks[requestID] = { resolve: aResolve, reject: aReject };
      }.bind(this)
    );

    let self = this;
    Services.cpmm.addMessageListener(
      `Activities:Get:${requestID}`,
      function receiveActivitiesGet(aMessage) {
        debug(
          `receive Activities:Get:${requestID} ${JSON.stringify(aMessage)}`
        );
        Services.cpmm.removeMessageListener(requestID, receiveActivitiesGet);
        let json = aMessage.json ? aMessage.json : {};
        if (json.success) {
          let array = new self._window.Array();
          let options = json?.results?.options ? json.results.options : [];
          options.forEach(function(aObject) {
            array.push(Cu.cloneInto(aObject, self._window));
          });
          self._callbacks[requestID].resolve(array);
        } else {
          self._callbacks[requestID].reject(json.error);
        }
        delete self._callbacks[requestID];
      }
    );

    Services.cpmm.sendAsyncMessage("Activities:Get", {
      requestID,
      name: aName,
    });

    return promise;
  },

  generateRequestID() {
    return this._requestCount++;
  },

  classID: Components.ID("{b857ca8c-616b-4121-ae92-464c356e6a80}"),
  contractID: "@mozilla.org/activity-utils;1",
  QueryInterface: ChromeUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),
};
