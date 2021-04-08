/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GonkDecoderModule.h"
#include "GonkVideoDecoderManager.h"
#include "GonkAudioDecoderManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/DebugOnly.h"
#include "GonkMediaDataDecoder.h"

namespace mozilla {
GonkDecoderModule::GonkDecoderModule() {}

GonkDecoderModule::~GonkDecoderModule() {}

/* static */
already_AddRefed<PlatformDecoderModule> GonkDecoderModule::Create() {
  if (Init()) {
    return MakeAndAddRef<GonkDecoderModule>();
  }
  return nullptr;
}

/* static */
bool GonkDecoderModule::Init() {
  return true;
}

already_AddRefed<MediaDataDecoder> GonkDecoderModule::CreateVideoDecoder(
    const CreateDecoderParams& aParams) {
  RefPtr<MediaDataDecoder> decoder = new GonkMediaDataDecoder(
      new GonkVideoDecoderManager(aParams.VideoConfig(),
                                  aParams.mImageContainer));
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder> GonkDecoderModule::CreateAudioDecoder(
    const CreateDecoderParams& aParams) {
  RefPtr<MediaDataDecoder> decoder = new GonkMediaDataDecoder(
      new GonkAudioDecoderManager(aParams.AudioConfig()));
  return decoder.forget();
}

bool GonkDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  return aMimeType.EqualsLiteral("audio/mp4a-latm") ||
         aMimeType.EqualsLiteral("audio/aac") ||
         aMimeType.EqualsLiteral("audio/mp4") ||
         aMimeType.EqualsLiteral("audio/3gpp") ||
         aMimeType.EqualsLiteral("audio/amr-wb") ||
         aMimeType.EqualsLiteral("audio/mpeg") ||
         aMimeType.EqualsLiteral("video/mp4") ||
         aMimeType.EqualsLiteral("video/mp4v-es") ||
         aMimeType.EqualsLiteral("video/avc") ||
         aMimeType.EqualsLiteral("video/3gpp");
}

}  // namespace mozilla
