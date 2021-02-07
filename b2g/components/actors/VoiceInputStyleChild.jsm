/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["VoiceInputStyleChild"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const kPrefVoiceInputEnabled = "voice-input.enabled";
const kPrefVoiceInputIconUrl = "voice-input.icon-url";

function prefObserver(subject, topic, data) {
  if (topic !== "nsPref:changed") {
    return;
  }

  if (data === kPrefVoiceInputEnabled) {
    this.enabled = Services.prefs.getBoolPref(
      kPrefVoiceInputEnabled,
      this.enabled
    );
  } else if (data === kPrefVoiceInputIconUrl) {
    this.iconUrl = Services.prefs.getCharPref(kPrefVoiceInputIconUrl, "");
  }

  // On switching from enabled to disabled, clear the existing voice input style.
  if (
    (data === kPrefVoiceInputEnabled && !this.enabled) ||
    (data === kPrefVoiceInputIconUrl && !this.iconUrl)
  ) {
    let element = Services.focus.focusedElement;
    if (element && element.voiceInputStyleApplied) {
      this.log(
        `remove voice input style due to preference change. ${data} ${this.enabled} ${this.iconUrl}`
      );
      this.removeVoiceInputStyle(element);
    }
  }
}

class VoiceInputStyleChild extends JSWindowActorChild {
  constructor() {
    super();
    this.enabled = Services.prefs.getBoolPref(kPrefVoiceInputEnabled, false);
    this.iconUrl = Services.prefs.getCharPref(kPrefVoiceInputIconUrl, "");
    this.originalStyle = {};

    Services.prefs.addObserver(kPrefVoiceInputEnabled, prefObserver.bind(this));
    Services.prefs.addObserver(kPrefVoiceInputIconUrl, prefObserver.bind(this));
  }

  log(msg) {
    console.log("VoiceInputStyleChild.jsm " + msg);
  }

  handleEvent(event) {
    this.log(`handleEvent ${event.type}`);
    if (!this.enabled || !this.iconUrl) {
      return;
    }
    switch (event.type) {
      case "IMEFocus": {
        this.onFocus(event.target);
        break;
      }
      case "IMEBlur": {
        this.onBlur(event.target);
        break;
      }
      case "input": {
        let element = event.target;
        if (this.hasText(element) === undefined) {
          this.log("element does not have value nor textContent property.");
        } else if (this.hasText(element)) {
          this.removeVoiceInputStyle(element);
        } else {
          this.applyVoiceInputStyle(element);
        }
        break;
      }
      default: {
        this.log("catch unrecognized event " + event.type);
        break;
      }
    }
  }

  onFocus(element) {
    if (element === undefined || !element) {
      this.log(`!element ${element}`);
      return;
    }

    if (!element.getAttribute) {
      this.log(`!element.getAttribute ${element}`);
      return;
    }

    if ("true" !== element.getAttribute("voice-input-supported")) {
      this.log(
        `voice-input-supported attr is not true. ${element.getAttribute(
          "voice-input-supported"
        )}`
      );
      return;
    }

    element.addEventListener("input", this);

    // Only apply icon if input field is empty.
    if (this.hasText(element) === undefined) {
      this.log("element does not have value nor textContent property.");
    } else if (!this.hasText(element)) {
      this.applyVoiceInputStyle(element);
    }
  }

  onBlur(element) {
    element.removeEventListener("input", this);
    this.removeVoiceInputStyle(element);
  }

  /**
   * _hasText will return either true, false or undefined.
   * element is expected to be a contentEditable <div> or <input> <textarea>,
   * return undefined for unexpected case.
   */
  hasText(element) {
    let text = undefined;
    if (element.isContentEditable && element.textContent !== undefined) {
      text = element.textContent;
      this.log("element.textContent: " + text);
    } else if (element.value !== undefined) {
      text = element.value;
      this.log("element.value: " + text);
    }

    return text === undefined ? undefined : text.length;
  }

  applyVoiceInputStyle(element) {
    if (element.voiceInputStyleApplied) {
      this.log("Voice input style is already applied.");
      return;
    }

    if (Object.keys(this.originalStyle).length) {
      this.log(
        "Warning! Style backup is not empty, and is going to be overwritten!"
      );
    }

    this.originalStyle.backgroundImage = element.style.backgroundImage;
    this.originalStyle.backgroundRepeat = element.style.backgroundRepeat;
    this.originalStyle.backgroundSize = element.style.backgroundSize;
    this.originalStyle.backgroundPosition = element.style.backgroundPosition;

    let win = element.ownerGlobal;
    let dir =
      element.ownerDocument.documentElement.dir === "rtl" ? "left" : "right";
    let inputHeight = parseInt(
      win.getComputedStyle(element).getPropertyValue("height")
    );
    let inputPaddingRight = parseInt(
      win.getComputedStyle(element).getPropertyValue(`padding-${dir}`)
    );
    let iconHeight = inputHeight * 0.6;
    let iconPadding = inputHeight * 0.4 * 0.5;

    element.style.backgroundImage = `url(${this.iconUrl})`;
    element.style.backgroundRepeat = "no-repeat";
    element.style.backgroundSize = `${iconHeight}px ${iconHeight}px`;
    element.style.backgroundPosition = `${dir} ${iconPadding +
      inputPaddingRight}px center`;

    element.voiceInputStyleApplied = true;
  }

  removeVoiceInputStyle(element) {
    if (!element.voiceInputStyleApplied) {
      this.log(
        "Warning! skip removing style on the element that is not applied."
      );
      return;
    }

    element.style.backgroundImage = this.originalStyle.backgroundImage;
    element.style.backgroundRepeat = this.originalStyle.backgroundRepeat;
    element.style.backgroundSize = this.originalStyle.backgroundSize;
    element.style.backgroundPosition = this.originalStyle.backgroundPosition;

    this.originalStyle = {};
    element.voiceInputStyleApplied = false;
  }
}
