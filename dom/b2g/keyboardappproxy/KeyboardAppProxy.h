/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_KeyboardAppProxy_h
#define mozilla_dom_KeyboardAppProxy_h

#include "nsCOMPtr.h"
#include "nsDeque.h"
#include "nsIKeyboardAppProxy.h"
#include "nsIWeakReferenceUtils.h"
#include "mozilla/WeakPtr.h"

namespace mozilla {
namespace dom {

class KeyboardAppProxy final : public nsIKeyboardAppProxy {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIKEYBOARDAPPPROXY

  static already_AddRefed<KeyboardAppProxy> GetInstance();

 private:
  class EventQueueDeallocator : public nsDequeFunctor<WidgetKeyboardEvent> {
    virtual void* operator()(void* aObject);
  };

  KeyboardAppProxy();
  ~KeyboardAppProxy();

  bool mIsActive;
  nsDeque<WidgetKeyboardEvent> mEventQueue;
  nsDeque<WidgetKeyboardEvent> mKeydownQueue;
  // Will be counted whenever keyboard app goes from active to inaction state.
  unsigned long long mGeneration;
  nsWeakPtr mKeyboardEventForwarder;

  // This object should be created or got from main thread only.
  static StaticRefPtr<KeyboardAppProxy> sSingleton;
  RefPtr<nsFrameLoader> mFrameLoader;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_KeyboardAppProxy_h
