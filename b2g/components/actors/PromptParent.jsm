/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PromptParent"];

class PromptParent extends JSWindowActorParent {
  constructor() {
    super();
    this._promise = undefined;
  }

  log(msg) {
    console.log("PromptParent.jsm " + msg);
  }

  receiveMessage(message) {
    let args = message.data;
    let browser = this.browsingContext.top.embedderElement;
    let window = browser.ownerGlobal;

    // If the page which called the prompt is different from the the top context
    // where we show the prompt, ask the prompt implementation to display the origin.
    // For example, this can happen if a cross origin subframe shows a prompt.
    args.showCallerOrigin =
      args.promptPrincipal &&
      !browser.contentPrincipal.equals(args.promptPrincipal);

    switch (message.name) {
      case "Prompt:Open": {
        let evt = new window.CustomEvent("showmodalprompt", { detail: args });
        let promise = new Promise(resolve => {
          function sendUnblockMsg() {
            evt.detail.value =
              evt.detail.returnValue === undefined
                ? null
                : evt.detail.returnValue;
            evt.detail.ok =
              evt.detail.promptType === "confirm" ? evt.detail.value : true;
            let ret = JSON.parse(JSON.stringify(evt.detail));
            this.log(
              `sendUnblockMsg evt.detail: ${JSON.stringify(evt.detail)}`
            );
            resolve(ret);
          }
          Cu.exportFunction(sendUnblockMsg.bind(this), evt.detail, {
            defineAs: "unblock",
          });
          browser.dispatchEvent(evt);
        });
        this._promise = promise;
        break;
      }
    }

    return this._promise;
  }
}
