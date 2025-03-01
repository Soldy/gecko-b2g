/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothService.h"

#include "BluetoothCommon.h"
#include "BluetoothParent.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothServiceChildProcess.h"
#include "BluetoothUtils.h"

#include "jsapi.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "nsContentUtils.h"
#include "nsIObserverService.h"
#include "nsITimer.h"
#include "nsServiceManagerUtils.h"
#include "nsXPCOM.h"

#if defined(MOZ_WIDGET_GONK)
#  include "cutils/properties.h"
#endif

#if defined(MOZ_B2G_BT_DAEMON)
#  include "BluetoothServiceBluedroid.h"
#else
#  error No backend
#endif

#define BLUETOOTH_ENABLED_SETTING u"bluetooth.enabled"_ns
#define BLUETOOTH_DEBUGGING_SETTING u"bluetooth.debugging.enabled"_ns

#define PROP_BLUETOOTH_ENABLED "bluetooth.isEnabled"

#define DEFAULT_SHUTDOWN_TIMER_MS 5000

using namespace mozilla;
using namespace mozilla::dom;
USING_BLUETOOTH_NAMESPACE

namespace {

StaticRefPtr<BluetoothService> sBluetoothService;

bool sInShutdown = false;
bool sToggleInProgress = false;

void ShutdownTimeExceeded(nsITimer* aTimer, void* aClosure) {
  MOZ_ASSERT(NS_IsMainThread());
  *static_cast<bool*>(aClosure) = true;
}

void GetAllBluetoothActors(nsTArray<BluetoothParent*>& aActors) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActors.IsEmpty());

  AutoTArray<ContentParent*, 20> contentActors;
  ContentParent::GetAll(contentActors);

  for (uint32_t contentIndex = 0; contentIndex < contentActors.Length();
       contentIndex++) {
    MOZ_ASSERT(contentActors[contentIndex]);

    AutoTArray<PBluetoothParent*, 5> bluetoothActors;
    contentActors[contentIndex]->ManagedPBluetoothParent(bluetoothActors);

    for (uint32_t bluetoothIndex = 0; bluetoothIndex < bluetoothActors.Length();
         bluetoothIndex++) {
      MOZ_ASSERT(bluetoothActors[bluetoothIndex]);

      BluetoothParent* actor =
          static_cast<BluetoothParent*>(bluetoothActors[bluetoothIndex]);
      aActors.AppendElement(actor);
    }
  }
}

}  // namespace

BluetoothService::ToggleBtAck::ToggleBtAck(bool aEnabled)
    : Runnable("ToggleBtAck"), mEnabled(aEnabled) {}

NS_IMETHODIMP
BluetoothService::ToggleBtAck::Run() {
  BluetoothService::AcknowledgeToggleBt(mEnabled);

  return NS_OK;
}

NS_IMPL_ISUPPORTS(BluetoothService, nsIObserver, nsISettingsGetResponse,
                  nsISettingsGetResponse, nsISettingsObserver)

bool BluetoothService::IsToggling() const { return sToggleInProgress; }

BluetoothService::~BluetoothService() { Cleanup(); }

// static
BluetoothService* BluetoothService::Create() {
#if defined(MOZ_B2G_BT_DAEMON)
  if (!XRE_IsParentProcess()) {
    return BluetoothServiceChildProcess::Create();
  }

  return new BluetoothServiceBluedroid();
#endif

  BT_WARNING("No platform support for bluetooth!");
  return nullptr;
}

bool BluetoothService::Init() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, false);

  if (NS_FAILED(obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false))) {
    BT_WARNING("Failed to add shutdown observer!");
    return false;
  }

  if (NS_FAILED(HandleStartup())) {
    BT_WARNING("Failed to HandleStartup");
    return false;
  }

  return true;
}

void BluetoothService::Cleanup() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs &&
      (NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID)))) {
    BT_WARNING("Can't unregister observers!");
  }

  nsCOMPtr<nsISettingsManager> settings =
      do_GetService("@mozilla.org/sidl-native/settings;1");
  if (settings) {
    settings->RemoveObserver(BLUETOOTH_DEBUGGING_SETTING, this, this);
  }
}

