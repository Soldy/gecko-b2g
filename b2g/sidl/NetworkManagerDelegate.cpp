/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NetworkManagerDelegate.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsXULAppAPI.h"
#include "nsQueryObject.h"
#include "mozilla/Hal.h"
#include "nsINetworkInterface.h"
#include "nsINetworkManager.h"

#define RETURN_IF_NULL_MSG(varname, msg) \
  if (NULL == varname) {                 \
    return NS_ERROR_FAILURE;             \
  }

#define RETURN_IF_ERROR_MSG(varname, msg) \
  if (NS_OK != varname) {                 \
    return NS_ERROR_FAILURE;              \
  }

using namespace mozilla;

namespace mozilla {

// The singleton NetworkManagerDelegate service, to be used on the main thread.
static StaticRefPtr<NetworkManagerDelegateService>
    gNetworkManagerDelegateService;

NS_IMPL_ISUPPORTS(NetworkManagerDelegateService, nsINetworkManagerDelegate)

NetworkManagerDelegateService::NetworkManagerDelegateService() {}

NetworkManagerDelegateService::~NetworkManagerDelegateService() {}

/* static */
already_AddRefed<NetworkManagerDelegateService>
NetworkManagerDelegateService::ConstructNetworkManagerDelegate() {
  if (!XRE_IsParentProcess()) {
    return nullptr;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!gNetworkManagerDelegateService) {
    gNetworkManagerDelegateService = new NetworkManagerDelegateService();
    ClearOnShutdown(&gNetworkManagerDelegateService);
  }

  RefPtr<NetworkManagerDelegateService> service =
      gNetworkManagerDelegateService.get();
  return service.forget();
}

NS_IMETHODIMP
NetworkManagerDelegateService::GetNetworkInfo(int32_t* state, int32_t* type) {
  nsCOMPtr<nsINetworkManager> networkManager =
      do_GetService("@mozilla.org/network/manager;1");
  RETURN_IF_NULL_MSG(networkManager, "Can't get nsINetworkManager.")

  nsCOMPtr<nsINetworkInfo> activeNetworkInfo;
  networkManager->GetActiveNetworkInfo(getter_AddRefs(activeNetworkInfo));
  RETURN_IF_NULL_MSG(activeNetworkInfo, "Can't get nsINetworkInfo")

  nsresult res = activeNetworkInfo->GetState(state);
  if (NS_OK != res) {
    return NS_ERROR_FAILURE;
  }

  res = activeNetworkInfo->GetType(type);
  if (NS_OK != res) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

}  // namespace mozilla
