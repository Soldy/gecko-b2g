/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadio.h"
#include "AudioChannelService.h"
#include "mozilla/dom/FMRadioBinding.h"
#include "mozilla/dom/FMRadioService.h"
#include "mozilla/dom/PFMRadioChild.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/HalTypes.h"
#include "mozilla/Preferences.h"
#include "nsCycleCollectionParticipant.h"

#undef LOG
#define LOG(args...) FM_LOG("FMRadio", args)

// The pref indicates if the device has an internal antenna.
// If the pref is true, the antanna will be always available.
#define DOM_FM_ANTENNA_INTERNAL_PREF "dom.fmradio.antenna.internal"

using mozilla::Preferences;

BEGIN_FMRADIO_NAMESPACE

class FMRadioRequest final : public FMRadioReplyRunnable {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  static already_AddRefed<FMRadioRequest> Create(
      nsPIDOMWindowInner* aWindow, FMRadio* aFMRadio,
      FMRadioRequestArgs::Type aType = FMRadioRequestArgs::T__None) {
    MOZ_ASSERT(aType >= FMRadioRequestArgs::T__None &&
                   aType <= FMRadioRequestArgs::T__Last,
               "Wrong FMRadioRequestArgs in FMRadioRequest");

    ErrorResult rv;
    RefPtr<Promise> promise = Promise::Create(aWindow->AsGlobal(), rv);
    if (rv.Failed()) {
      return nullptr;
    }

    RefPtr<FMRadioRequest> request =
        new FMRadioRequest(promise, aFMRadio, aType);
    return request.forget();
  }

  NS_IMETHODIMP
  Run() override {
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

    nsCOMPtr<EventTarget> target = do_QueryReferent(mFMRadio);
    if (!target) {
      return NS_OK;
    }

    FMRadio* fmRadio = static_cast<FMRadio*>(static_cast<EventTarget*>(target));
    if (fmRadio->mIsShutdown) {
      return NS_OK;
    }

    if (!mPromise) {
      return NS_OK;
    }

    switch (mResponseType.type()) {
      case FMRadioResponseType::TErrorResponse:
        mPromise->MaybeRejectWithOperationError(
            NS_ConvertUTF16toUTF8(mResponseType.get_ErrorResponse().error()));
        break;
      case FMRadioResponseType::TSuccessResponse:
        mPromise->MaybeResolveWithUndefined();
        break;
      default:
        MOZ_CRASH();
    }
    mPromise = nullptr;
    return NS_OK;
  }

  already_AddRefed<Promise> GetPromise() {
    RefPtr<Promise> promise = mPromise.get();
    return promise.forget();
  }

 protected:
  FMRadioRequest(Promise* aPromise, FMRadio* aFMRadio,
                 FMRadioRequestArgs::Type aType)
      : mPromise(aPromise), mType(aType) {
    // |FMRadio| inherits from |nsIDOMEventTarget| and
    // |nsISupportsWeakReference| which both inherits from nsISupports, so
    // |nsISupports| is an ambiguous base of |FMRadio|, we have to cast
    // |aFMRadio| to one of the base classes.
    mFMRadio = do_GetWeakReference(static_cast<EventTarget*>(aFMRadio));
  }

  ~FMRadioRequest() {}

 private:
  RefPtr<Promise> mPromise;
  FMRadioRequestArgs::Type mType;
  nsWeakPtr mFMRadio;
};

NS_IMPL_ISUPPORTS_INHERITED0(FMRadioRequest, FMRadioReplyRunnable)

FMRadio::FMRadio()
    : mRdsGroupMask(0),
      mAudioChannelAgentEnabled(false),
      mHasInternalAntenna(false),
      mIsShutdown(false) {
  LOG("FMRadio is initialized.");
}

FMRadio::~FMRadio() {}

void FMRadio::Init(nsIGlobalObject* aGlobal) {
  BindToOwner(aGlobal);

  IFMRadioService::Singleton()->AddObserver(this);

  mHasInternalAntenna = Preferences::GetBool(DOM_FM_ANTENNA_INTERNAL_PREF,
                                             /* default = */ false);

  mAudioChannelAgent = new AudioChannelAgent();
  nsresult rv = mAudioChannelAgent->InitWithWeakCallback(
      GetOwner(), nsIAudioChannelAgent::AUDIO_AGENT_CHANNEL_CONTENT, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mAudioChannelAgent = nullptr;
    LOG("FMRadio::Init, Fail to initialize the audio channel agent");
    return;
  }
}

