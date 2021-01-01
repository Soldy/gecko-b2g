/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InputMethodHandler_h
#define mozilla_dom_InputMethodHandler_h

#include "mozilla/AlreadyAddRefed.h"
#include "nsIEditableSupport.h"

namespace mozilla {
namespace dom {

class Promise;
class InputMethodRequest;

class InputMethodHandler final : public nsIEditableSupportListener {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEDITABLESUPPORTLISTENER

  static already_AddRefed<InputMethodHandler> Create(Promise* aPromise);
  static already_AddRefed<InputMethodHandler> Create();

  nsresult SetComposition(const nsAString& aText);
  nsresult EndComposition(const nsAString& aText);
  nsresult Keydown(const nsAString& aKey);
  nsresult Keyup(const nsAString& aKey);
  nsresult SendKey(const nsAString& aKey);
  nsresult DeleteBackward();

  void RemoveFocus();
  nsresult GetSelectionRange();
  nsresult GetText(int32_t aOffset, int32_t aLength);
  void SetValue(const nsAString& aValue);
  void ClearAll();

 private:
  explicit InputMethodHandler(Promise* aPromise);
  InputMethodHandler() = default;
  ~InputMethodHandler() = default;

  void Initialize();
  void SendRequest(ContentChild* aContentChild,
                   const InputMethodRequest& aRequest);
  RefPtr<Promise> mPromise;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_InputMethodHandler_h
