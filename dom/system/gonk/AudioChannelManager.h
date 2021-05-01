/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_system_AudioChannelManager_h
#define mozilla_dom_system_AudioChannelManager_h

#include "nsIDOMEventListener.h"
#include "mozilla/dom/AudioChannelHandler.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Hal.h"
#include "mozilla/HalTypes.h"
#include "mozilla/Maybe.h"
#include "AudioChannelService.h"

namespace mozilla {
namespace hal {
class SwitchEvent;
typedef Observer<SwitchEvent> SwitchObserver;
}  // namespace hal

namespace dom {
namespace system {

class AudioChannelManager final : public DOMEventTargetHelper,
                                  public hal::SwitchObserver,
                                  public nsIDOMEventListener {
 public:
  AudioChannelManager();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMEVENTLISTENER

  void Notify(const hal::SwitchEvent& aEvent) override;

  void Init(nsIGlobalObject* aGlobal);

  /**
   * WebIDL Interface
   */

  nsPIDOMWindowInner* GetParentObject() const { return GetOwner(); }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  bool Headphones();

  void GetHeadphonesStatus(nsAString& aType);

  void SetVolumeControlChannel(const nsAString& aChannel);

  void GetVolumeControlChannel(nsAString& aChannel);

  IMPL_EVENT_HANDLER(headphoneschange)

  void GetAllowedAudioChannels(
      nsTArray<RefPtr<AudioChannelHandler>>& aAudioChannels,
      mozilla::ErrorResult& aRv);

  void Shutdown();

 protected:
  virtual ~AudioChannelManager() = default;

 private:
  void NotifyVolumeControlChannelChanged();

  Maybe<hal::SwitchState> mState;
  int32_t mVolumeChannel;
};

}  // namespace system
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_system_AudioChannelManager_h
