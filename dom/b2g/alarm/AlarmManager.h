/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AlarmManager_h
#define mozilla_dom_AlarmManager_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/AlarmManagerBinding.h"
#include "mozilla/dom/Promise.h"
#include "nsIAlarmProxy.h"
#include "nsWrapperCache.h"

/*
  - AlarmManager:
    The AlarmManager webidl implementation.
  - AlarmManagerImpl:
    The abstract class for AlarmManager to handle requests from both main
    threads and worker threads.
  - AlarmManagerMain:
    The AlarmManagerImpl implementation on main threads, creates an AlarmProxy
    instance by itself, and uses the AlarmProxy to send requests to
    AlarmService.
  - AlarmManagerWorker:
    The AlarmManagerImpl implementation on worker threads, cannot create
    AlarmProxy instances by itself, so dispatches all requests to main thread
    by runnables.
*/

namespace mozilla {
namespace dom {

class AlarmManagerImpl {
 public:
  explicit AlarmManagerImpl(nsIGlobalObject* aOuterGlobal)
      : mOuterGlobal(aOuterGlobal) {}
  virtual nsresult Init() = 0;
  virtual already_AddRefed<Promise> GetAll() = 0;
  virtual already_AddRefed<Promise> Add(JSContext* aCx,
                                        const AlarmOptions& aOptions) = 0;
  virtual void Remove(long aId) = 0;

  virtual bool CheckPermission() = 0;
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

 protected:
  nsCString mUrl;
  nsCOMPtr<nsIGlobalObject> mOuterGlobal;
};

class AlarmManagerMain final : public AlarmManagerImpl {
 public:
  NS_INLINE_DECL_REFCOUNTING(AlarmManagerMain, override)
  explicit AlarmManagerMain(nsIGlobalObject* aOuterGlobal);
  virtual nsresult Init() override;

  virtual already_AddRefed<Promise> GetAll() override;
  virtual already_AddRefed<Promise> Add(JSContext* aCx,
                                        const AlarmOptions& aOptions) override;
  virtual void Remove(long aId) override;

  bool CheckPermission() override;

 private:
  ~AlarmManagerMain() = default;
};

class AlarmManager final : public nsISupports,
                           public nsWrapperCache

{
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AlarmManager)

  static already_AddRefed<AlarmManager> Create(nsIGlobalObject* aGlobal,
                                               nsresult& aRv);

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  explicit AlarmManager(nsIGlobalObject* aGlobal);
  nsresult Init();
  bool CheckPermission();

  already_AddRefed<Promise> GetAll();
  already_AddRefed<Promise> Add(JSContext* aCx, const AlarmOptions& options);
  void Remove(long aId);

 protected:
  ~AlarmManager() = default;
  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<AlarmManagerImpl> mImpl;
};

namespace alarm {
already_AddRefed<nsIAlarmProxy> CreateAlarmProxy();
nsresult SetupUrlFromPrincipal(const nsCOMPtr<nsIPrincipal>& aPrincipal,
                               nsCString& aUrl);
bool DoCheckPermission(nsCString aUrl);
}  // namespace alarm
}  // namespace dom
}  // namespace mozilla

#endif
