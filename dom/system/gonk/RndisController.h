/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RndisController_h
#define RndisController_h

#include "nsIRndisController.h"
#include "mozilla/Observer.h"
#include "mozilla/Hal.h"
#include "mozilla/dom/usb/Types.h"

#include "nsCOMPtr.h"

namespace mozilla {

class RndisController final : public nsIRndisController, public UsbObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRNDISCONTROLLER

  static already_AddRefed<RndisController> FactoryCreate();
  void DispatchResult(nsIRndisControllerResult* aCallback, bool aSuccess);
  // For IObserver.
  void Notify(const hal::UsbStatus& aUsbStatus) override;

 private:
  RndisController();
  ~RndisController();

  nsCOMPtr<nsIThread> mRndisThread;
};

}  // namespace mozilla

#endif  // RndisController_h
