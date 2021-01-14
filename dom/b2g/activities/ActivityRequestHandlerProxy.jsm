/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function debug(aMsg) {
  dump(`ActivityRequestHandlerProxy: ${aMsg}\n`);
}

/*
 * ActivityRequestHandlerProxy is a helper of passing Activity result, from
 * WebActivityRequestHandler (running on service worker) to ActivitiesService.
 */

function ActivityRequestHandlerProxy() {}

ActivityRequestHandlerProxy.prototype = {
  notifyActivityReady(id) {
    debug(`${id} notifyActivityReady`);
    Services.cpmm.sendAsyncMessage("Activity:Ready", { id });
  },

  postActivityResult(id, result) {
    debug(`${id} postActivityResult`);
    Services.cpmm.sendAsyncMessage("Activity:PostResult", {
      id,
      result,
    });
  },

  postActivityError(id, error) {
    debug(`${id} postActivityError`);
    Services.cpmm.sendAsyncMessage("Activity:PostError", {
      id,
      error,
    });
  },

  contractID: "@mozilla.org/dom/activities/handlerproxy;1",

  classID: Components.ID("{47f2248f-2c8e-4829-a4bb-6ec1e886b2d6}"),

  QueryInterface: ChromeUtils.generateQI([Ci.nsIActivityRequestHandlerProxy]),
};

//module initialization
var EXPORTED_SYMBOLS = ["ActivityRequestHandlerProxy"];
