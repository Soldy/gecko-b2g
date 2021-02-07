/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * InitializeDateCacheCleaner registers DateCacheCleaner to
 * SystemTimeChangeObserver. When time zone is changed, DateCacheCleaner calls
 * JS::ResetTimeZone to update the time zone information.
 */

#include "nsITime.h"

namespace mozilla {
namespace dom {
class DateCacheCleaner final : public nsITimeObserver, nsISidlDefaultResponse {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISIDLDEFAULTRESPONSE
  NS_DECL_NSITIMEOBSERVER

  static void InitializeSingleton();

 private:
  DateCacheCleaner();
  virtual ~DateCacheCleaner();
};
}  // namespace dom
}  // namespace mozilla
