/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AMRDemuxer.h"

#include "TimeUnits.h"
#include "VideoUtils.h"
#include "mozilla/UniquePtr.h"
#include <inttypes.h>

extern mozilla::LazyLogModule gMediaDemuxerLog;
#define AMRLOG(msg, ...) \
  DDMOZ_LOG(gMediaDemuxerLog, LogLevel::Debug, msg, ##__VA_ARGS__)
#define AMRLOGV(msg, ...) \
  DDMOZ_LOG(gMediaDemuxerLog, LogLevel::Verbose, msg, ##__VA_ARGS__)
#define AMRLOGE(msg, ...) \
  DDMOZ_LOG(gMediaDemuxerLog, LogLevel::Error, msg, ##__VA_ARGS__)

namespace mozilla {

#define AMR_FILE_HEADER_LENGTH_NB 6
#define AMR_FILE_HEADER_LENGTH_WB 9
#define AMR_FRAME_HEADER_LENGTH 1
// File has duration large than 5 mins, we treat it as large file.
// Num of 5 mins frames: 1000/20*60*5 = 15000
#define AMR_LARGE_FILE_FRAMES_THRESHOLD 15000

using media::TimeUnit;

// AMRDemuxer

AMRDemuxer::AMRDemuxer(MediaResource* aSource) : mSource(aSource) {
  DDLINKCHILD("source", aSource);
}

bool AMRDemuxer::InitInternal() {
  if (!mTrackDemuxer) {
    mTrackDemuxer = new AMRTrackDemuxer(mSource);
    DDLINKCHILD("track demuxer", mTrackDemuxer.get());
  }
  return mTrackDemuxer->Init();
}

RefPtr<AMRDemuxer::InitPromise> AMRDemuxer::Init() {
  if (!InitInternal()) {
    AMRLOGE("Init() failure: waiting for data");

    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                                        __func__);
  }

  AMRLOG("Init() successful");
  return InitPromise::CreateAndResolve(NS_OK, __func__);
}

uint32_t AMRDemuxer::GetNumberTracks(TrackInfo::TrackType aType) const {
  return (aType == TrackInfo::kAudioTrack) ? 1 : 0;
}

already_AddRefed<MediaTrackDemuxer> AMRDemuxer::GetTrackDemuxer(
    TrackInfo::TrackType aType, uint32_t aTrackNumber) {
  if (!mTrackDemuxer) {
    return nullptr;
  }

  return RefPtr<AMRTrackDemuxer>(mTrackDemuxer).forget();
}

bool AMRDemuxer::IsSeekable() const {
  int64_t length = mSource->GetLength();
  if (length > -1) return true;
  return false;
}

// AMRTrackDemuxer
AMRTrackDemuxer::AMRTrackDemuxer(MediaResource* aSource)
    : mSource(aSource),
      mOffset(0),
      mIsWide(false),
      mCurrentTimeUs(0),
      mOffsetTableLength(0) {
  DDLINKCHILD("source", aSource);
}

AMRTrackDemuxer::~AMRTrackDemuxer() {}

bool AMRTrackDemuxer::Init() {
  uint8_t header[AMR_FILE_HEADER_LENGTH_WB];
  ssize_t n = Read(header, 0, AMR_FILE_HEADER_LENGTH_WB);
  if (n < AMR_FILE_HEADER_LENGTH_WB) {
    AMRLOGE("Init(): Fail to read AMR file header from file.");
    return 0;
  }

  if (!SniffAMR(header, n, &mIsWide)) {
    return 0;
  }

  off64_t offset =
      mIsWide ? AMR_FILE_HEADER_LENGTH_WB : AMR_FILE_HEADER_LENGTH_NB;
  off64_t streamSize = mSource.GetLength();
  size_t frameSize, numFrames = 0;
  int64_t duration = 0;

  if (streamSize > 0) {
    while (offset < streamSize) {
      nsresult status = GetFrameSizeByOffset(offset, mIsWide, &frameSize);
      if (status == NS_ERROR_DOM_MEDIA_END_OF_STREAM) {
        break;
      } else if (status != NS_OK) {
        return 0;
      }

      if ((numFrames % 50 == 0) && (numFrames / 50 < OFFSET_TABLE_LEN)) {
        MOZ_ASSERT(mOffsetTableLength == (numFrames / 50));
        mOffsetTable[mOffsetTableLength] = offset;
        mOffsetTableLength++;
      }

      offset += frameSize;
      duration += 20000;  // Each frame is 20ms
      numFrames++;

      // Check large file.
      if (numFrames >= AMR_LARGE_FILE_FRAMES_THRESHOLD) {
        // For large file, we estimate the duration
        // by average size of above frames instead of
        // scanning whole file to save init time.
        duration = streamSize / (offset / numFrames) * 20000;
        break;
      }
    }
  }

  Seek(TimeUnit::Zero());

  if (!mInfo) {
    mInfo = MakeUnique<AudioInfo>();
  }

  mInfo->mRate = mIsWide ? 16000 : 8000;
  mInfo->mChannels = 1;
  mInfo->mBitDepth = 16;
  mInfo->mDuration = TimeUnit::FromMicroseconds(duration);

  // AMR Specific information
  mInfo->mMimeType = mIsWide ? "audio/amr-wb" : "audio/3gpp";

  AMRLOG("Init mInfo={mRate=%u mChannels=%u mBitDepth=%u mDuration=%" PRId64
         "}",
         mInfo->mRate, mInfo->mChannels, mInfo->mBitDepth,
         mInfo->mDuration.ToMicroseconds());

  return mInfo->mChannels;
}

int32_t AMRTrackDemuxer::GetFrameSize(bool aIsWide, unsigned aFT) {
  static const int32_t kFrameSizeNB[16] = {
      95, 103, 118, 134, 148, 159, 204, 244, 39, 43, 38, 37,  // SID
      0,  0,   0,                                             // future use
      0                                                       // no data
  };
  static const int32_t kFrameSizeWB[16] = {
      132, 177, 253, 285, 317, 365, 397, 461, 477,
      40,                // SID
      0,   0,   0,   0,  // future use
      0,                 // speech lost
      0                  // no data
  };

  if (aFT > 15 || (aIsWide && aFT > 9 && aFT < 14) ||
      (!aIsWide && aFT > 11 && aFT < 15)) {
    return 0;
  }

  size_t frameSize = aIsWide ? kFrameSizeWB[aFT] : kFrameSizeNB[aFT];

  // Round up bits to bytes and add 1 for the header byte.
  frameSize = (frameSize + 7) / 8 + 1;

  return frameSize;
}

nsresult AMRTrackDemuxer::GetFrameSizeByOffset(off64_t aOffset, bool aIsWide,
                                               size_t* aFrameSize) {
  uint8_t header;
  ssize_t count = Read(&header, aOffset, 1);
  if (count == 0) {
    return NS_ERROR_DOM_MEDIA_END_OF_STREAM;
  } else if (count < 0) {
    return NS_ERROR_DOM_MEDIA_FATAL_ERR;
  }

  unsigned FT = (header >> 3) & 0x0f;

  *aFrameSize = GetFrameSize(aIsWide, FT);
  if (*aFrameSize == 0) {
    return NS_ERROR_DOM_MEDIA_DEMUXER_ERR;
  }
  return NS_OK;
}

/* static */
bool AMRTrackDemuxer::SniffAMR(const uint8_t* aSource, const uint32_t aLength,
                               bool* aIsWide) {
  if (aLength < AMR_FILE_HEADER_LENGTH_WB) {
    return false;
  }

  if (!memcmp(aSource, "#!AMR\n", AMR_FILE_HEADER_LENGTH_NB)) {
    if (aIsWide != nullptr) {
      *aIsWide = false;
    }
    return true;
  } else if (!memcmp(aSource, "#!AMR-WB\n", AMR_FILE_HEADER_LENGTH_WB)) {
    if (aIsWide != nullptr) {
      *aIsWide = true;
    }
    return true;
  }

  return false;
}

UniquePtr<TrackInfo> AMRTrackDemuxer::GetInfo() const { return mInfo->Clone(); }

RefPtr<AMRTrackDemuxer::SeekPromise> AMRTrackDemuxer::Seek(
    const TimeUnit& aTime) {
  const int64_t time =
      (aTime.ToMicroseconds() >= 0) ? aTime.ToMicroseconds() : 0;

  int64_t seekFrame = time / 20000LL;  // 20ms per frame.
  if (mOffsetTableLength > 0) {
    size_t size;
    mCurrentTimeUs = seekFrame * 20000LL;

    size_t index = seekFrame < 0 ? 0 : seekFrame / 50;
    if (index >= mOffsetTableLength) {
      index = mOffsetTableLength - 1;
    }

    mOffset = mOffsetTable[index];

    for (size_t i = 0; i < seekFrame - index * 50; i++) {
      nsresult err;
      if ((err = GetFrameSizeByOffset(mOffset, mIsWide, &size)) != NS_OK) {
        return SeekPromise::CreateAndReject(err, __func__);
      }
      mOffset += size;
    }
  }

  return SeekPromise::CreateAndResolve(
      TimeUnit::FromMicroseconds(mCurrentTimeUs), __func__);
}

RefPtr<AMRTrackDemuxer::SamplesPromise> AMRTrackDemuxer::GetSamples(
    int32_t aNumSamples) {
  AMRLOGV("GetSamples(%d) Begin mOffset=%" PRIu64, aNumSamples, mOffset);

  MOZ_ASSERT(aNumSamples);

  RefPtr<SamplesHolder> frames = new SamplesHolder();

  nsresult err = NS_OK;
  while (aNumSamples--) {
    uint8_t header;
    ssize_t n = Read(&header, mOffset, 1);

    if (n < 1) {
      err = NS_ERROR_DOM_MEDIA_END_OF_STREAM;
      break;
    }

    if (header & 0x83) {
      // Padding bits must be 0.
      AMRLOGE("padding bits must be 0, header is 0x%02x", header);
      err = NS_ERROR_DOM_MEDIA_DEMUXER_ERR;
      break;
    }

    unsigned FT = (header >> 3) & 0x0f;

    size_t frameSize = GetFrameSize(mIsWide, FT);
    if (frameSize == 0) {
      err = NS_ERROR_DOM_MEDIA_DEMUXER_ERR;
      break;
    }

    RefPtr<MediaRawData> frame = new MediaRawData();
    frame->mOffset = 0;

    UniquePtr<MediaRawDataWriter> frameWriter(frame->CreateWriter());
    if (!frameWriter->SetSize(frameSize)) {
      AMRLOG("GetSamples() Exit failed to allocate media buffer");
      err = NS_ERROR_DOM_MEDIA_FATAL_ERR;
      break;
    }

    n = Read(frameWriter->Data(), mOffset, frameSize);
    if (n != ssize_t(frameSize)) {
      AMRLOG("GetSamples() Exit read=%u frame->Size()=%zu", n, frame->Size());
      if (n < 0) {
        err = NS_ERROR_DOM_MEDIA_FATAL_ERR;
        break;
      } else {
        // only partial frame is available, treat it as EOS.
        mOffset += n;
        err = NS_ERROR_DOM_MEDIA_END_OF_STREAM;
        break;
      }
    }

    frame->mTime = TimeUnit::FromMicroseconds(mCurrentTimeUs);
    frame->mDuration = TimeUnit::FromMicroseconds(20000);
    frame->mTimecode = frame->mTime;
    frame->mKeyframe = true;

    mOffset += frameSize;
    mCurrentTimeUs += 20000;  // Each frame is 20ms
    frames->AppendSample(frame);
  }

  AMRLOGV(
      "GetSamples() End mSamples.Size()=%zu aNumSamples=%d mOffset=%" PRIu64,
      frames->GetSamples().Length(), aNumSamples, mOffset);

  if (err != NS_OK) {
    return SamplesPromise::CreateAndReject(err, __func__);
  }

  return SamplesPromise::CreateAndResolve(frames, __func__);
}

void AMRTrackDemuxer::Reset() {
  AMRLOG("Reset()");
  Seek(TimeUnit::Zero());
}

RefPtr<AMRTrackDemuxer::SkipAccessPointPromise>
AMRTrackDemuxer::SkipToNextRandomAccessPoint(const TimeUnit& aTimeThreshold) {
  // Will not be called for audio-only resources.
  return SkipAccessPointPromise::CreateAndReject(
      SkipFailureHolder(NS_ERROR_DOM_MEDIA_DEMUXER_ERR, 0), __func__);
}

int64_t AMRTrackDemuxer::GetResourceOffset() const { return mOffset; }

media::TimeIntervals AMRTrackDemuxer::GetBuffered() {
  auto duration = mInfo->mDuration;

  if (!duration.IsPositive()) {
    return media::TimeIntervals();
  }

  AutoPinned<MediaResource> stream(mSource.GetResource());
  return GetEstimatedBufferedTimeRanges(stream, duration.ToMicroseconds());
}

int32_t AMRTrackDemuxer::Read(uint8_t* aBuffer, int64_t aOffset,
                              int32_t aSize) {
  AMRLOGV("AMRTrackDemuxer::Read(%p %" PRId64 " %d)", aBuffer, aOffset, aSize);

  const int64_t streamLen = mSource.GetLength();
  if (mInfo && streamLen > 0) {
    // Prevent blocking reads after successful initialization.
    aSize = std::min<int64_t>(aSize, streamLen - aOffset);
  }

  uint32_t read = 0;
  AMRLOGV("AMRTrackDemuxer::Read        -> ReadAt(%d)", aSize);
  const nsresult rv = mSource.ReadAt(aOffset, reinterpret_cast<char*>(aBuffer),
                                     static_cast<uint32_t>(aSize), &read);
  NS_ENSURE_SUCCESS(rv, 0);
  return static_cast<int32_t>(read);
}

/* static */
bool AMRDemuxer::AMRSniffer(const uint8_t* aData, const uint32_t aLength) {
  return AMRTrackDemuxer::SniffAMR(aData, aLength);
}

}  // namespace mozilla