void BluetoothService::RegisterBluetoothSignalHandler(
    const nsAString& aNodeName, BluetoothSignalObserver* aHandler) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aHandler);

  BT_LOGD("[S] %s: %s", __FUNCTION__, NS_ConvertUTF16toUTF8(aNodeName).get());

  BluetoothSignalObserverList* ol;
  if (!mBluetoothSignalObserverTable.Get(aNodeName, &ol)) {
    ol = mBluetoothSignalObserverTable.InsertOrUpdate(aNodeName, MakeUnique<BluetoothSignalObserverList>()).get();
  }

  ol->AddObserver(aHandler);

  // Distribute pending requests when listener has been added to signal
  // observer table.
  if (XRE_IsParentProcess()) {
    // For pairing requests
    if (!mPendingPairReqSignals.IsEmpty() &&
        aNodeName.Equals(KEY_PAIRING_LISTENER)) {
      for (uint32_t i = 0; i < mPendingPairReqSignals.Length(); ++i) {
        DistributeSignal(mPendingPairReqSignals[i]);
      }
      mPendingPairReqSignals.Clear();
    }

    // For Pbap request
    if (!mPendingPbapReqSignals.IsEmpty() && aNodeName.Equals(KEY_PBAP)) {
      for (uint32_t i = 0; i < mPendingPbapReqSignals.Length(); ++i) {
        DistributeSignal(mPendingPbapReqSignals[i]);
      }
      mPendingPbapReqSignals.Clear();
    }

    // For Map requests
    if (!mPendingMapReqSignals.IsEmpty() && aNodeName.Equals(KEY_MAP)) {
      for (uint32_t i = 0; i < mPendingMapReqSignals.Length(); ++i) {
        DistributeSignal(mPendingMapReqSignals[i]);
      }
      mPendingMapReqSignals.Clear();
    }
  }
}

void BluetoothService::UnregisterBluetoothSignalHandler(
    const nsAString& aNodeName, BluetoothSignalObserver* aHandler) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aHandler);

  BT_LOGD("[S] %s: %s", __FUNCTION__, NS_ConvertUTF16toUTF8(aNodeName).get());

  BluetoothSignalObserverList* ol;
  if (mBluetoothSignalObserverTable.Get(aNodeName, &ol)) {
    ol->RemoveObserver(aHandler);
    // We shouldn't have duplicate instances in the ObserverList, but there's
    // no appropriate way to do duplication check while registering, so
    // assertions are added here.
    MOZ_ASSERT(!ol->RemoveObserver(aHandler));
    if (ol->Length() == 0) {
      mBluetoothSignalObserverTable.Remove(aNodeName);
    }
  } else {
    BT_LOGD("Node isn't registered in the observr table.");
  }
}

void BluetoothService::UnregisterAllSignalHandlers(
    BluetoothSignalObserver* aHandler) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aHandler);

  for (auto iter = mBluetoothSignalObserverTable.Iter(); !iter.Done();
       iter.Next()) {
    auto& ol = iter.Data();

    ol->RemoveObserver(aHandler);
    // We shouldn't have duplicate instances in the ObserverList, but there's
    // no appropriate way to do duplication check while registering, so
    // assertions are added here.
    MOZ_ASSERT(!ol->RemoveObserver(aHandler));
    if (ol->Length() == 0) {
      iter.Remove();
    }
  }
}

void BluetoothService::DistributeSignal(const nsAString& aName,
                                        const nsAString& aPath) {
  DistributeSignal(aName, aPath, BluetoothValue(true));
}

void BluetoothService::DistributeSignal(const nsAString& aName,
                                        const nsAString& aPath,
                                        const BluetoothValue& aValue) {
  BluetoothSignal signal(nsString(aName), nsString(aPath), aValue);
  DistributeSignal(signal);
}

void BluetoothService::DistributeSignal(const nsAString& aName,
                                        const BluetoothAddress& aAddress) {
  nsAutoString path;
  AddressToString(aAddress, path);

  DistributeSignal(aName, path);
}

void BluetoothService::DistributeSignal(const nsAString& aName,
                                        const BluetoothAddress& aAddress,
                                        const BluetoothValue& aValue) {
  nsAutoString path;
  AddressToString(aAddress, path);

  DistributeSignal(aName, path, aValue);
}

void BluetoothService::DistributeSignal(const nsAString& aName,
                                        const BluetoothUuid& aUuid) {
  nsAutoString path;
  UuidToString(aUuid, path);

  DistributeSignal(aName, path);
}

void BluetoothService::DistributeSignal(const nsAString& aName,
                                        const BluetoothUuid& aUuid,
                                        const BluetoothValue& aValue) {
  nsAutoString path;
  UuidToString(aUuid, path);

  DistributeSignal(aName, path, aValue);
}

