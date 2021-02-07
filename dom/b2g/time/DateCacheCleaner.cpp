/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DateCacheCleaner.h"

#include "js/Date.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Services.h"
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {
StaticRefPtr<DateCacheCleaner> sDateCacheCleaner;

NS_IMPL_ISUPPORTS(DateCacheCleaner, nsITimeObserver, nsISidlDefaultResponse)

DateCacheCleaner::DateCacheCleaner() {
  nsCOMPtr<nsITime> time = do_GetService("@mozilla.org/sidl-native/time;1");
  if (time) {
    time->AddObserver(nsITime::TIMEZONE_CHANGED, this, this);
    time->AddObserver(nsITime::TIME_CHANGED, this, this);
  }
}

DateCacheCleaner::~DateCacheCleaner() {
  nsCOMPtr<nsITime> time = do_GetService("@mozilla.org/sidl-native/time;1");
  if (time) {
    time->RemoveObserver(nsITime::TIMEZONE_CHANGED, this, this);
    time->RemoveObserver(nsITime::TIME_CHANGED, this, this);
  }
}

/* static */ void DateCacheCleaner::InitializeSingleton() {
  if (!sDateCacheCleaner) {
    sDateCacheCleaner = new DateCacheCleaner();
    ClearOnShutdown(&sDateCacheCleaner);
  }
}

NS_IMETHODIMP DateCacheCleaner::Resolve(void) { return NS_OK; }

NS_IMETHODIMP DateCacheCleaner::Reject(void) { return NS_OK; }

NS_IMETHODIMP DateCacheCleaner::Notify(nsITimeInfo* info) {
  int16_t reason = -1;
  info->GetReason(&reason);

  // To reset js Date timezone
  if (reason == nsITime::TIMEZONE_CHANGED) {
    JS::ResetTimeZone();
    return NS_OK;
  }

  // To notify system clock change topic observers.
  if (reason == nsITime::TIME_CHANGED) {
    nsString dataStr;
    int64_t delta = -1;
    info->GetDelta(&delta);
    dataStr.AppendInt(delta);

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (!obs) {
      return NS_ERROR_UNEXPECTED;
    }
    obs->NotifyObservers(nullptr, "system-clock-change", dataStr.get());
    return NS_OK;
  }
  return NS_OK;
}
}  // namespace dom
}  // namespace mozilla
