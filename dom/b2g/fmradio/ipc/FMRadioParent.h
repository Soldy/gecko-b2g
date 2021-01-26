/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradioparent_h__
#define mozilla_dom_fmradioparent_h__

#include "FMRadioCommon.h"
#include "mozilla/dom/PFMRadioParent.h"

BEGIN_FMRADIO_NAMESPACE

class PFMRadioRequestParent;

class FMRadioParent final : public PFMRadioParent, public FMRadioEventObserver {
 public:
  FMRadioParent();
  ~FMRadioParent();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvGetStatusInfo(StatusInfo* aStatusInfo);

  PFMRadioRequestParent* AllocPFMRadioRequestParent(
      const FMRadioRequestArgs& aArgs);

  bool DeallocPFMRadioRequestParent(PFMRadioRequestParent* aActor);

  /* FMRadioEventObserver */
  virtual void Notify(const FMRadioEventType& aType) override;

  mozilla::ipc::IPCResult RecvEnableAudio(const bool& aAudioEnabled);

  mozilla::ipc::IPCResult RecvSetVolume(const float& aVolume);

  mozilla::ipc::IPCResult RecvSetRDSGroupMask(const uint32_t& aRDSGroupMask);
};

END_FMRADIO_NAMESPACE

#endif  // mozilla_dom_fmradioparent_h__