void BluetoothService::DistributeSignal(const BluetoothSignal& aSignal) {
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothSignalObserverList* ol;

  // If there is a corresponding signal path (key of hash table) in observer
  // table, broadcast the signal.
  if (mBluetoothSignalObserverTable.Get(aSignal.path(), &ol)) {
    MOZ_ASSERT(ol->Length());
    ol->Broadcast(aSignal);

    // Still send system message even KEY_PAIRING_LISTENER is in observer table.
    // The message benifits those apps which don't have BluetoohPairingListener.
    if (aSignal.path().Equals(KEY_PAIRING_LISTENER) && XRE_IsParentProcess()) {
      BT_ENSURE_TRUE_VOID_BROADCAST_SYSMSG(SYS_MSG_BT_PAIRING_REQ,
                                           BluetoothValue(EmptyString()));
    }
    return;
  }

  BT_WARNING("No observer registered for path %s",
             NS_ConvertUTF16toUTF8(aSignal.path()).get());

  // If there is no BluetoohPairingListener in observer table, put the signal
  // into a pending queue of pairing requests and send a system message to
  // launch bluetooth certified app.
  if (aSignal.path().Equals(KEY_PAIRING_LISTENER)) {
    mPendingPairReqSignals.AppendElement(aSignal);

    BT_ENSURE_TRUE_VOID_BROADCAST_SYSMSG(SYS_MSG_BT_PAIRING_REQ,
                                         BluetoothValue(EmptyString()));
  } else if (aSignal.path().Equals(KEY_PBAP)) {
    // If there is no signal path for KEY_PBAP in observer table
    mPendingPbapReqSignals.AppendElement(aSignal);

    BT_ENSURE_TRUE_VOID_BROADCAST_SYSMSG(SYS_MSG_BT_PBAP_REQ,
                                         BluetoothValue(EmptyString()));
    BT_LOGR("Queue the request and send system message to launch PBAP app");
  } else if (aSignal.path().Equals(KEY_MAP)) {
    // If there is no signal path for KEY_MAP in observer table
    mPendingMapReqSignals.AppendElement(aSignal);

    BT_ENSURE_TRUE_VOID_BROADCAST_SYSMSG(SYS_MSG_BT_MAP_REQ,
                                         BluetoothValue(EmptyString()));
    BT_LOGR("Queue the request and send system message to launch MAP app");
  }
}

nsresult BluetoothService::StartBluetooth(bool aIsStartup,
                                          BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  if (sInShutdown) {
    // Don't try to start if we're already shutting down.
    MOZ_ASSERT(false, "Start called while in shutdown!");
    return NS_ERROR_FAILURE;
  }

  /* When IsEnabled() is true, we don't switch on Bluetooth but we still
   * send ToggleBtAck task. One special case happens at startup stage. At
   * startup, the initialization of BluetoothService still has to be done
   * even if Bluetooth is already enabled.
   *
   * Please see bug 892392 for more information.
   */
  if (aIsStartup || !IsEnabled()) {
    // Switch Bluetooth on
    nsresult rv = StartInternal(aRunnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("Bluetooth service failed to start!");
      return rv;
    }
  } else {
    BT_WARNING("Bluetooth has already been enabled before.");
    RefPtr<nsIRunnable> runnable = new BluetoothService::ToggleBtAck(true);
    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      BT_WARNING("Failed to dispatch to main thread!");
    }
  }

  return NS_OK;
}

nsresult BluetoothService::StopBluetooth(bool aIsStartup,
                                         BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  /* When IsEnabled() is false, we don't switch off Bluetooth but we still
   * send ToggleBtAck task. One special case happens at startup stage. At
   * startup, the initialization of BluetoothService still has to be done
   * even if Bluetooth is disabled.
   *
   * Please see bug 892392 for more information.
   */
  if (aIsStartup || IsEnabled()) {
    // Any connected Bluetooth profile would be disconnected.
    nsresult rv = StopInternal(aRunnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("Bluetooth service failed to stop!");
      return rv;
    }
  } else {
    BT_WARNING("Bluetooth has already been enabled/disabled before.");
    RefPtr<nsIRunnable> runnable = new BluetoothService::ToggleBtAck(false);
    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      BT_WARNING("Failed to dispatch to main thread!");
    }
  }

  return NS_OK;
}

nsresult BluetoothService::StartStopBluetooth(
    bool aStart, bool aIsStartup, BluetoothReplyRunnable* aRunnable) {
  nsresult rv;
  if (aStart) {
    rv = StartBluetooth(aIsStartup, aRunnable);
  } else {
    rv = StopBluetooth(aIsStartup, aRunnable);
  }
  return rv;
}