void FMRadio::Shutdown() {
  IFMRadioService::Singleton()->RemoveObserver(this);
  DisableAudioChannelAgent();
  mIsShutdown = true;
}

JSObject* FMRadio::WrapObject(JSContext* aCx,
                              JS::Handle<JSObject*> aGivenProto) {
  return FMRadio_Binding::Wrap(aCx, this, aGivenProto);
}

void FMRadio::Notify(const FMRadioEventType& aType) {
  switch (aType) {
    case FrequencyChanged:
      DispatchTrustedEvent(u"frequencychange"_ns);
      break;
    case EnabledChanged:
      if (Enabled()) {
        EnableAudioChannelAgent();
        DispatchTrustedEvent(u"enabled"_ns);
      } else {
        DisableAudioChannelAgent();
        DispatchTrustedEvent(u"disabled"_ns);
      }
      break;
    case RDSEnabledChanged:
      if (RdsEnabled()) {
        DispatchTrustedEvent(u"rdsenabled"_ns);
      } else {
        DispatchTrustedEvent(u"rdsdisabled"_ns);
      }
      break;
    case PIChanged:
      DispatchTrustedEvent(u"pichange"_ns);
      break;
    case PSChanged:
      DispatchTrustedEvent(u"pschange"_ns);
      break;
    case RadiotextChanged:
      DispatchTrustedEvent(u"rtchange"_ns);
      break;
    case PTYChanged:
      DispatchTrustedEvent(u"ptychange"_ns);
      break;
    case NewRDSGroup:
      DispatchTrustedEvent(u"newrdsgroup"_ns);
      break;
    default:
      MOZ_CRASH();
  }
}

/* static */
bool FMRadio::Enabled() { return IFMRadioService::Singleton()->IsEnabled(); }

bool FMRadio::RdsEnabled() {
  return IFMRadioService::Singleton()->IsRDSEnabled();
}

bool FMRadio::AntennaAvailable() const { return mHasInternalAntenna; }

Nullable<double> FMRadio::GetFrequency() const {
  return Enabled()
             ? Nullable<double>(IFMRadioService::Singleton()->GetFrequency())
             : Nullable<double>();
}

double FMRadio::FrequencyUpperBound() const {
  return IFMRadioService::Singleton()->GetFrequencyUpperBound();
}

double FMRadio::FrequencyLowerBound() const {
  return IFMRadioService::Singleton()->GetFrequencyLowerBound();
}

double FMRadio::ChannelWidth() const {
  return IFMRadioService::Singleton()->GetChannelWidth();
}

uint32_t FMRadio::RdsGroupMask() const { return mRdsGroupMask; }

void FMRadio::SetRdsGroupMask(uint32_t aRdsGroupMask) {
  mRdsGroupMask = aRdsGroupMask;
  IFMRadioService::Singleton()->SetRDSGroupMask(aRdsGroupMask);
}

Nullable<unsigned short> FMRadio::GetPi() const {
  return IFMRadioService::Singleton()->GetPi();
}

Nullable<uint8_t> FMRadio::GetPty() const {
  return IFMRadioService::Singleton()->GetPty();
}

void FMRadio::GetPs(DOMString& aPsname) const {
  if (!IFMRadioService::Singleton()->GetPs(aPsname)) {
    aPsname.SetNull();
  }
}

void FMRadio::GetRt(DOMString& aRadiotext) const {
  if (!IFMRadioService::Singleton()->GetRt(aRadiotext)) {
    aRadiotext.SetNull();
  }
}

void FMRadio::GetRdsgroup(JSContext* cx, JS::MutableHandle<JSObject*> retval) {
  uint64_t group;
  if (!IFMRadioService::Singleton()->GetRdsgroup(group)) {
    return;
  }

  JSObject* rdsgroup = Uint16Array::Create(cx, this, 4);
  JS::AutoCheckCannotGC nogc;
  bool isShared = false;
  uint16_t* data = JS_GetUint16ArrayData(rdsgroup, &isShared, nogc);
  MOZ_ASSERT(!isShared);  // Because created above.
  data[3] = group & 0xFFFF;
  group >>= 16;
  data[2] = group & 0xFFFF;
  group >>= 16;
  data[1] = group & 0xFFFF;
  group >>= 16;
  data[0] = group & 0xFFFF;

  JS::ExposeObjectToActiveJS(rdsgroup);
  retval.set(rdsgroup);
}

