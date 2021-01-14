/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaOffloadPlayer.h"

#include "GonkOffloadPlayer.h"
#include "MediaTrackGraph.h"

namespace mozilla {

using namespace mozilla::media;
using namespace mozilla::layers;

#define LOG(fmt, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Debug, (fmt, ##__VA_ARGS__))
#define LOGV(fmt, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Verbose, (fmt, ##__VA_ARGS__))

/* static */
RefPtr<MediaOffloadPlayer> MediaOffloadPlayer::Create(
    MediaFormatReaderInit& aInit, nsIURI* aURI) {
  return new GonkOffloadPlayer(aInit, aURI);
}

#define INIT_MIRROR(name, val) \
  name(mTaskQueue, val, "MediaOffloadPlayer::" #name " (Mirror)")
#define INIT_CANONICAL(name, val) \
  name(mTaskQueue, val, "MediaOffloadPlayer::" #name " (Canonical)")

MediaOffloadPlayer::MediaOffloadPlayer(MediaFormatReaderInit& aInit)
    : mTaskQueue(new TaskQueue(GetMediaThreadPool(MediaThreadType::MDSM),
                               "MediaOffloadPlayer::mTaskQueue",
                               /* aSupportsTailDispatch = */ true)),
      mWatchManager(this, mTaskQueue),
      mCurrentPositionTimer(mTaskQueue, true /*aFuzzy*/),
      mDormantTimer(mTaskQueue, true /*aFuzzy*/),
      mVideoFrameContainer(aInit.mVideoFrameContainer),
      mResource(aInit.mResource),
      INIT_CANONICAL(mBuffered, TimeIntervals()),
      INIT_CANONICAL(mDuration, NullableTimeUnit()),
      INIT_CANONICAL(mCurrentPosition, TimeUnit::Zero()),
      INIT_CANONICAL(mIsAudioDataAudible, false),
      INIT_MIRROR(mPlayState, MediaDecoder::PLAY_STATE_LOADING),
      INIT_MIRROR(mVolume, 1.0),
      INIT_MIRROR(mPreservesPitch, true),
      INIT_MIRROR(mLooping, false),
      INIT_MIRROR(mSinkDevice, nullptr),
      INIT_MIRROR(mSecondaryVideoContainer, nullptr),
      INIT_MIRROR(mOutputCaptureState, MediaDecoder::OutputCaptureState::None),
      INIT_MIRROR(mOutputTracks, nsTArray<RefPtr<ProcessedMediaTrack>>()),
      INIT_MIRROR(mOutputPrincipal, PRINCIPAL_HANDLE_NONE) {
  LOG("MediaOffloadPlayer::MediaOffloadPlayer");
  DDLINKCHILD("source", mResource.get());
}

#undef INIT_MIRROR
#undef INIT_CANONICAL

MediaOffloadPlayer::~MediaOffloadPlayer() {
  LOG("MediaOffloadPlayer::~MediaOffloadPlayer");
}

void MediaOffloadPlayer::InitializationTask(MediaDecoder* aDecoder) {
  AUTO_PROFILER_LABEL("MediaOffloadPlayer::InitializationTask", MEDIA_PLAYBACK);
  MOZ_ASSERT(OnTaskQueue());
  LOG("MediaOffloadPlayer::InitializationTask");

  // Connect mirrors.
  mPlayState.Connect(aDecoder->CanonicalPlayState());
  mVolume.Connect(aDecoder->CanonicalVolume());
  mPreservesPitch.Connect(aDecoder->CanonicalPreservesPitch());
  mLooping.Connect(aDecoder->CanonicalLooping());
  mSinkDevice.Connect(aDecoder->CanonicalSinkDevice());
  mSecondaryVideoContainer.Connect(
      aDecoder->CanonicalSecondaryVideoContainer());
  mOutputCaptureState.Connect(aDecoder->CanonicalOutputCaptureState());
  mOutputTracks.Connect(aDecoder->CanonicalOutputTracks());
  mOutputPrincipal.Connect(aDecoder->CanonicalOutputPrincipal());

  // Initialize watchers.
  mWatchManager.Watch(mPlayState, &MediaOffloadPlayer::PlayStateChanged);
  mWatchManager.Watch(mVolume, &MediaOffloadPlayer::VolumeChanged);
  mWatchManager.Watch(mPreservesPitch,
                      &MediaOffloadPlayer::PreservesPitchChanged);
  mWatchManager.Watch(mLooping, &MediaOffloadPlayer::LoopingChanged);
  mWatchManager.Watch(mSecondaryVideoContainer,
                      &MediaOffloadPlayer::UpdateSecondaryVideoContainer);
  mWatchManager.Watch(mOutputCaptureState,
                      &MediaOffloadPlayer::UpdateOutputCaptureState);
  mWatchManager.Watch(mOutputTracks, &MediaOffloadPlayer::OutputTracksChanged);
  mWatchManager.Watch(mOutputPrincipal,
                      &MediaOffloadPlayer::OutputPrincipalChanged);

  mTransportSeekable = aDecoder->IsTransportSeekable();
  mAudioChannel = aDecoder->GetAudioChannel();
  mContainerType = aDecoder->ContainerType().Type().AsString();

  InitInternal();
}

nsresult MediaOffloadPlayer::Init(MediaDecoder* aDecoder) {
  MOZ_ASSERT(NS_IsMainThread());

  // Dispatch initialization that needs to happen on that task queue.
  OwnerThread()->DispatchStateChange(NewRunnableMethod<RefPtr<MediaDecoder>>(
      "MediaOffloadPlayer::InitializationTask", this,
      &MediaOffloadPlayer::InitializationTask, aDecoder));
  return NS_OK;
}

RefPtr<ShutdownPromise> MediaOffloadPlayer::Shutdown() {
  MOZ_ASSERT(OnTaskQueue());
  LOG("MediaOffloadPlayer::Shutdown");

  mCurrentPositionTimer.Reset();
  mDormantTimer.Reset();
  ResetInternal();

  // Disconnect canonicals and mirrors before shutting down our task queue.
  mPlayState.DisconnectIfConnected();
  mVolume.DisconnectIfConnected();
  mPreservesPitch.DisconnectIfConnected();
  mLooping.DisconnectIfConnected();
  mSinkDevice.DisconnectIfConnected();
  mSecondaryVideoContainer.DisconnectIfConnected();
  mOutputCaptureState.DisconnectIfConnected();
  mOutputTracks.DisconnectIfConnected();
  mOutputPrincipal.DisconnectIfConnected();

  mBuffered.DisconnectAll();
  mDuration.DisconnectAll();
  mCurrentPosition.DisconnectAll();
  mIsAudioDataAudible.DisconnectAll();

  // Shut down the watch manager to stop further notifications.
  mWatchManager.Shutdown();
  return OwnerThread()->BeginShutdown();
}

RefPtr<ShutdownPromise> MediaOffloadPlayer::BeginShutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  return InvokeAsync(OwnerThread(), this, __func__,
                     &MediaOffloadPlayer::Shutdown);
}

void MediaOffloadPlayer::UpdateCurrentPositionPeriodically() {
  bool scheduleNextUpdate = UpdateCurrentPosition();
  if (!scheduleNextUpdate) {
    return;
  }

  TimeStamp target = TimeStamp::Now() + TimeDuration::FromMilliseconds(250);
  mCurrentPositionTimer.Ensure(
      target,
      [this, self = RefPtr<MediaOffloadPlayer>(this)]() {
        mCurrentPositionTimer.CompleteRequest();
        UpdateCurrentPositionPeriodically();
      },
      []() { MOZ_DIAGNOSTIC_ASSERT(false); });
}

RefPtr<GenericPromise> MediaOffloadPlayer::RequestDebugInfo(
    dom::MediaDecoderStateMachineDebugInfo& aInfo) {
  return GenericPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
}

void MediaOffloadPlayer::NotifySeeked(bool aSuccess) {
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mCurrentSeek.Exists());

  LOG("MediaOffloadPlayer::NotifySeeked, %s seek to %.03lf sec %s",
      mCurrentSeek.mVisible ? "user" : "internal",
      mCurrentSeek.mTarget->GetTime().ToSeconds(),
      aSuccess ? "succeeded" : "failed");

  if (aSuccess) {
    mCurrentSeek.mPromise.ResolveIfExists(true, __func__);
  } else {
    mCurrentSeek.mPromise.RejectIfExists(true, __func__);
  }

  // If there is a pending seek job, dispatch it to another runnable.
  if (mPendingSeek.Exists()) {
    mCurrentSeek = std::move(mPendingSeek);
    mPendingSeek = SeekObject();
    nsresult rv = OwnerThread()->Dispatch(NS_NewRunnableFunction(
        "MediaOffloadPlayer::SeekPendingJob",
        [this, self = RefPtr<MediaOffloadPlayer>(this)]() {
          SeekInternal(mCurrentSeek.mTarget.ref(), mCurrentSeek.mVisible);
        }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  } else {
    mCurrentSeek = SeekObject();
    // We may just exit from dormant state, so manually call PlayStateChanged()
    // to enter playing/paused state correctly.
    PlayStateChanged();
  }
}

RefPtr<MediaDecoder::SeekPromise> MediaOffloadPlayer::HandleSeek(
    const SeekTarget& aTarget, bool aVisible) {
  MOZ_ASSERT(OnTaskQueue());

  LOG("MediaOffloadPlayer::HandleSeek, %s seek to %.03lf sec, type %d",
      aVisible ? "user" : "internal", aTarget.GetTime().ToSeconds(),
      aTarget.GetType());

  // If user seeks to a new position, need to exit dormant and fire seek so the
  // displayed image will be updated.
  if (aVisible && mVideoFrameContainer) {
    ExitDormant();
  }

  // If we are currently seeking or we need to defer seeking, save the job in
  // mPendingSeek. If there is already a pending seek job, reject it and replace
  // it by the new one.
  if (mCurrentSeek.Exists() || NeedToDeferSeek()) {
    LOG("MediaOffloadPlayer::HandleSeek, cannot seek now, replacing previous "
        "pending target %.03lf sec",
        mPendingSeek.Exists() ? mPendingSeek.mTarget->GetTime().ToSeconds()
                              : UnspecifiedNaN<double>());
    mPendingSeek.RejectIfExists(__func__);
    mPendingSeek.mTarget.emplace(aTarget);
    mPendingSeek.mVisible = aVisible;
    return mPendingSeek.mPromise.Ensure(__func__);
  }

  MOZ_ASSERT(!mPendingSeek.Exists());
  mPendingSeek = SeekObject();
  mCurrentSeek.mTarget.emplace(aTarget);
  mCurrentSeek.mVisible = aVisible;
  RefPtr<MediaDecoder::SeekPromise> p = mCurrentSeek.mPromise.Ensure(__func__);
  SeekInternal(aTarget, aVisible);
  return p;
}

RefPtr<MediaDecoder::SeekPromise> MediaOffloadPlayer::InvokeSeek(
    const SeekTarget& aTarget) {
  return InvokeAsync(OwnerThread(), this, __func__,
                     &MediaOffloadPlayer::HandleSeek, aTarget, true);
}

void MediaOffloadPlayer::FirePendingSeekIfExists() {
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(!mCurrentSeek.Exists());

  if (mPendingSeek.Exists()) {
    LOG("MediaOffloadPlayer::FirePendingSeekIfExists, %s seek, %.03lf sec",
        mPendingSeek.mVisible ? "user" : "internal",
        mPendingSeek.mTarget->GetTime().ToSeconds());
    mCurrentSeek = std::move(mPendingSeek);
    mPendingSeek = SeekObject();
    SeekInternal(mCurrentSeek.mTarget.ref(), mCurrentSeek.mVisible);
  }
}

void MediaOffloadPlayer::DispatchSetPlaybackRate(double aPlaybackRate) {
  MOZ_ASSERT(NS_IsMainThread());
  OwnerThread()->DispatchStateChange(NS_NewRunnableFunction(
      "MediaOffloadPlayer::SetPlaybackRate",
      [this, self = RefPtr<MediaOffloadPlayer>(this), aPlaybackRate]() {
        mPlaybackRate = aPlaybackRate;
        PlaybackRateChanged();
      }));
}

RefPtr<GenericPromise> MediaOffloadPlayer::InvokeSetSink(
    RefPtr<AudioDeviceInfo> aSink) {
  return GenericPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
}

RefPtr<SetCDMPromise> MediaOffloadPlayer::SetCDMProxy(CDMProxy* aProxy) {
  return SetCDMPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
}

void MediaOffloadPlayer::UpdateCompositor(
    already_AddRefed<layers::KnowsCompositor> aCompositor) {
  RefPtr<layers::KnowsCompositor> compositor = aCompositor;
}

void MediaOffloadPlayer::StartDormantTimer() {
  if (mInDormant) {
    return;
  }

  if (!mTransportSeekable || !mInfo.mMediaSeekable) {
    return;
  }

  auto timeout = StaticPrefs::media_dormant_on_pause_timeout_ms();
  if (timeout < 0) {
    return;
  } else if (timeout == 0) {
    EnterDormant();
    return;
  }

  LOG("MediaOffloadPlayer::StartDormantTimer, start in %d ms", timeout);
  TimeStamp target = TimeStamp::Now() + TimeDuration::FromMilliseconds(timeout);
  mDormantTimer.Ensure(
      target,
      [this, self = RefPtr<MediaOffloadPlayer>(this)]() {
        mDormantTimer.CompleteRequest();
        EnterDormant();
      },
      []() { MOZ_DIAGNOSTIC_ASSERT(false); });
}

void MediaOffloadPlayer::EnterDormant() {
  LOG("MediaOffloadPlayer::EnterDormant, resetting internal player");
  mInDormant = true;
  UpdateCurrentPosition();
  ResetInternal();
  // This queues a seek job of the current position, and it will be fired when
  // leaving dormant state.
  HandleSeek(SeekTarget(mCurrentPosition, SeekTarget::Accurate), false);
}

void MediaOffloadPlayer::ExitDormant() {
  mDormantTimer.Reset();
  if (mInDormant) {
    LOG("MediaOffloadPlayer::ExitDormant, initializing internal player");
    mInDormant = false;
    InitInternal();
  }
}

void MediaOffloadPlayer::PlayStateChanged() {
  MOZ_ASSERT(OnTaskQueue());
  LOG("MediaOffloadPlayer::PlayStateChanged, %d", mPlayState.Ref());
  if (mPlayState == MediaDecoder::PLAY_STATE_PAUSED) {
    StartDormantTimer();
  } else if (mPlayState == MediaDecoder::PLAY_STATE_PLAYING) {
    ExitDormant();
  }
}

void MediaOffloadPlayer::NotifyMediaNotSeekable() {
  LOG("MediaOffloadPlayer::NotifyMediaNotSeekable");
  mOnMediaNotSeekable.Notify();
}

void MediaOffloadPlayer::NotifyMetadataLoaded(UniquePtr<MediaInfo> aInfo,
                                              UniquePtr<MetadataTags> aTags) {
  LOG("MediaOffloadPlayer::NotifyMetadataLoaded, duration %.03lf sec, "
      "audio: %s, video: %s",
      aInfo->mMetadataDuration ? aInfo->mMetadataDuration->ToSeconds()
                               : UnspecifiedNaN<double>(),
      aInfo->HasAudio() ? aInfo->mAudio.mMimeType.get() : "none",
      aInfo->HasVideo() ? aInfo->mVideo.mMimeType.get() : "none");
  mMetadataLoadedEvent.Notify(std::move(aInfo), std::move(aTags),
                              MediaDecoderEventVisibility::Observable);
}

void MediaOffloadPlayer::NotifyFirstFrameLoaded(UniquePtr<MediaInfo> aInfo) {
  LOG("MediaOffloadPlayer::NotifyFirstFrameLoaded");
  mFirstFrameLoadedEvent.Notify(std::move(aInfo),
                                MediaDecoderEventVisibility::Observable);
}

void MediaOffloadPlayer::NotifyPlaybackEvent(MediaPlaybackEvent aEvent) {
  LOG("MediaOffloadPlayer::NotifyPlaybackEvent, type %d", aEvent.mType);
  mOnPlaybackEvent.Notify(aEvent);
}

void MediaOffloadPlayer::NotifyPlaybackError(MediaResult aError) {
  LOG("MediaOffloadPlayer::NotifyPlaybackError, %s",
      aError.Description().get());
  mOnPlaybackErrorEvent.Notify(aError);
}

void MediaOffloadPlayer::NotifyNextFrameStatus(NextFrameStatus aStatus) {
  LOG("MediaOffloadPlayer::NotifyNextFrameStatus, status %d", aStatus);
  mOnNextFrameStatus.Notify(aStatus);
}

}  // namespace mozilla
