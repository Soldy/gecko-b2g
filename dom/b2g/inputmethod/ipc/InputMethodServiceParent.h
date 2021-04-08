/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InputMethodServiceParent_h
#define mozilla_dom_InputMethodServiceParent_h

#include "mozilla/dom/PInputMethodServiceParent.h"
#include "InputMethodServiceCommon.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::IPCResult;

class InputMethodServiceParent final
    : public InputMethodServiceCommon<PInputMethodServiceParent>,
      public nsIEditableSupport {
  friend class PInputMethodServiceParent;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEDITABLESUPPORT

  InputMethodServiceParent();

  nsIEditableSupport* GetEditableSupport() override;
  nsIEditableSupportListener* GetEditableSupportListener(uint32_t aId) override;

 protected:
  IPCResult RecvRequest(const InputMethodRequest& aRequest);
  IPCResult RecvResponse(const InputMethodResponse& aResponse) {
    IPCResult result =
        InputMethodServiceCommon<PInputMethodServiceParent>::RecvResponse(
            aResponse);

    switch (aResponse.type()) {
      case InputMethodResponse::TSetCompositionResponse: {
        mRequestMap.Remove(((const SetCompositionResponse&)aResponse).id());
        break;
      }
      case InputMethodResponse::TEndCompositionResponse: {
        mRequestMap.Remove(((const EndCompositionResponse&)aResponse).id());
        break;
      }
      case InputMethodResponse::TKeydownResponse: {
        mRequestMap.Remove(((const KeydownResponse&)aResponse).id());
        break;
      }
      case InputMethodResponse::TKeyupResponse: {
        mRequestMap.Remove(((const KeyupResponse&)aResponse).id());
        break;
      }
      case InputMethodResponse::TSendKeyResponse: {
        mRequestMap.Remove(((const SendKeyResponse&)aResponse).id());
        break;
      }
      case InputMethodResponse::TDeleteBackwardResponse: {
        mRequestMap.Remove(((const DeleteBackwardResponse&)aResponse).id());
        break;
      }
      case InputMethodResponse::TSetSelectedOptionResponse: {
        mRequestMap.Remove(((const SetSelectedOptionResponse&)aResponse).id());
        break;
      }
      case InputMethodResponse::TSetSelectedOptionsResponse: {
        mRequestMap.Remove(((const SetSelectedOptionsResponse&)aResponse).id());
        break;
      }
      case InputMethodResponse::TCommonResponse: {
        mRequestMap.Remove(((const CommonResponse&)aResponse).id());
        break;
      }
      case InputMethodResponse::TGetSelectionRangeResponse: {
        mRequestMap.Remove(((const GetSelectionRangeResponse&)aResponse).id());
        break;
      }
      case InputMethodResponse::TGetTextResponse: {
        mRequestMap.Remove(((const GetTextResponse&)aResponse).id());
        break;
      }
      default:
        break;
    }
    return result;
  }
  void ActorDestroy(ActorDestroyReason why) override;

 private:
  ~InputMethodServiceParent();
  nsTHashMap<nsUint32HashKey, RefPtr<nsIEditableSupportListener>> mRequestMap;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_InputMethodServiceParent_h
