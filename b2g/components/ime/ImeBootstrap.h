/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_b2g_ime_Bootstrap_h
#define mozilla_b2g_ime_Bootstrap_h

#include "nsIObserver.h"

namespace mozilla::ime {

// This class is instanciated in each process and hooks up the
// GeckoEditableSupport properly.
class ImeBootstrap final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  ImeBootstrap();

  static void SetOnBrowserChild(dom::BrowserChild* aBrowserChild,
                                nsPIDOMWindowOuter* aDOMWindow);

  static already_AddRefed<ImeBootstrap> GetSingleton();

 protected:
  virtual ~ImeBootstrap();
};

}  // namespace mozilla::ime

#endif  // mozilla_b2g_ime_Bootstrap_h
