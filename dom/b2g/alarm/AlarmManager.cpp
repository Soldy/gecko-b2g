/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AlarmManager.h"
#include "mozilla/dom/AlarmManagerWorker.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIPermissionManager.h"
#include "nsNetUtil.h"

static mozilla::LazyLogModule sAlarmManagerLog("AlarmManager");
#define LOG(...) \
  MOZ_LOG(sAlarmManagerLog, mozilla::LogLevel::Info, (__VA_ARGS__))

namespace mozilla {
namespace dom {

/*  Call flow of GetAll
 *  1. On Main Thread:
 *     AlarmManagerMain::GetAll
 *     new a AlarmGetAllCallback, pass to AlarmProxy.
 *
 *  2. On Main Thread:
 *     AlarmGetAllCallback::OnGetAll
 *     Invoked by AlarmProxy. Wrap the return value, and then resolve or
 *     reject.
 */

class AlarmGetAllCallback final : public nsIAlarmGetAllCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIALARMGETALLCALLBACK
  explicit AlarmGetAllCallback(Promise* aPromise);

 protected:
  ~AlarmGetAllCallback() = default;

 private:
  RefPtr<Promise> mPromise;
};

NS_IMPL_ISUPPORTS(AlarmGetAllCallback, nsIAlarmGetAllCallback)

AlarmGetAllCallback::AlarmGetAllCallback(Promise* aPromise)
    : mPromise(aPromise) {
  LOG("AlarmGetAllCallback constructor.");
}

NS_IMETHODIMP
AlarmGetAllCallback::OnGetAll(nsresult aStatus, JS::HandleValue aResult,
                              JSContext* aCx) {
  LOG("OnGetAll. aCx:[%p]", aCx);

  if (NS_WARN_IF(!mPromise)) {
    LOG("!mPromise");
    return NS_ERROR_ABORT;
  }
  if (NS_SUCCEEDED(aStatus)) {
    mPromise->MaybeResolveWithClone(aCx, aResult);
  } else {
    mPromise->MaybeRejectWithClone(aCx, aResult);
  }
  return NS_OK;
}

class AlarmAddCallback final : public nsIAlarmAddCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIALARMADDCALLBACK
  explicit AlarmAddCallback(Promise* aPromise);

 protected:
  ~AlarmAddCallback() = default;

 private:
  RefPtr<Promise> mPromise;
};

NS_IMPL_ISUPPORTS(AlarmAddCallback, nsIAlarmAddCallback)

AlarmAddCallback::AlarmAddCallback(Promise* aPromise) : mPromise(aPromise) {
  LOG("AlarmAddCallback constructor.");
}

NS_IMETHODIMP
AlarmAddCallback::OnAdd(nsresult aStatus, JS::HandleValue aResult,
                        JSContext* aCx) {
  LOG("OnAdd aCx:[%p]", aCx);

  if (NS_WARN_IF(!mPromise)) {
    LOG("!mPromise");
    return NS_ERROR_ABORT;
  }
  if (NS_SUCCEEDED(aStatus)) {
    mPromise->MaybeResolve(aResult);
  } else {
    mPromise->MaybeReject(aResult);
  }
  return NS_OK;
}

AlarmManagerMain::AlarmManagerMain(nsCString aUrl,
                                   nsIGlobalObject* aOuterGlobal)
    : AlarmManagerImpl(aUrl, aOuterGlobal) {
  LOG("AlarmManagerMain constructor.");
  if (NS_WARN_IF(!NS_IsMainThread())) {
    LOG("Error! AlarmManagerWorker should be on main thread.");
  }
}

already_AddRefed<Promise> AlarmManagerMain::GetAll() {
  LOG("AlarmManagerMain::GetAll mUrl:[%s]", mUrl.get());

  if (NS_WARN_IF(!mOuterGlobal)) {
    LOG("!mOuterGlobal");
    return nullptr;
  }

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(mOuterGlobal, rv);
  ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIAlarmProxy> alarmProxy = alarm::CreateAlarmProxy();
  if (NS_WARN_IF(!alarmProxy)) {
    LOG("!alarmProxy");
    promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return promise.forget();
  }

  nsCOMPtr<nsIAlarmGetAllCallback> callback = new AlarmGetAllCallback(promise);
  alarmProxy->GetAll(mUrl, callback);

  return promise.forget();
}

