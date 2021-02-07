/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

document.addEventListener("DOMContentLoaded", () => {
  // Bug 109000, we must set the openWindowInfo.
  document.getElementById("keyboard").openWindowInfo = null;
  window.addEventListener("inputmethod-contextchange", event => {
    let detail = event.detail;
    console.log(`Event 'inputmethod-contextchange' ${JSON.stringify(detail)}`);
    if (detail.isFocus === true) {
      document.getElementById("keyboard").classList.remove("offscreen");
    }
    if (detail.isFocus === false) {
      document.getElementById("keyboard").classList.add("offscreen");
    }
  });
});
