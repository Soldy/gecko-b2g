/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CameraPreviewMediaStream.h"
#include "CameraCommon.h"
#include "MediaTrackListener.h"
#include "VideoFrameContainer.h"

/**
 * Maximum number of outstanding invalidates before we start to drop frames;
 * if we hit this threshold, it is an indicator that the main thread is
 * either very busy or the device is busy elsewhere (e.g. encoding or
 * persisting video data).
 */
#define MAX_INVALIDATE_PENDING 4

using namespace mozilla::layers;
using namespace mozilla::dom;

namespace mozilla {

void FakeMediaTrackGraph::DispatchToMainThreadStableState(
    already_AddRefed<nsIRunnable> aRunnable) {
  nsCOMPtr<nsIRunnable> task = aRunnable;
  NS_DispatchToMainThread(task);
}

nsresult FakeMediaTrackGraph::OpenAudioInput(CubebUtils::AudioDeviceID aID,
                                             AudioDataListener* aListener) {
  return NS_ERROR_FAILURE;
}

void FakeMediaTrackGraph::CloseAudioInput(CubebUtils::AudioDeviceID aID,
                                          AudioDataListener* aListener) {}

Watchable<mozilla::GraphTime>& FakeMediaTrackGraph::CurrentTime() {
  return mCurrentTime;
}

bool FakeMediaTrackGraph::OnGraphThreadOrNotRunning() const { return false; }

bool FakeMediaTrackGraph::OnGraphThread() const { return false; }

bool FakeMediaTrackGraph::Destroyed() const { return false; }

CameraPreviewMediaStream::CameraPreviewMediaStream()
    : ProcessedMediaTrack(FakeMediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
                          MediaSegment::VIDEO, new VideoSegment()),
      mMutex("mozilla::camera::CameraPreviewMediaStream"),
      mInvalidatePending(0),
      mDiscardedFrames(0),
      mRateLimit(false),
      mTrackCreated(false) {
  mFakeMediaTrackGraph = new FakeMediaTrackGraph();
}

void CameraPreviewMediaStream::AddAudioOutput(void* aKey) {}

void CameraPreviewMediaStream::SetAudioOutputVolume(void* aKey, float aVolume) {
}

void CameraPreviewMediaStream::RemoveAudioOutput(void* aKey) {}

void CameraPreviewMediaStream::AddVideoOutput(VideoFrameContainer* aContainer) {
  MutexAutoLock lock(mMutex);
  RefPtr<VideoFrameContainer> container = aContainer;
  AddVideoOutputImpl(container.forget());
}

void CameraPreviewMediaStream::RemoveVideoOutput(
    VideoFrameContainer* aContainer) {
  MutexAutoLock lock(mMutex);
  RemoveVideoOutputImpl(aContainer);
}

void CameraPreviewMediaStream::AddListener(MediaTrackListener* aListener) {
  MutexAutoLock lock(mMutex);

  MediaTrackListener* listener = *mListeners.AppendElement() = aListener;
  // TODO: Need to check the usage of these callbacks
  // listener->NotifyBlockingChanged(mFakeMediaTrackGraph,
  // MediaStreamListener::UNBLOCKED);
  // listener->NotifyHasCurrentData(mFakeMediaTrackGraph);
}

RefPtr<GenericPromise> CameraPreviewMediaStream::RemoveListener(
    MediaTrackListener* aListener) {
  MutexAutoLock lock(mMutex);

  RefPtr<MediaTrackListener> listener(aListener);
  mListeners.RemoveElement(aListener);
  listener->NotifyRemoved(mFakeMediaTrackGraph);

  // TODO: check if that's correct.
  RefPtr<GenericPromise> p = GenericPromise::CreateAndResolve(true, __func__);

  return p;
}

void CameraPreviewMediaStream::OnPreviewStateChange(bool aActive) {
  if (aActive) {
    MutexAutoLock lock(mMutex);
    if (!mTrackCreated) {
      mTrackCreated = true;
      VideoSegment tmpSegment;
      for (uint32_t j = 0; j < mListeners.Length(); ++j) {
        MediaTrackListener* l = mListeners[j];
        // TODO: Need to check the usage of these callbacks
        l->NotifyQueuedChanges(mFakeMediaTrackGraph, 0, tmpSegment);
        // l->NotifyFinishedTrackCreation(mFakeMediaTrackGraph);
      }
    }
  }
}

void CameraPreviewMediaStream::Destroy() {
  MutexAutoLock lock(mMutex);
  mMainThreadDestroyed = true;
  DestroyImpl();
}

void CameraPreviewMediaStream::Invalidate() {
  MutexAutoLock lock(mMutex);
  --mInvalidatePending;
  for (nsTArray<RefPtr<VideoFrameContainer> >::size_type i = 0;
       i < mVideoOutputs.Length(); ++i) {
    VideoFrameContainer* output = mVideoOutputs[i];
    output->Invalidate();
  }
}

void CameraPreviewMediaStream::ProcessInput(GraphTime aFrom, GraphTime aTo,
                                            uint32_t aFlags) {
  return;
}

void CameraPreviewMediaStream::RateLimit(bool aLimit) { mRateLimit = aLimit; }

void CameraPreviewMediaStream::SetCurrentFrame(
    const gfx::IntSize& aIntrinsicSize, Image* aImage) {
  {
    MutexAutoLock lock(mMutex);

    if (mInvalidatePending > 0) {
      if (mRateLimit || mInvalidatePending > MAX_INVALIDATE_PENDING) {
        ++mDiscardedFrames;
        DOM_CAMERA_LOGW("Discard preview frame %d, %d invalidation(s) pending",
                        mDiscardedFrames, mInvalidatePending);
        return;
      }

      DOM_CAMERA_LOGI("Update preview frame, %d invalidation(s) pending",
                      mInvalidatePending);
    }
    mDiscardedFrames = 0;

    TimeStamp now = TimeStamp::Now();
    for (nsTArray<RefPtr<VideoFrameContainer> >::size_type i = 0;
         i < mVideoOutputs.Length(); ++i) {
      VideoFrameContainer* output = mVideoOutputs[i];
      output->SetCurrentFrame(aIntrinsicSize, aImage, now);
    }

    ++mInvalidatePending;
  }

  NS_DispatchToMainThread(
      NewRunnableMethod("CameraPreviewMediaStream::SetCurrentFrame", this,
                        &CameraPreviewMediaStream::Invalidate));
}

void CameraPreviewMediaStream::ClearCurrentFrame() {
  MutexAutoLock lock(mMutex);

  for (nsTArray<RefPtr<VideoFrameContainer> >::size_type i = 0;
       i < mVideoOutputs.Length(); ++i) {
    VideoFrameContainer* output = mVideoOutputs[i];
    output->ClearCurrentFrame();
    NS_DispatchToMainThread(
        NewRunnableMethod("CameraPreviewMediaStream::ClearCurrentFrame", output,
                          &VideoFrameContainer::Invalidate));
  }
}

uint32_t CameraPreviewMediaStream::NumberOfChannels() const { return 1; };

/*void
CameraPreviewMediaStream::AddListener(MediaStreamListener* aListener)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, MediaStreamListener* aListener) :
      ControlMessage(aStream), mListener(aListener) {}
    virtual void Run()
    {
      mStream->AddListenerImpl(mListener.forget());
    }
    RefPtr<MediaStreamListener> mListener;
  };
  GraphImpl()->AppendMessage(new Message(this, aListener));
}*/

}  // namespace mozilla
