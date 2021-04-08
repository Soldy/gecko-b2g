/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AuthorizationManager.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "nsComponentManagerUtils.h"

namespace mozilla {
namespace dom {

class OnAuthorizationWorkerRunnable final : public WorkerRunnable,
                                            public StructuredCloneHolder {
 public:
  OnAuthorizationWorkerRunnable(
      WorkerPrivate* aWorkerPrivate,
      already_AddRefed<PromiseWorkerProxy>&& aPromiseWorkerProxy,
      nsresult aStatus)
      : WorkerRunnable(aWorkerPrivate),
        StructuredCloneHolder(CloningSupported, TransferringSupported,
                              StructuredCloneScope::SameProcess),
        mPromiseWorkerProxy(std::move(aPromiseWorkerProxy)),
        mStatus(aStatus) {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->AssertIsOnWorkerThread();
    RefPtr<Promise> promise = mPromiseWorkerProxy->WorkerPromise();

    JS::RootedObject globalObject(aCx, JS::CurrentGlobalOrNull(aCx));
    if (NS_WARN_IF(!globalObject)) {
      promise->MaybeReject(NS_ERROR_UNEXPECTED);
      return false;
    }

    nsCOMPtr<nsIGlobalObject> global = xpc::NativeGlobal(globalObject);
    if (NS_WARN_IF(!global)) {
      promise->MaybeReject(NS_ERROR_UNEXPECTED);
      return false;
    }

    JS::RootedValue result(aCx);
    ErrorResult rv;

    Read(global, aCx, &result, rv);
    if (NS_WARN_IF(rv.Failed())) {
      promise->MaybeReject(NS_ERROR_UNEXPECTED);
      return false;
    }
    JS_WrapValue(aCx, &result);

    if (NS_SUCCEEDED(mStatus)) {
      promise->MaybeResolve(result);
    } else {
      promise->MaybeReject(result);
    }

    mPromiseWorkerProxy->CleanUp();

    return true;
  }

 private:
  ~OnAuthorizationWorkerRunnable() {
    MOZ_ASSERT(mPromiseWorkerProxy);
    mPromiseWorkerProxy->CleanUp();
  };

  RefPtr<PromiseWorkerProxy> mPromiseWorkerProxy;
  nsresult mStatus;
};

class AuthorizationCallback final : public nsIAuthorizationCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTHORIZATIONCALLBACK
  explicit AuthorizationCallback(Promise* aPromise);
  explicit AuthorizationCallback(PromiseWorkerProxy* aPromiseWorkerProxy);

 protected:
  ~AuthorizationCallback();

 private:
  RefPtr<Promise> mPromise;
  RefPtr<PromiseWorkerProxy> mPromiseWorkerProxy;
};

NS_IMPL_ISUPPORTS(AuthorizationCallback, nsIAuthorizationCallback)

AuthorizationCallback::AuthorizationCallback(Promise* aPromise)
    : mPromise(aPromise) {}

AuthorizationCallback::AuthorizationCallback(
    PromiseWorkerProxy* aPromiseWorkerProxy)
    : mPromiseWorkerProxy(aPromiseWorkerProxy) {}

AuthorizationCallback::~AuthorizationCallback() {}

NS_IMETHODIMP
AuthorizationCallback::OnRestrictedToken(nsresult aStatus,
                                         JS::HandleValue aResult,
                                         JSContext* aCx) {
  if (mPromise) {
    if (NS_SUCCEEDED(aStatus)) {
      mPromise->MaybeResolveWithClone(aCx, aResult);
    } else {
      mPromise->MaybeRejectWithClone(aCx, aResult);
    }
  } else if (mPromiseWorkerProxy) {
    MutexAutoLock lock(mPromiseWorkerProxy->Lock());
    if (mPromiseWorkerProxy->CleanedUp()) {
      return NS_OK;
    }

    WorkerPrivate* worker = mPromiseWorkerProxy->GetWorkerPrivate();
    RefPtr<OnAuthorizationWorkerRunnable> runnable =
        new OnAuthorizationWorkerRunnable(worker, mPromiseWorkerProxy.forget(),
                                          aStatus);

    ErrorResult rv;
    runnable->Write(aCx, aResult, rv);
    MOZ_ALWAYS_TRUE(runnable->Dispatch());
  }

  return NS_OK;
}

class AuthorizationRunnable : public Runnable {
 public:
  AuthorizationRunnable(PromiseWorkerProxy* aPromiseWorkerProxy,
                        nsCString aServiceType)
      : Runnable("dom::AuthorizationRunnable"),
        mPromiseWorkerProxy(aPromiseWorkerProxy),
        mServiceType(aServiceType) {}
  NS_IMETHOD
  Run() override {
    AssertIsOnMainThread();

    RefPtr<nsIAuthorizationCallback> callback =
        new AuthorizationCallback(mPromiseWorkerProxy);
    nsCOMPtr<nsIAuthorizationManager> authProxy =
        do_CreateInstance("@mozilla.org/kaiauth/authorization-manager;1");
    MOZ_ASSERT(authProxy);
    authProxy->GetRestrictedToken(mServiceType, callback);

    return NS_OK;
  }

 private:
  ~AuthorizationRunnable() = default;
  RefPtr<PromiseWorkerProxy> mPromiseWorkerProxy;
  nsCString mServiceType;
};

AuthorizationManager::AuthorizationManager(nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal) {
  MOZ_ASSERT(aGlobal);
}

AuthorizationManager::~AuthorizationManager() {}

already_AddRefed<Promise> AuthorizationManager::GetRestrictedToken(
    const KaiServiceType& aServiceType) {
  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(mGlobal, rv);
  ENSURE_SUCCESS(rv, nullptr);

  Span<const char> str = KaiServiceTypeValues::GetString(aServiceType);
  nsCString serviceType = nsCString(str.data(), str.size());

  if (NS_IsMainThread()) {
    RefPtr<nsIAuthorizationCallback> callback =
        new AuthorizationCallback(promise);
    if (!mAuthProxy) {
      mAuthProxy =
          do_CreateInstance("@mozilla.org/kaiauth/authorization-manager;1");
      MOZ_ASSERT(mAuthProxy);
    }
    mAuthProxy->GetRestrictedToken(serviceType, callback);
  } else {
    WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);
    worker->AssertIsOnWorkerThread();

    RefPtr<PromiseWorkerProxy> promiseWorkerProxy =
        PromiseWorkerProxy::Create(worker, promise);
    if (!promiseWorkerProxy) {
      promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
      return promise.forget();
    }

    RefPtr<AuthorizationRunnable> runnable =
        new AuthorizationRunnable(promiseWorkerProxy, serviceType);
    NS_DispatchToMainThread(runnable);
  }

  return promise.forget();
}

JSObject* AuthorizationManager::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return AuthorizationManager_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AuthorizationManager, mGlobal)
NS_IMPL_CYCLE_COLLECTING_ADDREF(AuthorizationManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AuthorizationManager)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AuthorizationManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

}  // namespace dom
}  // namespace mozilla
