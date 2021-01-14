/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/InputMethodHandler.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/InputMethodService.h"
#include "mozilla/dom/InputMethodServiceChild.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/IMELog.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(InputMethodHandler, nsIEditableSupportListener)

/*static*/
already_AddRefed<InputMethodHandler> InputMethodHandler::Create(
    Promise* aPromise) {
  IME_LOGD("--InputMethodHandler::Create");
  RefPtr<InputMethodHandler> handler = new InputMethodHandler(aPromise);

  return handler.forget();
}

/*static*/
already_AddRefed<InputMethodHandler> InputMethodHandler::Create() {
  IME_LOGD("--InputMethodHandler::Create without promise");
  RefPtr<InputMethodHandler> handler = new InputMethodHandler();

  return handler.forget();
}

InputMethodHandler::InputMethodHandler(Promise* aPromise)
    : mPromise(aPromise) {}

// nsIEditableSupportListener methods.
NS_IMETHODIMP
InputMethodHandler::OnSetComposition(uint32_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnSetComposition");
  if (mPromise) {
    if (NS_SUCCEEDED(aStatus)) {
      mPromise->MaybeResolve(JS::UndefinedHandleValue);
    } else {
      mPromise->MaybeReject(aStatus);
    }

    mPromise = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnEndComposition(uint32_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnEndComposition");
  if (mPromise) {
    if (NS_SUCCEEDED(aStatus)) {
      mPromise->MaybeResolve(JS::UndefinedHandleValue);
    } else {
      mPromise->MaybeReject(aStatus);
    }

    mPromise = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnKeydown(uint32_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnKeydown");
  if (mPromise) {
    if (NS_SUCCEEDED(aStatus)) {
      mPromise->MaybeResolve(JS::UndefinedHandleValue);
    } else {
      mPromise->MaybeReject(aStatus);
    }

    mPromise = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnKeyup(uint32_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnKeyup");
  if (mPromise) {
    if (NS_SUCCEEDED(aStatus)) {
      mPromise->MaybeResolve(JS::UndefinedHandleValue);
    } else {
      mPromise->MaybeReject(aStatus);
    }

    mPromise = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnSendKey(uint32_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnSendKey");
  if (mPromise) {
    if (NS_SUCCEEDED(aStatus)) {
      mPromise->MaybeResolve(JS::UndefinedHandleValue);
    } else {
      mPromise->MaybeReject(aStatus);
    }

    mPromise = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnDeleteBackward(uint32_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnDeleteBackward");
  if (mPromise) {
    if (NS_SUCCEEDED(aStatus)) {
      IME_LOGD("--InputMethodHandler::OnDeleteBackward: true");
      mPromise->MaybeResolve(true);
    } else {
      IME_LOGD("--InputMethodHandler::OnDeleteBackward: false");
      mPromise->MaybeReject(aStatus);
    }

    mPromise = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnSetSelectedOption(uint32_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnSetSelectedOption");
  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnSetSelectedOptions(uint32_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnSetSelectedOptions");
  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnRemoveFocus(uint32_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnRemoveFocus");
  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnGetSelectionRange(uint32_t aId, nsresult aStatus,
                                        int32_t aStart, int32_t aEnd) {
  IME_LOGD("--InputMethodHandler::OnGetSelectionRange");
  if (mPromise) {
    if (NS_SUCCEEDED(aStatus)) {
      IME_LOGD("--InputMethodHandler::OnGetSelectionRange: %lu %lu", aStart,
               aEnd);
      nsTArray<int32_t> range;
      range.AppendElement(aStart);
      range.AppendElement(aEnd);
      mPromise->MaybeResolve(range);
    } else {
      IME_LOGD("--InputMethodHandler::OnGetSelectionRange failed");
      mPromise->MaybeReject(aStatus);
    }
    mPromise = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnGetText(uint32_t aId, nsresult aStatus,
                              const nsAString& aText) {
  IME_LOGD("--InputMethodHandler::OnGetText");
  if (mPromise) {
    if (NS_SUCCEEDED(aStatus)) {
      IME_LOGD("--InputMethodHandler::OnGetText:[%s]",
               NS_ConvertUTF16toUTF8(aText).get());
      nsString text(aText);
      mPromise->MaybeResolve(text);
    } else {
      IME_LOGD("--InputMethodHandler::OnGetText failed");
      mPromise->MaybeReject(aStatus);
    }
    mPromise = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnSetValue(uint32_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnSetValue");
  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnClearAll(uint32_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnClearAll");
  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnReplaceSurroundingText(uint32_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnReplaceSurroundingText");
  if (mPromise) {
    if (NS_SUCCEEDED(aStatus)) {
      mPromise->MaybeResolve(JS::UndefinedHandleValue);
    } else {
      mPromise->MaybeReject(aStatus);
    }
    mPromise = nullptr;
  }
  return NS_OK;
}

nsresult InputMethodHandler::SetComposition(const nsAString& aText) {
  nsString text(aText);
  // TODO use a pure interface, and make it point to either the remote version
  // or the local version at Listener's creation.
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (contentChild) {
    IME_LOGD("--InputMethodHandler::SetComposition content process");
    // Call from content process.
    SendRequest(contentChild, SetCompositionRequest(0, text));
  } else {
    IME_LOGD("--InputMethodHandler::SetComposition in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->SetComposition(0, this, aText);
  }
  return NS_OK;
}

nsresult InputMethodHandler::EndComposition(const nsAString& aText) {
  nsString text(aText);
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (contentChild) {
    IME_LOGD("--InputMethodHandler::EndComposition content process");
    // Call from content process.
    SendRequest(contentChild, EndCompositionRequest(0, text));
  } else {
    IME_LOGD("--InputMethodHandler::EndComposition in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->EndComposition(0, this, aText);
  }
  return NS_OK;
}

nsresult InputMethodHandler::Keydown(const nsAString& aKey) {
  nsString key(aKey);
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (contentChild) {
    IME_LOGD("--InputMethodHandler::Keydown content process");
    // Call from content process.
    SendRequest(contentChild, KeydownRequest(0, key));
  } else {
    IME_LOGD("--InputMethodHandler::Keydown in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->Keydown(0, this, aKey);
  }
  return NS_OK;
}

nsresult InputMethodHandler::Keyup(const nsAString& aKey) {
  nsString key(aKey);
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (contentChild) {
    IME_LOGD("--InputMethodHandler::Keyup content process");
    // Call from content process.
    SendRequest(contentChild, KeyupRequest(0, key));
  } else {
    IME_LOGD("--InputMethodHandler::Keyup in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->Keyup(0, this, aKey);
  }
  return NS_OK;
}

nsresult InputMethodHandler::SendKey(const nsAString& aKey) {
  nsString key(aKey);
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (contentChild) {
    IME_LOGD("--InputMethodHandler::SendKey content process");
    // Call from content process.
    SendRequest(contentChild, SendKeyRequest(0, key));
  } else {
    IME_LOGD("--InputMethodHandler::SendKey in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->SendKey(0, this, aKey);
  }
  return NS_OK;
}

nsresult InputMethodHandler::DeleteBackward() {
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (contentChild) {
    IME_LOGD("--InputMethodHandler::DeleteBackward content process");
    // Call from content process.
    SendRequest(contentChild, DeleteBackwardRequest(0));
  } else {
    IME_LOGD("--InputMethodHandler::DeleteBackward in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->DeleteBackward(0, this);
  }
  return NS_OK;
}

void InputMethodHandler::RemoveFocus() {
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (contentChild) {
    IME_LOGD("--InputMethodHandler::RemoveFocus content process");
    // Call from content process.
    CommonRequest request(0, u"RemoveFocus"_ns);
    SendRequest(contentChild, request);
  } else {
    IME_LOGD("--InputMethodHandler::RemoveFocus in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->RemoveFocus(0, this);
  }
}

nsresult InputMethodHandler::GetSelectionRange() {
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (contentChild) {
    IME_LOGD("--InputMethodHandler::GetSelectionRange content process");
    // Call from content process.
    SendRequest(contentChild, GetSelectionRangeRequest());
  } else {
    IME_LOGD("--InputMethodHandler::GetSelectionRange in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->GetSelectionRange(0, this);
  }
  return NS_OK;
}

nsresult InputMethodHandler::GetText(int32_t aOffset, int32_t aLength) {
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (contentChild) {
    IME_LOGD("--InputMethodHandler::GetText content process");
    // Call from content process.
    GetTextRequest request(0, aOffset, aLength);
    SendRequest(contentChild, request);
  } else {
    IME_LOGD("--InputMethodHandler::GetText in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->GetText(0, this, aOffset, aLength);
  }
  return NS_OK;
}

void InputMethodHandler::SetValue(const nsAString& aValue) {
  nsString value(aValue);
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (contentChild) {
    IME_LOGD("--InputMethodHandler::SetValue content process");
    // Call from content process.
    SendRequest(contentChild, SetValueRequest(0, value));
  } else {
    IME_LOGD("--InputMethodHandler::SetValue in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->SetValue(0, this, aValue);
  }
}

void InputMethodHandler::ClearAll() {
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (contentChild) {
    IME_LOGD("--InputMethodHandler::ClearAll content process");
    // Call from content process.
    CommonRequest request(0, u"ClearAll"_ns);
    SendRequest(contentChild, request);
  } else {
    IME_LOGD("--InputMethodHandler::ClearAll in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->ClearAll(0, this);
  }
}

nsresult InputMethodHandler::ReplaceSurroundingText(const nsAString& aText,
                                                    int32_t aOffset,
                                                    int32_t aLength) {
  nsString text(aText);
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (contentChild) {
    IME_LOGD("--InputMethodHandler::ReplaceSurroundingText content process");
    ReplaceSurroundingTextRequest request(0, text, aOffset, aLength);
    SendRequest(contentChild, request);
  } else {
    IME_LOGD("--InputMethodHandler::ReplaceSurroundingText in-process");
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->ReplaceSurroundingText(0, this, aText, aOffset, aLength);
  }
  return NS_OK;
}

void InputMethodHandler::SendRequest(ContentChild* aContentChild,
                                     const InputMethodRequest& aRequest) {
  InputMethodServiceChild* child = new InputMethodServiceChild(this);
  aContentChild->SendPInputMethodServiceConstructor(child);
  child->SendRequest(aRequest);
}

}  // namespace dom
}  // namespace mozilla