already_AddRefed<Promise> AlarmManagerMain::Add(JSContext* aCx,
                                                const AlarmOptions& aOptions) {
  LOG("AlarmManagerMain::Add mUrl:[%s]", mUrl.get());

  if (NS_WARN_IF(!mOuterGlobal)) {
    LOG("!mOuterGlobal");
    return nullptr;
  }

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(mOuterGlobal, rv);
  ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIAlarmProxy> alarmProxy = alarm::CreateAlarmProxy();
  if (NS_WARN_IF(!alarmProxy)) {
    LOG("!alarmProxy");
    promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return promise.forget();
  }

  // We're about to pass the dictionary to a JS-implemented component, so
  // rehydrate it in a system scode so that security wrappers don't get in the
  // way. See bug 1161748 comment 16.
  bool ok;
  JS::RootedValue optionsValue(aCx);
  {
    JSAutoRealm ar(aCx, xpc::PrivilegedJunkScope());
    ok = ToJSValue(aCx, aOptions, &optionsValue);
    if (!ok) {
      promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
      return promise.forget();
    }
  }
  ok = JS_WrapValue(aCx, &optionsValue);
  if (!ok) {
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return promise.forget();
  }

  nsCOMPtr<nsIAlarmAddCallback> callback = new AlarmAddCallback(promise);
  alarmProxy->Add(mUrl, optionsValue, callback);
  return promise.forget();
}

void AlarmManagerMain::Remove(long aId) {
  nsCOMPtr<nsIAlarmProxy> alarmProxy = alarm::CreateAlarmProxy();
  if (NS_WARN_IF(!alarmProxy)) {
    LOG("!alarmProxy");
    return;
  }

  alarmProxy->Remove(mUrl, aId);
}

// static
already_AddRefed<AlarmManager> AlarmManager::Create(nsIGlobalObject* aGlobal,
                                                    nsresult& aRv) {
  if (NS_WARN_IF(!aGlobal)) {
    LOG("!aGlobal");
    aRv = NS_ERROR_DOM_ABORT_ERR;
    return nullptr;
  }

  RefPtr<AlarmManager> alarmManager = new AlarmManager(aGlobal);

  aRv = alarmManager->Init();
  if (NS_WARN_IF(NS_FAILED(aRv))) {
    LOG("Init failed. rv:[%u]", uint(aRv));
    return nullptr;
  }

  if (!alarmManager->CheckPermission()) {
    LOG("!CheckPermission");
    aRv = NS_ERROR_DOM_SECURITY_ERR;
    return nullptr;
  }

  return alarmManager.forget();
}

AlarmManager::AlarmManager(nsIGlobalObject* aGlobal) : mGlobal(aGlobal) {
  LOG("AlarmManager constructor.");
}

NS_IMETHODIMP AlarmManager::Init() {
  LOG("AlarmManager Init. NS_IsMainThread:[%s]",
      NS_IsMainThread() ? "true" : "false");

  if (NS_WARN_IF(!mGlobal)) {
    LOG("!mGlobal");
    return NS_ERROR_DOM_ABORT_ERR;
  }

  Maybe<ClientInfo> clientInfo = mGlobal->GetClientInfo();
  if (clientInfo.isSome()) {
    // System app might call AlarmManager from the system startup chrome url.
    // To ensure proper system message receiver, replace the url with system
    // app's origin.
    // Other modules or services in Gecko of chrome origins should call
    // AlarmService directly, not through AlarmManager or AlarmProxy.
    LOG("clientInfo->URL() [%s]", clientInfo->URL().get());
    if (nsContentUtils::IsCallerChrome()) {
      nsAutoCString systemStartupUrl;
      nsresult rv =
          Preferences::GetCString("b2g.system_startup_url", systemStartupUrl);
      if (NS_SUCCEEDED(rv) && clientInfo->URL().Equals(systemStartupUrl)) {
        mUrl = "http://system.localhost"_ns;
      }
    } else {
      nsCOMPtr<nsIIOService> ios(do_GetIOService());
      nsCOMPtr<nsIURI> uri;
      nsresult rv =
          ios->NewURI(clientInfo->URL(), nullptr, nullptr, getter_AddRefs(uri));

      if (NS_WARN_IF(NS_FAILED(rv)) || !uri) {
        LOG("NewURI failed.");
        return NS_ERROR_DOM_UNKNOWN_ERR;
      }

      uri->GetPrePath(mUrl);
    }
  } else {
    LOG("Error: !clientInfo.isSome()");
    return NS_ERROR_DOM_UNKNOWN_ERR;
  }

  LOG("mUrl:[%s]", mUrl.get());

  // Alarms added by apps should be well-managed according to app urls.
  // Empty urls may cause ambiguity.
  if (mUrl.IsEmpty()) {
    return NS_ERROR_DOM_UNKNOWN_ERR;
  }

  if (NS_IsMainThread()) {
    mImpl = new AlarmManagerMain(mUrl, mGlobal);
  } else {
    mImpl = new AlarmManagerWorker(mUrl, mGlobal);
  }

  return NS_OK;
}

