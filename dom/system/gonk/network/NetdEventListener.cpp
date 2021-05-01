/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <utils/String16.h>
#include <binder/IServiceManager.h>
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Sprintf.h"
#include "NetdEventListener.h"
#include <android-base/strings.h>
#include <android/log.h>

#define BUF_SIZE 1024

using android::binder::Status;

static bool ENABLE_NEL_DEBUG = false;

static void NEL_DBG(const char* format, ...) {
  if (!ENABLE_NEL_DEBUG) {
    return;
  }

  va_list args;
  va_start(args, format);
  __android_log_vprint(ANDROID_LOG_INFO, "NetdEventListener", format, args);
  va_end(args);
}

NetdEventListener::NetdEventListener(NetdEventCallback aCallback)
    : mNetdEventCallback(aCallback) {
  android::defaultServiceManager()->addService(
      android::String16(NetdEventListener::getServiceName()), this, false,
      android::IServiceManager::DUMP_FLAG_PRIORITY_DEFAULT);
  android::sp<android::ProcessState> ps(android::ProcessState::self());
  ps->startThreadPool();
}

void NetdEventListener::updateDebug(bool aEnable) {
  ENABLE_NEL_DEBUG = aEnable;
}

void NetdEventListener::sendBroadcast(NetdEvent evt, char* reason) {
  mozilla::dom::NetworkResultOptions result;

  switch (evt) {
    case DnsEvent:
      result.mTopic = NS_ConvertUTF8toUTF16("on-dns-event");
      break;
    case Nat64PrefixEvent:
      result.mTopic = NS_ConvertUTF8toUTF16("on-nat64prefix-event");
      break;
    default:
      return;
  }

  result.mBroadcast = true;
  result.mReason = NS_ConvertUTF8toUTF16(reason);
  if (mNetdEventCallback) {
    mNetdEventCallback(result);
  }
}

Status NetdEventListener::onDnsEvent(
    int32_t netId, int32_t eventType, int32_t returnCode, int32_t latencyMs,
    const std::string& hostname, const ::std::vector<std::string>& ipAddresses,
    int32_t ipAddressesCount, int32_t uid) {
  char message[BUF_SIZE];
  SprintfLiteral(message, "%d %d %d %d %s %d %d %s", netId, eventType,
                 returnCode, latencyMs, hostname.c_str(), uid, ipAddressesCount,
                 android::base::Join(ipAddresses, " ").c_str());
  NEL_DBG("%s %s", __FUNCTION__, message);
  sendBroadcast(DnsEvent, message);
  return Status::ok();
}

Status NetdEventListener::onNat64PrefixEvent(int32_t netId, bool added,
                                             const ::std::string& prefixString,
                                             int32_t prefixLength) {
  char message[BUF_SIZE];
  SprintfLiteral(message, "%d %s %s %d", netId, added ? "add" : "remove",
                 prefixString.c_str(), prefixLength);
  NEL_DBG("%s %s", __FUNCTION__, message);
  sendBroadcast(Nat64PrefixEvent, message);
  return Status::ok();
}

Status NetdEventListener::onWakeupEvent(
    const ::android::String16& prefix, int32_t uid, int32_t ethertype,
    int32_t ipNextHeader, const ::std::vector<uint8_t>& dstHw,
    const ::android::String16& srcIp, const ::android::String16& dstIp,
    int32_t srcPort, int32_t dstPort, int64_t timestampNs) {
  char message[BUF_SIZE];
  SprintfLiteral(message, "%s %d %d %d %s %s %s %d %ld",
                 ::android::String8(prefix).string(), uid, ethertype,
                 ipNextHeader, android::base::Join(dstHw, ":").c_str(),
                 ::android::String8(srcIp).string(),
                 ::android::String8(dstIp).string(), dstPort, timestampNs);

  NEL_DBG("%s %s", __FUNCTION__, message);
  return Status::ok();
}

// TODO: Implement below notify if we needed.
Status NetdEventListener::onPrivateDnsValidationEvent(
    int32_t netId, const ::android::String16& ipAddress,
    const ::android::String16& hostname, bool validated) {
  return Status::ok();
}

Status NetdEventListener::onConnectEvent(int32_t netId, int32_t error,
                                         int32_t latencyMs,
                                         const ::android::String16& ipAddr,
                                         int32_t port, int32_t uid) {
  return Status::ok();
}

Status NetdEventListener::onTcpSocketStatsEvent(
    const ::std::vector<int32_t>& networkIds,
    const ::std::vector<int32_t>& sentPackets,
    const ::std::vector<int32_t>& lostPackets,
    const ::std::vector<int32_t>& rttUs,
    const ::std::vector<int32_t>& sentAckDiffMs) {
  return Status::ok();
}
