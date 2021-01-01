/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "KeyboardAppProxy.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/TextEvents.h"
#include "nsFrameLoader.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/BrowserParent.h"
#include "KeyboardEventForwarderParent.h"

mozilla::LazyLogModule gKeyboardAppProxyLog("KeyboardAppProxy");

namespace mozilla {
namespace dom {

void* KeyboardAppProxy::EventQueueDeallocator::operator()(void* aObject) {
  if (aObject) {
    delete static_cast<WidgetKeyboardEvent*>(aObject);
  }
  return nullptr;
}

NS_IMPL_ISUPPORTS(KeyboardAppProxy, nsIKeyboardAppProxy)

StaticRefPtr<KeyboardAppProxy> KeyboardAppProxy::sSingleton;

KeyboardAppProxy::KeyboardAppProxy() : mIsActive(false), mGeneration(0) {}

KeyboardAppProxy::~KeyboardAppProxy() {
  mEventQueue.Erase();
  mKeydownQueue.Erase();
}

NS_IMETHODIMP
KeyboardAppProxy::MaybeForwardKey(WidgetKeyboardEvent* aEvent,
                                  bool* aIsForwarded) {
  NS_ENSURE_ARG(aEvent);
  *aIsForwarded = false;
  if (aEvent->mHandledByIME) {
    return NS_OK;
  }

  if (!mIsActive || !mKeyboardEventForwarder) {
    return NS_OK;
  }

  if (aEvent->mMessage != eKeyDown && aEvent->mMessage != eKeyPress &&
      aEvent->mMessage != eKeyUp) {
    MOZ_ASSERT_UNREACHABLE("It should be only keyup/down/press");
    return NS_ERROR_UNEXPECTED;
  }

  nsCString keyType;
  switch (aEvent->mMessage) {
    case eKeyDown:
      keyType = "keydown"_ns;
      mKeydownQueue.Push(new WidgetKeyboardEvent(*aEvent));
      break;
    case eKeyPress:
      keyType = "keypress"_ns;
      break;
    case eKeyUp:
      keyType = "keyup"_ns;
      break;
    default:
      return NS_ERROR_UNEXPECTED;
  }

  nsString key;
  aEvent->GetDOMKeyName(key);
  MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
          ("SendKey => keyType: [%s], DOMKeyName: [%s]", keyType.get(),
           NS_ConvertUTF16toUTF8(key).get()));
  nsCOMPtr<nsIKeyboardEventForwarder> forwarder =
      do_QueryReferent(mKeyboardEventForwarder);
  if (!forwarder) {
    return NS_OK;
  }
  forwarder->OnKeyboardEventReceived(
      keyType, aEvent->mKeyCode, aEvent->mCharCode, NS_ConvertUTF16toUTF8(key),
      aEvent->mTime, mGeneration);
  mEventQueue.Push(new WidgetKeyboardEvent(*aEvent));
  *aIsForwarded = true;
  return NS_OK;
}

NS_IMETHODIMP
KeyboardAppProxy::Activate(nsFrameLoader* aFrameLoader) {
  if (!aFrameLoader) {
    MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
            ("Activate fail due to no valid frameLoader"));
    return NS_ERROR_FAILURE;
  }

  if (mIsActive) {
    if (mKeyboardEventForwarder) {
      // Double active, just ignore.
      return NS_OK;
    } else {
      // State does not match weakPtr we hold.
      MOZ_ASSERT_UNREACHABLE(
          "Active state without valid mKeyboardEventForwarder.");
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (BrowserParent* browser = aFrameLoader->GetBrowserParent()) {
    mFrameLoader = aFrameLoader;
    RefPtr<KeyboardEventForwarderParent> actor =
        static_cast<KeyboardEventForwarderParent*>(
            browser->SendPKeyboardEventForwarderConstructor());
    mKeyboardEventForwarder = do_GetWeakReference(actor);
  }

  MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug, ("Activate"));
  // Advance event generation and empty event queue. Instead of doing such
  // clean up at set inactive, we perform it at set active for the reason if
  // target app destroys itself at keydown, System app still expect handling
  // the keyup of that key.
  mGeneration++;
  mEventQueue.Erase();
  mKeydownQueue.Erase();

  mIsActive = true;
  return NS_OK;
}

NS_IMETHODIMP
KeyboardAppProxy::Deactivate() {
  if (!mIsActive) {
    return NS_OK;
  }
  MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug, ("Deactivate"));
  mIsActive = false;
  nsCOMPtr<nsIKeyboardEventForwarder> forwarder =
      do_QueryReferent(mKeyboardEventForwarder);
  if (!forwarder) {
    return NS_OK;
  }
  RefPtr<KeyboardEventForwarderParent> actor =
      static_cast<KeyboardEventForwarderParent*>(
          static_cast<nsIKeyboardEventForwarder*>(forwarder));
  Unused << actor->Send__delete__(actor);
  mKeyboardEventForwarder = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
KeyboardAppProxy::ReplyKey(const nsACString& aEventType,
                           bool aDefaultPreventedStatus, uint64_t aGeneration) {
  MOZ_ASSERT(mEventQueue.GetSize() != 0, "queue should not be empty");

  // Skip if the key replied does't belong to the current generation
  if (mGeneration != aGeneration) {
    return NS_OK;
  }

  if (mEventQueue.GetSize() == 0) {
    return NS_ERROR_FAILURE;
  }

  UniquePtr<WidgetKeyboardEvent> keyboardEvent(mEventQueue.PopFront());

  if (aEventType.Equals("keyup"_ns)) {
    UniquePtr<WidgetKeyboardEvent> keydownEvent(mKeydownQueue.PopFront());

    // Theorectically a keyup should pair with a keydown, if a keyup is sent to
    // IME but no keydown was sent to IME previously, consume and do not
    // dispatch this keyup event to the target chain.
    if (!keydownEvent) {
      aDefaultPreventedStatus = true;
    }
  }

  // Keyboard app doesn't take this event so send it to the target and finish
  // postHandleEVent.
  if (!aDefaultPreventedStatus) {
    if (!mFrameLoader) {
      MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
              ("KeyboardAppProxy::ReplyKey no mFrameLoader"));
      return NS_OK;
    }

    Document* doc = mFrameLoader->GetOwnerDoc();
    if (!doc) {
      MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
              ("KeyboardAppProxy::ReplyKey no doc"));
      return NS_OK;
    }

    PresShell* presShell = doc->GetPresShell();
    if (!presShell) {
      MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
              ("KeyboardAppProxy::ReplyKey no presShell"));
      return NS_OK;
    }

    nsIFrame* frame = presShell->GetRootScrollFrame();
    if (!frame) {
      MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
              ("KeyboardAppProxy::ReplyKey no frame"));
      return NS_OK;
    }
    nsEventStatus result = nsEventStatus_eIgnore;
    keyboardEvent->mHandledByIME = true;
    presShell->HandleEvent(frame, keyboardEvent.get(), false, &result);
  }
  return NS_OK;
}

/* static */ already_AddRefed<KeyboardAppProxy>
KeyboardAppProxy::GetInstance() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!sSingleton) {
    sSingleton = new KeyboardAppProxy();
    ClearOnShutdown(&sSingleton);
  }

  RefPtr<KeyboardAppProxy> service = sSingleton.get();
  return service.forget();
}

}  // namespace dom
}  // namespace mozilla