already_AddRefed<Promise> FMRadio::Enable(double aFrequency) {
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  RefPtr<FMRadioRequest> r =
      FMRadioRequest::Create(win, this, FMRadioRequestArgs::TEnableRequestArgs);
  if (!r) {
    return nullptr;
  }

  IFMRadioService::Singleton()->Enable(aFrequency, r);
  return r->GetPromise();
}

already_AddRefed<Promise> FMRadio::Disable() {
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  RefPtr<FMRadioRequest> r = FMRadioRequest::Create(
      win, this, FMRadioRequestArgs::TDisableRequestArgs);
  if (!r) {
    return nullptr;
  }

  IFMRadioService::Singleton()->Disable(r);
  return r->GetPromise();
}

already_AddRefed<Promise> FMRadio::SetFrequency(double aFrequency) {
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  RefPtr<FMRadioRequest> r = FMRadioRequest::Create(win, this);
  if (!r) {
    return nullptr;
  }

  IFMRadioService::Singleton()->SetFrequency(aFrequency, r);
  return r->GetPromise();
}

already_AddRefed<Promise> FMRadio::SeekUp() {
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  RefPtr<FMRadioRequest> r = FMRadioRequest::Create(win, this);
  if (!r) {
    return nullptr;
  }

  IFMRadioService::Singleton()->Seek(hal::FM_RADIO_SEEK_DIRECTION_UP, r);
  return r->GetPromise();
}

already_AddRefed<Promise> FMRadio::SeekDown() {
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  RefPtr<FMRadioRequest> r = FMRadioRequest::Create(win, this);
  if (!r) {
    return nullptr;
  }

  IFMRadioService::Singleton()->Seek(hal::FM_RADIO_SEEK_DIRECTION_DOWN, r);
  return r->GetPromise();
}

already_AddRefed<Promise> FMRadio::CancelSeek() {
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  RefPtr<FMRadioRequest> r = FMRadioRequest::Create(win, this);
  if (!r) {
    return nullptr;
  }

  IFMRadioService::Singleton()->CancelSeek(r);
  return r->GetPromise();
}

already_AddRefed<Promise> FMRadio::EnableRDS() {
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  RefPtr<FMRadioRequest> r = FMRadioRequest::Create(win, this);
  if (!r) {
    return nullptr;
  }

  IFMRadioService::Singleton()->EnableRDS(r);
  return r->GetPromise();
}

already_AddRefed<Promise> FMRadio::DisableRDS() {
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  RefPtr<FMRadioRequest> r = FMRadioRequest::Create(win, this);
  if (!r) {
    return nullptr;
  }

  IFMRadioService::Singleton()->DisableRDS(r);
  return r->GetPromise();
}

void FMRadio::EnableAudioChannelAgent() {
  NS_ENSURE_TRUE_VOID(mAudioChannelAgent);
  mAudioChannelAgent->NotifyStartedPlaying(
      AudioChannelService::AudibleState::eAudible);
  mAudioChannelAgent->PullInitialUpdate();
  mAudioChannelAgentEnabled = true;
}

void FMRadio::DisableAudioChannelAgent() {
  if (mAudioChannelAgentEnabled) {
    NS_ENSURE_TRUE_VOID(mAudioChannelAgent);
    mAudioChannelAgent->NotifyStoppedPlaying();
    mAudioChannelAgentEnabled = false;
  }
}

NS_IMETHODIMP FMRadio::WindowVolumeChanged(float aVolume, bool aMuted) {
  IFMRadioService::Singleton()->SetVolume(aMuted ? 0.0f : aVolume);
  return NS_OK;
}

NS_IMETHODIMP FMRadio::WindowSuspendChanged(SuspendTypes aSuspend) {
  IFMRadioService::Singleton()->EnableAudio(aSuspend ==
                                            nsISuspendedTypes::NONE_SUSPENDED);
  return NS_OK;
}

NS_IMETHODIMP FMRadio::WindowAudioCaptureChanged(bool aCapture) {
  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FMRadio)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIAudioChannelAgentCallback)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(FMRadio, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(FMRadio, DOMEventTargetHelper)

END_FMRADIO_NAMESPACE
