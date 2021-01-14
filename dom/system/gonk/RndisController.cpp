/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AutoMounter.h"
#include "RndisController.h"
#include <nsThreadUtils.h>
#include "mozilla/ClearOnShutdown.h"
#include "SystemProperty.h"
#include "nsTArray.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::system;

// Polling interval for "sys.usb.state" and uevent.
static const uint32_t POLLING_INTERVAL_MS = 100;
// The maximum time for setup usb configuration.
static const uint32_t SET_FUNCTIONS_TIMEOUT_MS = 3000;
// Wait for maximum time SET_FUNCTIONS_TIMEOUT_MS + ENUMERATION_TIME_OUT_MS
// during switch functions.
static const uint32_t ENUMERATION_TIME_OUT_MS = 2000;

static const char* USB_CONFIG_DELIMIT = ",";

static bool gTryEnableRndis = false;
static bool gUsbCableAttached = false;
static bool gUsbCableConfigured = false;
static uint32_t pollingTime = 0;

/**
 * Helper function that implement join function.
 */
static void join(nsTArray<nsCString>& array, const char* sep,
                 const uint32_t maxlen, char* result) {
#define CHECK_LENGTH(len, add, max) \
  len += add;                       \
  if (len > max - 1) return;

  uint32_t len = 0;
  uint32_t seplen = strlen(sep);

  if (array.Length() > 0) {
    CHECK_LENGTH(len, strlen(array[0].get()), maxlen)
    strcpy(result, array[0].get());

    for (uint32_t i = 1; i < array.Length(); i++) {
      CHECK_LENGTH(len, seplen, maxlen)
      strcat(result, sep);

      CHECK_LENGTH(len, strlen(array[i].get()), maxlen)
      strcat(result, array[i].get());
    }
  }
#undef CHECK_LEN
}

/**
 * Helper function to split string by seperator, store split result as an
 * nsTArray.
 */
static void split(char* str, const char* sep, nsTArray<nsCString>& result) {
  char* s = strtok(str, sep);
  while (s != nullptr) {
    result.AppendElement(s);
    s = strtok(nullptr, sep);
  }
}

static bool WaitForUsbState(bool aTryToFind, const char* aState) {
  char currentState[Property::VALUE_MAX_LENGTH];
  Property::Get(SYS_USB_STATE_PROPERTY, currentState, nullptr);

  nsTArray<nsCString> stateFuncs;
  split(currentState, USB_CONFIG_DELIMIT, stateFuncs);
  bool foundState = stateFuncs.Contains(nsCString(aState));

  if (foundState == aTryToFind) {
    return true;
  }
  if (pollingTime < SET_FUNCTIONS_TIMEOUT_MS) {
    pollingTime += POLLING_INTERVAL_MS;
    usleep(POLLING_INTERVAL_MS * 1000);
    return WaitForUsbState(aTryToFind, aState);
  }
  return false;
}

static bool WaitForConfigured() {
  // Disable rndis disturb.
  if (!gTryEnableRndis) {
    return false;
  } else if (gUsbCableConfigured) {
    return true;
  } else if (pollingTime < SET_FUNCTIONS_TIMEOUT_MS + ENUMERATION_TIME_OUT_MS) {
    pollingTime += POLLING_INTERVAL_MS;
    usleep(POLLING_INTERVAL_MS * 1000);
    return WaitForConfigured();
  }
  return false;
}

