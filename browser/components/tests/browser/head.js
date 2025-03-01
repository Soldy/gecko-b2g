/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  sinon: "resource://testing-common/Sinon.jsm",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.jsm",
});

let didMockShell = false;
function mockShell(overrides = {}) {
  if (!didMockShell) {
    sinon.stub(window, "getShellService");
    registerCleanupFunction(() => {
      getShellService.restore();
    });
    didMockShell = true;
  }

  let mock = {
    canPin: false,
    isDefault: false,
    isPinned: false,

    checkPinCurrentAppToTaskbar() {
      if (!this.canPin) {
        throw Error;
      }
    },
    isCurrentAppPinnedToTaskbarAsync() {
      return Promise.resolve(this.isPinned);
    },
    isDefaultBrowser() {
      return this.isDefault;
    },
    // eslint-disable-next-line mozilla/use-chromeutils-generateqi
    QueryInterface() {
      return this;
    },
    get shellService() {
      return this;
    },

    pinCurrentAppToTaskbar: sinon.stub(),
    setAsDefault: sinon.stub(),
    ...overrides,
  };

  // Prefer the mocked implementation and fall back to the original version,
  // which can call back into the mocked version (via this.shellService).
  mock = new Proxy(mock, {
    get(target, prop) {
      return (prop in target ? target : ShellService)[prop];
    },
    set(target, prop, val) {
      (prop in target ? target : ShellService)[prop] = val;
      return true;
    },
  });

  getShellService.returns(mock);
  return mock;
}
