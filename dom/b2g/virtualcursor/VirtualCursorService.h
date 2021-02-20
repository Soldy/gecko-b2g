/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VirtualCursorService_h
#define mozilla_dom_VirtualCursorService_h

#include "nsIVirtualCursorService.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsIDOMWindowUtils.h"
#include "nsFrameLoader.h"
#include "mozilla/dom/PanSimulator.h"
#include "VirtualCursorProxy.h"

namespace mozilla {
namespace dom {

class nsIVirtualCursor;
class VirtualCursorProxy;

// Manages the VirtualCursorProxy objects for each outer window and forward
// mouse events.
class VirtualCursorService final : public nsIVirtualCursor,
                                   public nsIVirtualCursorService {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVIRTUALCURSORSERVICE

  static already_AddRefed<VirtualCursorService> GetService();
  static already_AddRefed<VirtualCursorProxy> GetOrCreateCursor(
      nsPIDOMWindowOuter* aWindow);
  static already_AddRefed<VirtualCursorProxy> GetCursor(
      nsPIDOMWindowOuter* aWindow);

  static void RemoveCursor(nsPIDOMWindowOuter* aWindow);
  static void Shutdown();

  virtual void UpdatePos(const LayoutDeviceIntPoint& aPoint) override;
  virtual void CursorClick() override;
  virtual void CursorDown() override;
  virtual void CursorUp() override;
  virtual void CursorMove() override;
  virtual void CursorOut(bool aCheckActive = false) override;
  virtual void ShowContextMenu() override;
  void SendCursorEvent(const nsAString& aType, int32_t aButton,
                       int32_t aButtons);

  void StartPanning();
  void StopPanning();
  bool IsPanning();

 private:
  VirtualCursorService() = default;
  virtual ~VirtualCursorService();

  nsCOMPtr<nsPIDOMWindowOuter> mWindow;
  nsCOMPtr<nsIDOMWindowUtils> mWindowUtils;
  RefPtr<PanSimulator> mPanSimulator;
  CSSPoint mCSSCursorPoint;

  // A table to map outer window to the VirtualCursorProxy
  nsDataHashtable<nsPtrHashKey<nsPIDOMWindowOuter>, RefPtr<VirtualCursorProxy>>
      mCursorMap;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_VirtualCursorService_h