namespace mozilla {

StaticRefPtr<RndisController> sRndisController;
NS_IMPL_ISUPPORTS(RndisController, nsIRndisController)

// Working on rndis command in separate thread.
class RndisSetupDispatcher : public Runnable {
 public:
  explicit RndisSetupDispatcher(nsIRndisControllerResult* aCallback,
                                bool aEnable)
      : mozilla::Runnable("RndisSetupDispatcher"),
        mCallback(aCallback),
        mEnable(aEnable) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run() override {
    MOZ_ASSERT(!NS_IsMainThread());
    bool success = false;
    char currentConfig[Property::VALUE_MAX_LENGTH];
    Property::Get(SYS_USB_CONFIG, currentConfig, nullptr);

    nsTArray<nsCString> configFuncs;
    split(currentConfig, USB_CONFIG_DELIMIT, configFuncs);

    char persistConfig[Property::VALUE_MAX_LENGTH];
    Property::Get(PERSIST_SYS_USB_CONFIG, persistConfig, nullptr);

    nsTArray<nsCString> persistFuncs;
    split(persistConfig, USB_CONFIG_DELIMIT, persistFuncs);

    // Assign functions we request.
    if (mEnable) {
      configFuncs.Clear();
      configFuncs.AppendElement(nsCString(USB_FUNC_RNDIS));
      if (persistFuncs.Contains(nsCString(USB_FUNC_ADB))) {
        configFuncs.AppendElement(nsCString(USB_FUNC_ADB));
      }
    } else {
      // We're turning rndis off, revert back to the persist setting.
      // adb will already be correct there, so we don't need to do any
      // further adjustments.
      configFuncs = persistFuncs.Clone();
    }

    char newConfig[Property::VALUE_MAX_LENGTH] = "";
    Property::Get(SYS_USB_CONFIG, currentConfig, nullptr);
    join(configFuncs, USB_CONFIG_DELIMIT, Property::VALUE_MAX_LENGTH,
         newConfig);

    memset(&persistConfig, 0, sizeof(persistConfig));
    join(persistFuncs, USB_CONFIG_DELIMIT, Property::VALUE_MAX_LENGTH,
         persistConfig);

    bool cleanState = false;
    if (strcmp(currentConfig, newConfig)) {
      // Clean the USB stack to close existing connections.
      Property::Set(SYS_USB_CONFIG, USB_FUNC_NONE);
      if (WaitForUsbState(true, USB_FUNC_NONE)) {
        cleanState = true;
        Property::Set(SYS_USB_CONFIG, newConfig);
      } else {
        // Clean failed, reset to default and report failed.
        Property::Set(SYS_USB_CONFIG, persistConfig);
      }
    }

    usleep(POLLING_INTERVAL_MS * 1000);
    success = cleanState ? WaitForUsbState(mEnable, USB_FUNC_RNDIS) : false;

    // Change state finished, let's wait for enumeration result.
    if (mEnable && success) {
      success = WaitForConfigured();
    }
    // Reset timer since whole monitoring finish.
    pollingTime = 0;

    nsresult rv = NS_DispatchToMainThread(NS_NewRunnableFunction(
        "RndisResultDispatcher",
        [callback = std::move(mCallback), success]() -> void {
          if (sRndisController) {
            sRndisController->DispatchResult(callback, success);
          }
        }));
    return NS_OK;
  }

 private:
  bool mEnable;
  nsCOMPtr<nsIRndisControllerResult> mCallback;
};

RndisController::RndisController() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sRndisController);

  hal::RegisterUsbObserver(this);

  nsresult rv = NS_NewNamedThread("RndisThread", getter_AddRefs(mRndisThread));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
}

RndisController::~RndisController() {
  hal::UnregisterUsbObserver(this);

  if (mRndisThread) {
    mRndisThread->Shutdown();
    mRndisThread = nullptr;
  }

  MOZ_ASSERT(!sRndisController);
}

NS_IMETHODIMP
RndisController::SetupRndis(bool aEnable, nsIRndisControllerResult* aCallback) {
  gTryEnableRndis = aEnable;
  if (!mRndisThread) {
    aCallback->OnResult(false);
    return NS_OK;
  }

  // Notify AutoMounter to sync status.
  SetAutoMounterRndis(aEnable);
  nsCOMPtr<nsIRunnable> runnable = new RndisSetupDispatcher(aCallback, aEnable);
  mRndisThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  return NS_OK;
}

void RndisController::DispatchResult(nsIRndisControllerResult* aCallback,
                                     bool aSuccess) {
  MOZ_ASSERT(!NS_IsMainThread());
  if (aCallback) {
    aCallback->OnResult(aSuccess);
  }
}

void RndisController::Notify(const hal::UsbStatus& aUsbStatus) {
  gUsbCableAttached = aUsbStatus.deviceAttached();
  gUsbCableConfigured = aUsbStatus.deviceConfigured();
}

already_AddRefed<RndisController> RndisController::FactoryCreate() {
  if (!XRE_IsParentProcess()) {
    return nullptr;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!sRndisController) {
    sRndisController = new RndisController();
    ClearOnShutdown(&sRndisController);
  }

  RefPtr<RndisController> service = sRndisController.get();
  return service.forget();
}

}  // namespace mozilla