void BluetoothService::SetEnabled(bool aEnabled) {
  MOZ_ASSERT(NS_IsMainThread());

  AutoTArray<BluetoothParent*, 10> childActors;
  GetAllBluetoothActors(childActors);

  for (uint32_t index = 0; index < childActors.Length(); index++) {
    Unused << childActors[index]->SendEnabled(aEnabled);
  }

  /**
   * mEnabled: real status of bluetooth
   * aEnabled: expected status of bluetooth
   */
  if (mEnabled == aEnabled) {
    BT_WARNING("Bluetooth is already %s, or the toggling failed.",
               mEnabled ? "enabled" : "disabled");
  }

  mEnabled = aEnabled;
}

nsresult BluetoothService::HandleStartup() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sToggleInProgress);

  nsCOMPtr<nsISettingsManager> settings =
      do_GetService("@mozilla.org/sidl-native/settings;1");

  if (!settings) {
    BT_WARNING("Failed to get nsISettingsManager");
    return NS_ERROR_FAILURE;
  }

  settings->Get(BLUETOOTH_ENABLED_SETTING, this);
  settings->Get(BLUETOOTH_DEBUGGING_SETTING, this);
  settings->AddObserver(BLUETOOTH_DEBUGGING_SETTING, this, this);

  sToggleInProgress = true;
  return NS_OK;
}

nsresult BluetoothService::HandleStartupSettingsCheck(bool aEnable) {
  MOZ_ASSERT(NS_IsMainThread());
  return StartStopBluetooth(aEnable, true, nullptr);
}

// Implements nsISettingsGetResponse::Resolve
NS_IMETHODIMP BluetoothService::Resolve(nsISettingInfo* info) {
  MOZ_ASSERT(NS_IsMainThread());

  if (info) {
    nsString name;
    info->GetName(name);

    nsString value;
    info->GetValue(value);
    bool enabled = !value.EqualsLiteral("false");

    if (name.Equals(BLUETOOTH_ENABLED_SETTING)) {
      // It is theoretically possible to shut down before the first settings
      // check has completed (though extremely unlikely).
      if (sBluetoothService) {
        sBluetoothService->HandleStartupSettingsCheck(enabled);
      }
    } else if (name.Equals(BLUETOOTH_DEBUGGING_SETTING)) {
      SWITCH_BT_DEBUG(enabled);
    }
  } else {
    BT_WARNING("SettingInfo in BluetoothService is null");
    sBluetoothService->HandleStartupSettingsCheck(false);
  }
  return NS_OK;
}

// Implements nsISettingsGetResponse::Reject
NS_IMETHODIMP BluetoothService::Reject([
    [maybe_unused]] nsISettingError* error) {
  if (error) {
    nsString name;
    error->GetName(name);
    BT_WARNING("Failed to read settings '%s'",
               NS_ConvertUTF16toUTF8(name).get());
    if (sToggleInProgress && sBluetoothService &&
        name.Equals(BLUETOOTH_ENABLED_SETTING)) {
      sBluetoothService->HandleStartupSettingsCheck(false);
    }
  }

  return NS_OK;
}

// Implements nsISettingsObserver::ObserveSetting
NS_IMETHODIMP BluetoothService::ObserveSetting(nsISettingInfo* info) {
  MOZ_ASSERT(NS_IsMainThread());

  if (info) {
    nsString name;
    info->GetName(name);
    if (name.Equals(BLUETOOTH_DEBUGGING_SETTING)) {
      nsString value;
      info->GetValue(value);
      bool enabled = !value.EqualsLiteral("false");
      SWITCH_BT_DEBUG(enabled);
    }
  }
  return NS_OK;
}

// Implements nsISidlDefaultResponse
NS_IMETHODIMP BluetoothService::Resolve() { return NS_OK; }
NS_IMETHODIMP BluetoothService::Reject() {
  BT_WARNING("Failed to observe setting 'bluetooth.debugging.enabled'");
  return NS_OK;
}