bool AlarmManager::CheckPermission() {
  if (NS_WARN_IF(!mImpl)) {
    LOG("!mImpl");
    return false;
  }

  return mImpl->CheckPermission();
}

already_AddRefed<Promise> AlarmManager::GetAll() {
  LOG("AlarmManager::GetAll mUrl:[%s] NS_IsMainThread:[%s]", mUrl.get(),
      NS_IsMainThread() ? "true" : "false");
  if (NS_WARN_IF(!mImpl)) {
    LOG("!mImpl");
    return nullptr;
  }

  return mImpl->GetAll();
}

already_AddRefed<Promise> AlarmManager::Add(JSContext* aCx,
                                            const AlarmOptions& aOptions) {
  LOG("AlarmManager::Add mUrl:[%s] NS_IsMainThread:[%s]", mUrl.get(),
      NS_IsMainThread() ? "true" : "false");
  if (NS_WARN_IF(!mImpl)) {
    LOG("!mImpl");
    return nullptr;
  }

  return mImpl->Add(aCx, aOptions);
}

void AlarmManager::Remove(long aId) {
  LOG("AlarmManager::Remove mUrl:[%s] aId:[%d] NS_IsMainThread:[%s]",
      mUrl.get(), aId, NS_IsMainThread() ? "true" : "false");
  if (NS_WARN_IF(!mImpl)) {
    LOG("!mImpl");
    return;
  }

  return mImpl->Remove(aId);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AlarmManager, mGlobal)
NS_IMPL_CYCLE_COLLECTING_ADDREF(AlarmManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AlarmManager)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AlarmManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* AlarmManager::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return AlarmManager_Binding::Wrap(aCx, this, aGivenProto);
}

bool AlarmManagerMain::CheckPermission() {
  return alarm::DoCheckPermission(mUrl);
}

already_AddRefed<nsIAlarmProxy> alarm::CreateAlarmProxy() {
  if (NS_WARN_IF(!NS_IsMainThread())) {
    LOG("CreateAlarmProxy should not be called from non main threads.");
    return nullptr;
  }

  nsresult rv;
  nsCOMPtr<nsIAlarmProxy> alarmProxy =
      do_CreateInstance("@mozilla.org/dom/alarm/proxy;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv)) || NS_WARN_IF(!alarmProxy)) {
    LOG("do_CreateInstance of nsIAlarmProxy failed. rv:[%u]", uint(rv));
    return nullptr;
  }

  return alarmProxy.forget();
}

bool alarm::DoCheckPermission(nsCString aUrl) {
  if (NS_WARN_IF(!NS_IsMainThread())) {
    LOG("DoCheckPermission should not be called from non main threads.");
    return false;
  }

  nsCOMPtr<nsIPermissionManager> permissionManager =
      services::GetPermissionManager();
  if (NS_WARN_IF(!permissionManager)) {
    LOG("!permissionManager");
    return false;
  }

  nsCOMPtr<nsIPrincipal> principal =
      BasePrincipal::CreateContentPrincipal(aUrl);
  if (NS_WARN_IF(!principal)) {
    LOG("!principal aUrl:[%s]", aUrl.get());
    return false;
  }

  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  nsresult rv = permissionManager->TestPermissionFromPrincipal(
      principal, "alarms"_ns, &permission);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG("TestPermissionFromPrincipal failed. rv[%u] mUrl:[%s]", uint(rv),
        aUrl.get());
    return false;
  }

  return permission == nsIPermissionManager::ALLOW_ACTION;
}
}  // namespace dom
}  // namespace mozilla
