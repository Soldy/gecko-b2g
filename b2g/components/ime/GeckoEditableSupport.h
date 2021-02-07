/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_GeckoEditableSupport_h
#define mozilla_widget_GeckoEditableSupport_h

#include "nsIWidget.h"
#include "nsTArray.h"

#include "mozilla/dom/InputMethodServiceChild.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextEventDispatcherListener.h"
#include "mozilla/UniquePtr.h"
#include "nsIEditableSupport.h"
#include "nsIDOMEventListener.h"
#include "nsIObserver.h"

class nsWindow;
class nsIGlobalObject;
class nsPIDOMWindowOuter;

namespace mozilla {

class TextComposition;

namespace dom {
class BrowserChild;
class Promise;
class nsInputContext;
class EventTarget;
}  // namespace dom

namespace widget {

class GeckoEditableSupport final : public TextEventDispatcherListener,
                                   public nsIEditableSupport,
                                   public nsIDOMEventListener,
                                   public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEDITABLESUPPORT
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIOBSERVER

  explicit GeckoEditableSupport(nsPIDOMWindowOuter* aDOMWindow);

  // TextEventDispatcherListener methods
  NS_IMETHOD NotifyIME(TextEventDispatcher* aTextEventDispatcher,
                       const IMENotification& aNotification) override;

  NS_IMETHOD_(IMENotificationRequests) GetIMENotificationRequests() override {
    return IMENotificationRequests(
        IMENotificationRequests::NOTIFY_TEXT_CHANGE |
        IMENotificationRequests::NOTIFY_POSITION_CHANGE);
  }

  NS_IMETHOD_(void)
  OnRemovedFrom(TextEventDispatcher* aTextEventDispatcher) override {}

  NS_IMETHOD_(void)
  WillDispatchKeyboardEvent(TextEventDispatcher* aTextEventDispatcher,
                            WidgetKeyboardEvent& aKeyboardEvent,
                            uint32_t aIndexOfKeypress, void* aData) override {}

 protected:
  virtual ~GeckoEditableSupport();
  void HandleFocus();
  void HandleBlur();
  nsresult GetInputContextBag(dom::nsInputContext* aInputContext);
  void HandleTextChanged();

 private:
  void EnsureServiceChild();

  RefPtr<TextEventDispatcher> mDispatcher;
  dom::InputMethodServiceChild* mServiceChild;
  nsCOMPtr<dom::EventTarget> mChromeEventHandler;

  // We need this flag to remember whether we already send focus to IME. Can't
  // fully rely on focus/blur event due to the blur event may be suppressed in
  // some cases (e.g. content removed)
  bool mIsFocused;
  bool mIsVoiceInputEnabled;
  nsTArray<nsString> mVoiceInputSupportedTypes;
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_GeckoEditableSupport_h