nsresult BluetoothService::HandleShutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  // This is a two phase shutdown. First we notify all child processes that
  // bluetooth is going away, and then we wait for them to acknowledge. Then we
  // close down all the bluetooth machinery.

  Cleanup();

  AutoTArray<BluetoothParent*, 10> childActors;
  GetAllBluetoothActors(childActors);

  if (!childActors.IsEmpty()) {
    // Notify child processes that they should stop using bluetooth now.
    for (uint32_t index = 0; index < childActors.Length(); index++) {
      childActors[index]->BeginShutdown();
    }

    // Create a timer to ensure that we don't wait forever for a child process
    // or the bluetooth threads to finish. If we don't get a timer or can't use
    // it for some reason then we skip all the waiting entirely since we really
    // can't afford to hang on shutdown.
    nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
    MOZ_ASSERT(timer);

    if (timer) {
      bool timeExceeded = false;

      if (NS_SUCCEEDED(timer->InitWithNamedFuncCallback(
              ShutdownTimeExceeded, &timeExceeded, DEFAULT_SHUTDOWN_TIMER_MS,
              nsITimer::TYPE_ONE_SHOT, "BluetoothServiceTimer"))) {
        nsIThread* currentThread = NS_GetCurrentThread();
        MOZ_ASSERT(currentThread);

        // Wait for those child processes to acknowledge.
        while (!timeExceeded && !childActors.IsEmpty()) {
          if (!NS_ProcessNextEvent(currentThread)) {
            MOZ_ASSERT(false, "Something horribly wrong here!");
            break;
          }
          GetAllBluetoothActors(childActors);
        }

        if (NS_FAILED(timer->Cancel())) {
          MOZ_CRASH("Failed to cancel shutdown timer, this will crash!");
        }
      } else {
        MOZ_ASSERT(false, "Failed to initialize shutdown timer!");
      }
    }
  }

  if (IsEnabled() && NS_FAILED(StopBluetooth(false, nullptr))) {
    MOZ_ASSERT(false, "Failed to deliver stop message!");
  }

  return NS_OK;
}

// static
BluetoothService* BluetoothService::Get() {
  MOZ_ASSERT(NS_IsMainThread());

  // If we already exist, exit early
  if (sBluetoothService) {
    return sBluetoothService;
  }

  // If we're in shutdown, don't create a new instance
  if (sInShutdown) {
    BT_WARNING("BluetoothService can't be created during shutdown");
    return nullptr;
  }

  // Create new instance, register, return
  sBluetoothService = BluetoothService::Create();
  NS_ENSURE_TRUE(sBluetoothService, nullptr);

  if (!sBluetoothService->Init()) {
    sBluetoothService->Cleanup();
    return nullptr;
  }

  ClearOnShutdown(&sBluetoothService);
  return sBluetoothService;
}

nsresult BluetoothService::Observe(nsISupports* aSubject, const char* aTopic,
                                   const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    /**
     * |sInShutdown| flag should be set for instances created in content
     * processes or parent processes. Please see bug 1199653 for detailed
     * information.
     */
    sInShutdown = true;

    return HandleShutdown();
  }

  MOZ_ASSERT(false, "BluetoothService got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

/**
 * Enable/Disable the local adapter.
 *
 * There is only one adapter on the mobile in current use cases.
 * In addition, bluedroid couldn't enable/disable a single adapter.
 * So currently we will turn on/off BT to enable/disable the adapter.
 *
 * TODO: To support enable/disable single adapter in the future,
 *       we will need to implement EnableDisableInternal for different stacks.
 */
nsresult BluetoothService::EnableDisable(bool aEnable,
                                         BluetoothReplyRunnable* aRunnable) {
  sToggleInProgress = true;
  return StartStopBluetooth(aEnable, false, aRunnable);
}

void BluetoothService::FireAdapterStateChanged(bool aEnable) {
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<BluetoothNamedValue> props;
  AppendNamedValue(props, "State", aEnable);

  DistributeSignal(u"PropertyChanged"_ns, KEY_ADAPTER, BluetoothValue(props));
}

void BluetoothService::AcknowledgeToggleBt(bool aEnabled) {
  MOZ_ASSERT(NS_IsMainThread());

#if defined(MOZ_WIDGET_GONK)
  // This is requested in Bug 836516. With settings this property, WLAN
  // firmware could be aware of Bluetooth has been turned on/off, so that the
  // mecahnism of handling coexistence of WIFI and Bluetooth could be started.
  //
  // In the future, we may have our own way instead of setting a system
  // property to let firmware developers be able to sense that Bluetooth has
  // been toggled.
  if (property_set(PROP_BLUETOOTH_ENABLED, aEnabled ? "true" : "false") != 0) {
    BT_WARNING("Failed to set bluetooth enabled property");
  }
#endif

  if (sInShutdown) {
    sBluetoothService = nullptr;
    return;
  }

  NS_ENSURE_TRUE_VOID(sBluetoothService);

  sBluetoothService->CompleteToggleBt(aEnabled);
}

void BluetoothService::CompleteToggleBt(bool aEnabled) {
  MOZ_ASSERT(NS_IsMainThread());

  // Update |mEnabled| of |BluetoothService| object since
  // |StartInternal| and |StopInternal| have been already
  // done.
  SetEnabled(aEnabled);
  sToggleInProgress = false;

  FireAdapterStateChanged(aEnabled);
}
