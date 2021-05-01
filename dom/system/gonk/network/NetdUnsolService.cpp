/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <utils/String16.h>
#include <binder/IServiceManager.h>
#include "mozilla/ClearOnShutdown.h"
#include "NetdUnsolService.h"
#include <android-base/strings.h>
#include <android/log.h>

#define BUF_SIZE 1024

using android::binder::Status;

static bool ENABLE_NUS_DEBUG = false;

static void NUS_DBG(const char* format, ...) {
  if (!ENABLE_NUS_DEBUG) {
    return;
  }

  va_list args;
  va_start(args, format);
  __android_log_vprint(ANDROID_LOG_INFO, "NetdUnsolService", format, args);
  va_end(args);
}

NetdUnsolService::NetdUnsolService(NetdEventCallback aCallback)
    : mNetdEventCallback(aCallback) {
  android::defaultServiceManager()->addService(
      android::String16(NetdUnsolService::getServiceName()), this, false,
      android::IServiceManager::DUMP_FLAG_PRIORITY_DEFAULT);
  android::sp<android::ProcessState> ps(android::ProcessState::self());
  ps->startThreadPool();
}

void NetdUnsolService::updateDebug(bool aEnable) { ENABLE_NUS_DEBUG = aEnable; }

void NetdUnsolService::sendBroadcast(UnsolEvent evt, char* reason) {
  mozilla::dom::NetworkResultOptions result;

  switch (evt) {
    case InterfaceDnsServersAdded:
      result.mTopic = NS_ConvertUTF8toUTF16("interface-dns-info");
      break;
    case InterfaceAddressUpdated:
    case InterfaceAddressRemoved:
      result.mTopic = NS_ConvertUTF8toUTF16("interface-address-change");
      break;
    case InterfaceLinkStatusChanged:
      result.mTopic = NS_ConvertUTF8toUTF16("netd-interface-change");
      break;
    case InterfaceAdded:
      result.mTopic = NS_ConvertUTF8toUTF16("netd-interface-add");
      break;
    case InterfaceRemoved:
      result.mTopic = NS_ConvertUTF8toUTF16("netd-interface-remove");
      break;
    case RouteChanged:
      result.mTopic = NS_ConvertUTF8toUTF16("route-change");
      break;
    case QuotaLimitReached:
      result.mTopic = NS_ConvertUTF8toUTF16("netd-bandwidth-control");
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

Status NetdUnsolService::onInterfaceDnsServerInfo(
    const std::string& ifName, int64_t lifetime,
    const std::vector<std::string>& servers) {
  char message[BUF_SIZE];

  SprintfLiteral(message, "DnsInfo servers %s %" PRId64 " %s", ifName.c_str(),
                 lifetime, android::base::Join(servers, ",").c_str());
  NUS_DBG("%s", message);
  sendBroadcast(InterfaceDnsServersAdded, message);
  return Status::ok();
}

Status NetdUnsolService::onInterfaceAddressUpdated(const std::string& addr,
                                                   const std::string& ifName,
                                                   int flags, int scope) {
  char message[BUF_SIZE];

  SprintfLiteral(message, "Address updated %s %s %d %d", addr.c_str(),
                 ifName.c_str(), flags, scope);
  NUS_DBG("%s", message);
  sendBroadcast(InterfaceAddressUpdated, message);
  return Status::ok();
}

Status NetdUnsolService::onInterfaceAddressRemoved(const std::string& addr,
                                                   const std::string& ifName,
                                                   int flags, int scope) {
  char message[BUF_SIZE];
  SprintfLiteral(message, "Address removed %s %s %d %d", addr.c_str(),
                 ifName.c_str(), flags, scope);
  NUS_DBG("%s", message);
  sendBroadcast(InterfaceAddressRemoved, message);
  return Status::ok();
}

Status NetdUnsolService::onInterfaceLinkStateChanged(const std::string& ifName,
                                                     bool status) {
  char message[BUF_SIZE];
  SprintfLiteral(message, "Iface linkstate %s %s", ifName.c_str(),
                 status ? "up" : "down");
  NUS_DBG("%s", message);
  sendBroadcast(InterfaceLinkStatusChanged, message);
  return Status::ok();
}

Status NetdUnsolService::onRouteChanged(bool updated, const std::string& route,
                                        const std::string& gateway,
                                        const std::string& ifName) {
  char message[BUF_SIZE];
  if (gateway.empty()) {
    SprintfLiteral(message, "Route %s %s dev %s",
                   updated ? "updated" : "removed", route.c_str(),
                   ifName.c_str());
  } else {
    SprintfLiteral(message, "Route %s %s via %s dev %s",
                   updated ? "updated" : "removed", route.c_str(),
                   gateway.c_str(), ifName.c_str());
  }
  NUS_DBG("%s", message);
  sendBroadcast(RouteChanged, message);
  return Status::ok();
}

Status NetdUnsolService::onQuotaLimitReached(const std::string& alertName,
                                             const std::string& ifName) {
  char message[BUF_SIZE];
  SprintfLiteral(message, "limit alert %s %s", alertName.c_str(),
                 ifName.c_str());
  NUS_DBG("%s", message);
  sendBroadcast(QuotaLimitReached, message);
  return Status::ok();
}

// TODO: Implement notify if we need these.
Status NetdUnsolService::onInterfaceAdded(const std::string& ifName) {
  char message[BUF_SIZE];
  SprintfLiteral(message, "Iface added %s", ifName.c_str());
  NUS_DBG("%s", message);
  sendBroadcast(InterfaceAdded, message);
  return Status::ok();
}

Status NetdUnsolService::onInterfaceRemoved(const std::string& ifName) {
  char message[BUF_SIZE];
  SprintfLiteral(message, "Iface removed %s", ifName.c_str());
  NUS_DBG("%s", message);
  sendBroadcast(InterfaceRemoved, message);
  return Status::ok();
}

Status NetdUnsolService::onInterfaceClassActivityChanged(bool isActive,
                                                         int label,
                                                         int64_t timestamp,
                                                         int uid) {
  return Status::ok();
}

Status NetdUnsolService::onInterfaceChanged(const std::string& ifName,
                                            bool status) {
  return Status::ok();
}

Status NetdUnsolService::onStrictCleartextDetected(int uid,
                                                   const std::string& hex) {
  return Status::ok();
}
