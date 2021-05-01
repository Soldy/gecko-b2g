/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AlarmManagerWorker_h
#define mozilla_dom_AlarmManagerWorker_h

#include "mozilla/dom/AlarmManager.h"

namespace mozilla {
namespace dom {

class AlarmManagerWorker final : public AlarmManagerImpl {
 public:
  NS_INLINE_DECL_REFCOUNTING(AlarmManagerWorker, override)
  explicit AlarmManagerWorker(nsIGlobalObject* aOuterGlobal);
  virtual nsresult Init() override;

  virtual already_AddRefed<Promise> GetAll() override;
  virtual already_AddRefed<Promise> Add(JSContext* aCx,
                                        const AlarmOptions& aOptions) override;
  virtual void Remove(long aId) override;

  virtual bool CheckPermission() override;

 private:
  ~AlarmManagerWorker() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif
