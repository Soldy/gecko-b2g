/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

document.addEventListener(
  "DOMContentLoaded",
  () => {
    const startURL = "https://duckduckgo.com/";

    let browser = document.createElement("web-view");
    // Bug 109000, we must set the openWindowInfo.
    browser.openWindowInfo = null;
    browser.setAttribute("id", "browser");
    browser.setAttribute("src", startURL);
    //browser.setAttribute("remote", "true"); // do not work yet.

    console.log("append browser");
    document.body.append(browser);

    browser.go = function() {
      browser.src = document.getElementById("url").value;
    };

    // Binding Actions
    ["action-back", "action-forward", "action-go"].forEach(id => {
      let btn = document.getElementById(id);
      let action = btn.dataset.cmd;

      btn.addEventListener("click", () => {
        browser[action]();
      });
    });

    // Binding events
    browser.addEventListener("mozbrowserlocationchange", aEvent => {
      UpdateActionsUI(aEvent.detail);
    });

    // Init UI
    UpdateActionsUI({ canGoBack: false, canGoForward: false, url: startURL });

    function UpdateActionsUI({ canGoBack, canGoForward, url }) {
      document
        .getElementById("action-back")
        .toggleAttribute("disabled", !canGoBack);
      document
        .getElementById("action-forward")
        .toggleAttribute("disabled", !canGoForward);
      document.getElementById("url").value = url;
    }
  },
  { once: true }
);
