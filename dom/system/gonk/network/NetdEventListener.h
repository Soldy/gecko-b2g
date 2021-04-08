/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NETD_EVENT_LISTENER_H_
#define _NETD_EVENT_LISTENER_H_

#include <binder/BinderService.h>
#include "mozilla/dom/NetworkOptionsBinding.h"
#include "android/net/metrics/BnNetdEventListener.h"

enum NetdEvent : uint32_t {
  DnsEvent = 1 << 0,
  PrivateDnsValidationEvent = 1 << 1,
  ConnectEvent = 1 << 2,
  WakeupEvent = 1 << 3,
  TcpSocketStatsEvent = 1 << 4,
  Nat64PrefixEvent = 1 << 5,
};

typedef void (*NetdEventCallback)(mozilla::dom::NetworkResultOptions& aResult);

class NetdEventListener : public android::BinderService<NetdEventListener>,
                          public android::net::metrics::BnNetdEventListener {
 public:
  explicit NetdEventListener(NetdEventCallback aCallback);
  ~NetdEventListener() = default;

  void updateDebug(bool aEnable);

  static char const* getServiceName() { return "netdEventListener"; }

  android::binder::Status onDnsEvent(
      int32_t netId, int32_t eventType, int32_t returnCode, int32_t latencyMs,
      const std::string& hostname,
      const ::std::vector<std::string>& ipAddresses, int32_t ipAddressesCount,
      int32_t uid) override;
  android::binder::Status onPrivateDnsValidationEvent(
      int32_t netId, const ::android::String16& ipAddress,
      const ::android::String16& hostname, bool validated) override;
  android::binder::Status onConnectEvent(int32_t netId, int32_t error,
                                         int32_t latencyMs,
                                         const ::android::String16& ipAddr,
                                         int32_t port, int32_t uid) override;
  android::binder::Status onWakeupEvent(
      const ::android::String16& prefix, int32_t uid, int32_t ethertype,
      int32_t ipNextHeader, const ::std::vector<uint8_t>& dstHw,
      const ::android::String16& srcIp, const ::android::String16& dstIp,
      int32_t srcPort, int32_t dstPort, int64_t timestampNs) override;
  android::binder::Status onTcpSocketStatsEvent(
      const ::std::vector<int32_t>& networkIds,
      const ::std::vector<int32_t>& sentPackets,
      const ::std::vector<int32_t>& lostPackets,
      const ::std::vector<int32_t>& rttUs,
      const ::std::vector<int32_t>& sentAckDiffMs) override;
  android::binder::Status onNat64PrefixEvent(int32_t netId, bool added,
                                             const ::std::string& prefixString,
                                             int32_t prefixLength) override;

 private:
  NetdEventCallback mNetdEventCallback;

  /**
   * Notify broadcast message to main thread.
   */
  void sendBroadcast(NetdEvent evt, char* reason);
};

#endif  // _NETD_EVENT_LISTENER_H_
