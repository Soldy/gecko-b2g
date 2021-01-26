/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <media/stagefright/MediaCodecList.h>

#include "nsTArray.h"
#include "GfxInfo.h"

using namespace mozilla::widget;

/* GetD2DEnabled and GetDwriteEnabled shouldn't be called until after
 * gfxPlatform initialization has occurred because they depend on it for
 * information. (See bug 591561) */
nsresult GfxInfo::GetD2DEnabled(bool* aEnabled) { return NS_ERROR_FAILURE; }

nsresult GfxInfo::GetDWriteEnabled(bool* aEnabled) { return NS_ERROR_FAILURE; }

NS_IMETHODIMP
GfxInfo::GetTestType(nsAString& aTestType) { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
GfxInfo::GetDrmRenderDevice(nsACString& aDrmRenderDevice) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GfxInfo::GetDWriteVersion(nsAString& aDwriteVersion) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetCleartypeParameters(nsAString& aCleartypeParams) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDescription(nsAString& aAdapterDescription) {
  aAdapterDescription.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDescription2(nsAString& aAdapterDescription) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterRAM(uint32_t* aAdapterRAM) {
  *aAdapterRAM = 0;
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterRAM2(uint32_t* aAdapterRAM) { return NS_ERROR_FAILURE; }

NS_IMETHODIMP
GfxInfo::GetAdapterDriver(nsAString& aAdapterDriver) {
  aAdapterDriver.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriver2(nsAString& aAdapterDriver) {
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDriverVendor; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVendor(nsAString& aAdapterDriverVendor) {
  aAdapterDriverVendor.Truncate();
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverVendor2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVendor2(nsAString& aAdapterDriverVendor) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion(nsAString& aAdapterDriverVersion) {
  aAdapterDriverVersion.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion2(nsAString& aAdapterDriverVersion) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate(nsAString& aAdapterDriverDate) {
  aAdapterDriverDate.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate2(nsAString& aAdapterDriverDate) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterVendorID(nsAString& aAdapterVendorID) {
  aAdapterVendorID.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterVendorID2(nsAString& aAdapterVendorID) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID(nsAString& aAdapterDeviceID) {
  aAdapterDeviceID.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID2(nsAString& aAdapterDeviceID) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterSubsysID(nsAString& aAdapterSubsysID) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterSubsysID2(nsAString& aAdapterSubsysID) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetIsGPU2Active(bool* aIsGPU2Active) { return NS_ERROR_FAILURE; }

const nsTArray<GfxDriverInfo>& GfxInfo::GetGfxDriverInfo() {
  return *sDriverInfo;
}

uint32_t GfxInfo::OperatingSystemVersion() { return 0; }

NS_IMETHODIMP
GfxInfo::GetWindowProtocol(nsAString& aWindowProtocol) {
  return NS_ERROR_FAILURE;
}

nsresult GfxInfo::GetFeatureStatusImpl(
    int32_t aFeature, int32_t* aStatus, nsAString& /*aSuggestedDriverVersion*/,
    const nsTArray<GfxDriverInfo>& /*aDriverInfo*/, nsACString& aFailureId,
    OperatingSystem* /*aOS*/ /* = nullptr */) {
  NS_ENSURE_ARG_POINTER(aStatus);

  if (aFeature == nsIGfxInfo::FEATURE_WEBRENDER) {
    *aStatus = nsIGfxInfo::FEATURE_ALLOW_ALWAYS;
  } else if (aFeature == FEATURE_WEBRENDER_SOFTWARE) {
    *aStatus = nsIGfxInfo::FEATURE_DENIED;
  } else if (aFeature == FEATURE_WEBRTC_HW_ACCELERATION_ENCODE) {
    *aStatus = WebRtcHwVp8EncodeSupported();
    aFailureId = "FEATURE_FAILURE_WEBRTC_ENCODE";
  } else if (aFeature == FEATURE_WEBRTC_HW_ACCELERATION_DECODE) {
    *aStatus = WebRtcHwVp8DecodeSupported();
    aFailureId = "FEATURE_FAILURE_WEBRTC_DECODE";
  } else if (aFeature == FEATURE_WEBRTC_HW_ACCELERATION_H264) {
    *aStatus = WebRtcHwH264Supported();
    aFailureId = "FEATURE_FAILURE_WEBRTC_H264";
  } else {
    *aStatus = nsIGfxInfo::FEATURE_STATUS_OK;
  }

  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetEmbeddedInFirefoxReality(bool* aEmbeddedInFirefoxReality) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetDisplayInfo(nsTArray<nsString>& aDisplayInfo) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetDisplayWidth(nsTArray<uint32_t>& aDisplayWidth) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetDisplayHeight(nsTArray<uint32_t>& aDisplayHeight) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetDesktopEnvironment(nsAString& aDesktopEnvironment) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GfxInfo::GetHasBattery(bool* aHasBattery) {
  // All Gonk devices should have a battery!
  *aHasBattery = true;
  return NS_OK;
}

static bool HwCodecSupported(const char* aMimeType, bool aIsEncoder) {
  uint32_t flags = android::MediaCodecList::kHardwareCodecsOnly;
  android::Vector<android::AString> matchingCodecs;
  android::MediaCodecList::findMatchingCodecs(aMimeType, aIsEncoder, flags,
                                              &matchingCodecs);
  return !matchingCodecs.isEmpty();
}

int32_t GfxInfo::WebRtcHwVp8EncodeSupported() {
  bool supported = HwCodecSupported("video/x-vnd.on2.vp8", true);
  return supported ? nsIGfxInfo::FEATURE_STATUS_OK
                   : nsIGfxInfo::FEATURE_STATUS_UNKNOWN;
}

int32_t GfxInfo::WebRtcHwVp8DecodeSupported() {
  bool supported = HwCodecSupported("video/x-vnd.on2.vp8", false);
  return supported ? nsIGfxInfo::FEATURE_STATUS_OK
                   : nsIGfxInfo::FEATURE_STATUS_UNKNOWN;
}

int32_t GfxInfo::WebRtcHwH264Supported() {
  bool supported = HwCodecSupported("video/avc", true) &&
                   HwCodecSupported("video/avc", false);
  return supported ? nsIGfxInfo::FEATURE_STATUS_OK
                   : nsIGfxInfo::FEATURE_STATUS_UNKNOWN;
}

#ifdef DEBUG

// Implement nsIGfxInfoDebug

NS_IMETHODIMP GfxInfo::SpoofVendorID(const nsAString&) { return NS_OK; }

NS_IMETHODIMP GfxInfo::SpoofDeviceID(const nsAString&) { return NS_OK; }

NS_IMETHODIMP GfxInfo::SpoofDriverVersion(const nsAString&) { return NS_OK; }

NS_IMETHODIMP GfxInfo::SpoofOSVersion(uint32_t) { return NS_OK; }

NS_IMETHODIMP GfxInfo::FireTestProcess() { return NS_OK; }

#endif
