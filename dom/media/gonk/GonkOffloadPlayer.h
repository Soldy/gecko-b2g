/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GonkOffloadPlayer_h_
#define GonkOffloadPlayer_h_

#include <media/mediaplayer.h>
#include <utils/StrongPointer.h>

#include "GonkNativeWindow.h"
#include "MediaOffloadPlayer.h"
#include "mozilla/Atomics.h"

namespace mozilla {

DDLoggedTypeDeclNameAndBase(GonkOffloadPlayer, MediaOffloadPlayer);

class GonkOffloadPlayer : public MediaOffloadPlayer,
                          public DecoderDoctorLifeLogger<GonkOffloadPlayer>,
                          public android::GonkNativeWindowNewFrameCallback {
  friend class AndroidMediaPlayerListener;

 public:
  GonkOffloadPlayer(MediaFormatReaderInit& aInit, nsIURI* aURI);
  virtual void OnNewFrame() override;

 private:
  ~GonkOffloadPlayer() = default;
  virtual void InitInternal() override;
  virtual void ResetInternal() override;
  virtual void SeekInternal(const SeekTarget& aTarget, bool aVisible) override;
  virtual bool UpdateCurrentPosition() override;
  virtual bool NeedToDeferSeek() override { return !mPrepared; }

  virtual void PlayStateChanged() override;
  virtual void VolumeChanged() override;
  virtual void PreservesPitchChanged() override { PlaybackSettingsChanged(); }
  virtual void LoopingChanged() override;
  virtual void PlaybackRateChanged() override { PlaybackSettingsChanged(); }

  void Notify(int msg, int ext1, int ext2);
  void NotifyMediaInfo(int ext1, int ext2);
  void NotifyPrepared();
  void PlaybackSettingsChanged();
  // Must be called after matadata is loaded.
  bool VideoEnabled() { return mInfo.HasVideo() && mVideoFrameContainer; }
  void UpdateAudibleState();

  bool mPrepared = false;
  bool mVisibleSeek = false;
  nsCOMPtr<nsIURI> mURI;
  Atomic<bool> mSentFirstFrameLoadedEvent;
  Atomic<int64_t> mPackedDisplaySize;  // To avoid adding mutex only for this.
  android::sp<android::MediaPlayer> mMediaPlayer;
  android::sp<android::MediaPlayerListener> mMediaPlayerListener;
  android::sp<android::GonkNativeWindow> mNativeWindow;
};

}  // namespace mozilla

#endif
