/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRilWorker.h"
#include "mozilla/Preferences.h"

/* Logging related */
#if !defined(RILWORKER_LOG_TAG)
#  define RILWORKER_LOG_TAG "RilWorker"
#endif

#undef INFO
#undef ERROR
#undef DEBUG
#define INFO(args...) \
  __android_log_print(ANDROID_LOG_INFO, RILWORKER_LOG_TAG, ##args)
#define ERROR(args...) \
  __android_log_print(ANDROID_LOG_ERROR, RILWORKER_LOG_TAG, ##args)
#define ERROR_NS_OK(args...) \
  { \
   __android_log_print(ANDROID_LOG_ERROR, RILWORKER_LOG_TAG, ##args); \
   return NS_OK; \
  }
#define DEBUG(args...)                                                   \
  do {                                                                   \
    if (gRilDebug_isLoggingEnabled) {                                    \
      __android_log_print(ANDROID_LOG_DEBUG, RILWORKER_LOG_TAG, ##args); \
    }                                                                    \
  } while (0)

NS_IMPL_ISUPPORTS(nsRilWorker, nsIRilWorker)
static hidl_string HIDL_SERVICE_NAME[3] = {"slot1", "slot2", "slot3"};

/**
 *
 */
nsRilWorker::nsRilWorker(uint32_t aClientId) {
  DEBUG("init nsRilWorker");
  mRadioProxy = nullptr;
  mDeathRecipient = nullptr;
  mRilCallback = nullptr;
  mClientId = aClientId;
  mRilResponse = new nsRilResponse(this);
  mRilIndication = new nsRilIndication(this);
  updateDebug();
}

void nsRilWorker::updateDebug() {
  gRilDebug_isLoggingEnabled =
      mozilla::Preferences::GetBool("ril.debugging.enabled", false);
}

/**
 * nsIRadioInterface implementation
 */
NS_IMETHODIMP nsRilWorker::SendRilRequest(JS::HandleValue message) {
  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::InitRil(nsIRilCallback* callback) {
  mRilCallback = callback;
  GetRadioProxy();
  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetRadioPower(int32_t serial, bool enabled) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_RADIO_POWER on = %d", serial, enabled);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->setRadioPower(serial, enabled);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetDeviceIdentity(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_DEVICE_IDENTITY", serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getDeviceIdentity(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetVoiceRegistrationState(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_VOICE_REGISTRATION_STATE", serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getVoiceRegistrationState(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetDataRegistrationState(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_DATA_REGISTRATION_STATE", serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getDataRegistrationState(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetOperator(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_OPERATOR", serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getOperator(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetNetworkSelectionMode(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE", serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getNetworkSelectionMode(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetSignalStrength(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SIGNAL_STRENGTH", serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getSignalStrength(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetVoiceRadioTechnology(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_VOICE_RADIO_TECH", serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getVoiceRadioTechnology(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetIccCardStatus(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_SIM_STATUS", serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getIccCardStatus(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::ReportSmsMemoryStatus(int32_t serial,
                                                 bool available) {
  DEBUG(
      "nsRilWorker: [%d] > RIL_REQUEST_REPORT_SMS_MEMORY_STATUS available = %d",
      serial, available);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->reportSmsMemoryStatus(serial, available);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetCellInfoListRate(int32_t serial,
                                               int32_t rateInMillis) {
  DEBUG(
      "nsRilWorker: [%d] > RIL_REQUEST_SET_CELL_INFO_LIST_RATE rateInMillis = "
      "%d",
      serial, rateInMillis);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }

  if (rateInMillis == 0) {
    rateInMillis = INT32_MAX;
  }

  mRadioProxy->setCellInfoListRate(serial, rateInMillis);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetDataAllowed(int32_t serial, bool allowed) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_ALLOW_DATA allowed = %d", serial,
        allowed);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->setDataAllowed(serial, allowed);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetBasebandVersion(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_BASEBAND_VERSION", serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getBasebandVersion(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetUiccSubscription(int32_t serial, int32_t slotId,
                                               int32_t appIndex, int32_t subId,
                                               int32_t subStatus) {
  DEBUG(
      "nsRilWorker: [%d] > RIL_REQUEST_SET_UICC_SUBSCRIPTION slotId = %d "
      "appIndex = %d subId = %d subStatus = %d",
      serial, slotId, appIndex, subId, subStatus);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }

  SelectUiccSub info;
  info.slot = slotId;
  info.appIndex = appIndex;
  info.subType = SubscriptionType(subId);
  info.actStatus = UiccSubActStatus(subStatus);

  mRadioProxy->setUiccSubscription(serial, info);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetMute(int32_t serial, bool enableMute) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_MUTE enableMute = %d", serial,
        enableMute);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->setMute(serial, enableMute);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetMute(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_MUTE ", serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getMute(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetSmscAddress(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_SMSC_ADDRESS", serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getSmscAddress(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::RequestDial(int32_t serial, const nsAString& address,
                                       int32_t clirMode, int32_t uusType,
                                       int32_t uusDcs,
                                       const nsAString& uusData) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_DIAL", serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }

  UusInfo info;
  std::vector<UusInfo> uusInfo;
  info.uusType = UusType(uusType);
  info.uusDcs = UusDcs(uusDcs);
  if (uusData.Length() == 0) {
    info.uusData = NULL;
  } else {
    info.uusData = NS_ConvertUTF16toUTF8(uusData).get();
  }
  uusInfo.push_back(info);

  Dial dialInfo;
  dialInfo.address = NS_ConvertUTF16toUTF8(address).get();
  dialInfo.clir = Clir(clirMode);
  dialInfo.uusInfo = uusInfo;
  mRadioProxy->dial(serial, dialInfo);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetCurrentCalls(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_CURRENT_CALLS", serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getCurrentCalls(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::HangupConnection(int32_t serial, int32_t callIndex) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_HANGUP callIndex = %d", serial,
        callIndex);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->hangup(serial, callIndex);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::HangupWaitingOrBackground(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND", serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->hangupWaitingOrBackground(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::HangupForegroundResumeBackground(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND",
        serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->hangupForegroundResumeBackground(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SwitchWaitingOrHoldingAndActive(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE",
        serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->switchWaitingOrHoldingAndActive(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::Conference(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_CONFERENCE", serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->conference(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetLastCallFailCause(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_LAST_CALL_FAIL_CAUSE", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getLastCallFailCause(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::AcceptCall(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_ANSWER", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->acceptCall(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetPreferredNetworkType(int32_t serial,
                                                   int32_t networkType) {
  DEBUG(
      "nsRilWorker: [%d] > RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE "
      "networkType=%d",
      serial, networkType);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->setPreferredNetworkType(serial,
                                       PreferredNetworkType(networkType));

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetPreferredNetworkType(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getPreferredNetworkType(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetNetworkSelectionModeAutomatic(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC",
        serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->setNetworkSelectionModeAutomatic(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetNetworkSelectionModeManual(
    int32_t serial, const nsAString& operatorNumeric) {
  DEBUG(
      "nsRilWorker: [%d] > RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL "
      "operatorNumeric = %s",
      serial, NS_ConvertUTF16toUTF8(operatorNumeric).get());
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->setNetworkSelectionModeManual(
      serial, NS_ConvertUTF16toUTF8(operatorNumeric).get());

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetAvailableNetworks(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_QUERY_AVAILABLE_NETWORKS", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getAvailableNetworks(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetInitialAttachApn(int32_t serial,
                                               nsIDataProfile* profile,
                                               bool isRoaming) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_INITIAL_ATTACH_APN", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }

  bool modemCognitive;
  profile->GetModemCognitive(&modemCognitive);

  mRadioProxy->setInitialAttachApn(serial, convertToHalDataProfile(profile),
                                   modemCognitive, isRoaming);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetDataProfile(
    int32_t serial, const nsTArray<RefPtr<nsIDataProfile>>& profileList,
    bool isRoaming) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_DATA_PROFILE", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }

  std::vector<DataProfileInfo> dataProfileInfoList;
  for (uint32_t i = 0; i < profileList.Length(); i++) {
    DataProfileInfo profile = convertToHalDataProfile(profileList[i]);
    dataProfileInfoList.push_back(profile);
  }
  mRadioProxy->setDataProfile(serial, dataProfileInfoList, isRoaming);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetupDataCall(int32_t serial,
                                         int32_t radioTechnology,
                                         nsIDataProfile* profile,
                                         bool isRoaming, bool allowRoaming) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SETUP_DATA_CALL", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }

  bool modemCognitive;
  profile->GetModemCognitive(&modemCognitive);

  mRadioProxy->setupDataCall(serial, RadioTechnology(radioTechnology),
                             convertToHalDataProfile(profile), modemCognitive,
                             allowRoaming, isRoaming);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::DeactivateDataCall(int32_t serial, int32_t cid,
                                              int32_t reason) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_DEACTIVATE_DATA_CALL", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->deactivateDataCall(serial, cid, (reason == 0 ? false : true));

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetDataCallList(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_DATA_CALL_LIST", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getDataCallList(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetCellInfoList(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_CELL_INFO_LIST", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getCellInfoList(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetIMSI(int32_t serial, const nsAString& aid) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_IMSI aid = %s", serial,
        NS_ConvertUTF16toUTF8(aid).get());
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getImsiForApp(serial, NS_ConvertUTF16toUTF8(aid).get());

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::IccIOForApp(int32_t serial, int32_t command,
                                       int32_t fileId, const nsAString& path,
                                       int32_t p1, int32_t p2, int32_t p3,
                                       const nsAString& data,
                                       const nsAString& pin2,
                                       const nsAString& aid) {
  DEBUG(
      "nsRilWorker: [%d] > RIL_REQUEST_SIM_IO command = %d, fileId = %d, path "
      "= %s, p1 = %d, p2 = %d, p3 = %d, data = %s, pin2 = %s, aid = %s",
      serial, command, fileId, NS_ConvertUTF16toUTF8(path).get(), p1, p2, p3,
      NS_ConvertUTF16toUTF8(data).get(), NS_ConvertUTF16toUTF8(pin2).get(),
      NS_ConvertUTF16toUTF8(aid).get());
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }

  IccIo iccIo;
  iccIo.command = command;
  iccIo.fileId = fileId;
  iccIo.path = NS_ConvertUTF16toUTF8(path).get();
  iccIo.p1 = p1;
  iccIo.p2 = p2;
  iccIo.p3 = p3;
  iccIo.data = NS_ConvertUTF16toUTF8(data).get();
  iccIo.pin2 = NS_ConvertUTF16toUTF8(pin2).get();
  iccIo.aid = NS_ConvertUTF16toUTF8(aid).get();

  mRadioProxy->iccIOForApp(serial, iccIo);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetClir(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_CLIR", serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getClir(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetClir(int32_t serial, int32_t clirMode) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_CLIR clirMode = %d", serial,
        clirMode);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->setClir(serial, clirMode);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SendDtmf(int32_t serial, const nsAString& dtmfChar) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_DTMF dtmfChar = %s", serial,
        NS_ConvertUTF16toUTF8(dtmfChar).get());
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->sendDtmf(serial, NS_ConvertUTF16toUTF8(dtmfChar).get());

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::StartDtmf(int32_t serial,
                                     const nsAString& dtmfChar) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_DTMF_START dtmfChar = %s", serial,
        NS_ConvertUTF16toUTF8(dtmfChar).get());
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->startDtmf(serial, NS_ConvertUTF16toUTF8(dtmfChar).get());

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::StopDtmf(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_DTMF_STOP", serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->stopDtmf(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::RejectCall(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_UDUB", serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->rejectCall(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SendUssd(int32_t serial, const nsAString& ussd) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SEND_USSD ussd = %s", serial,
        NS_ConvertUTF16toUTF8(ussd).get());
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->sendUssd(serial, NS_ConvertUTF16toUTF8(ussd).get());

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::CancelPendingUssd(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_CANCEL_USSD", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->cancelPendingUssd(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetCallForwardStatus(int32_t serial,
                                                int32_t cfReason,
                                                int32_t serviceClass,
                                                const nsAString& number,
                                                int32_t toaNumber) {
  DEBUG(
      "nsRilWorker: [%d] > RIL_REQUEST_QUERY_CALL_FORWARD_STATUS cfReason = %d "
      ", serviceClass = %d, number = %s",
      serial, cfReason, serviceClass, NS_ConvertUTF16toUTF8(number).get());
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }

  CallForwardInfo cfInfo;
  cfInfo.reason = cfReason;
  cfInfo.serviceClass = serviceClass;
  cfInfo.toa = toaNumber;
  cfInfo.number = NS_ConvertUTF16toUTF8(number).get();
  cfInfo.timeSeconds = 0;

  mRadioProxy->getCallForwardStatus(serial, cfInfo);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetCallForwardStatus(int32_t serial, int32_t action,
                                                int32_t cfReason,
                                                int32_t serviceClass,
                                                const nsAString& number,
                                                int32_t toaNumber,
                                                int32_t timeSeconds) {
  DEBUG(
      "nsRilWorker: [%d] > RIL_REQUEST_SET_CALL_FORWARD action = %d, cfReason "
      "= %d , serviceClass = %d, number = %s, timeSeconds = %d",
      serial, action, cfReason, serviceClass,
      NS_ConvertUTF16toUTF8(number).get(), timeSeconds);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }

  CallForwardInfo cfInfo;
  cfInfo.status = CallForwardInfoStatus(action);
  cfInfo.reason = cfReason;
  cfInfo.serviceClass = serviceClass;
  cfInfo.toa = toaNumber;
  cfInfo.number = NS_ConvertUTF16toUTF8(number).get();
  cfInfo.timeSeconds = timeSeconds;

  mRadioProxy->setCallForward(serial, cfInfo);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetCallWaiting(int32_t serial,
                                          int32_t serviceClass) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_QUERY_CALL_WAITING serviceClass = %d",
        serial, serviceClass);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getCallWaiting(serial, serviceClass);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetCallWaiting(int32_t serial, bool enable,
                                          int32_t serviceClass) {
  DEBUG(
      "nsRilWorker: [%d] > RIL_REQUEST_SET_CALL_WAITING enable = %d, "
      "serviceClass = %d",
      serial, enable, serviceClass);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->setCallWaiting(serial, enable, serviceClass);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetBarringPassword(int32_t serial,
                                              const nsAString& facility,
                                              const nsAString& oldPwd,
                                              const nsAString& newPwd) {
  DEBUG(
      "nsRilWorker: [%d] > RIL_REQUEST_CHANGE_BARRING_PASSWORD facility = %s, "
      "oldPwd = %s, newPwd = %s",
      serial, NS_ConvertUTF16toUTF8(facility).get(),
      NS_ConvertUTF16toUTF8(oldPwd).get(), NS_ConvertUTF16toUTF8(newPwd).get());
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->setBarringPassword(serial, NS_ConvertUTF16toUTF8(facility).get(),
                                  NS_ConvertUTF16toUTF8(oldPwd).get(),
                                  NS_ConvertUTF16toUTF8(newPwd).get());

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SeparateConnection(int32_t serial,
                                              int32_t gsmIndex) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SEPARATE_CONNECTION gsmIndex = %d",
        serial, gsmIndex);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->separateConnection(serial, gsmIndex);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetClip(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_QUERY_CLIP", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getClip(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::ExplicitCallTransfer(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_EXPLICIT_CALL_TRANSFER", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->explicitCallTransfer(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetNeighboringCids(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_NEIGHBORING_CELL_IDS", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getNeighboringCids(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetTTYMode(int32_t serial, int32_t ttyMode) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_TTY_MODE ttyMode = %d", serial,
        ttyMode);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->setTTYMode(serial, TtyMode(ttyMode));

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::QueryTTYMode(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_QUERY_TTY_MODE ", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getTTYMode(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::ExitEmergencyCallbackMode(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE ",
        serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->exitEmergencyCallbackMode(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SupplyIccPinForApp(int32_t serial,
                                              const nsAString& pin,
                                              const nsAString& aid) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_ENTER_SIM_PIN pin = %s , aid = %s",
        serial, NS_ConvertUTF16toUTF8(pin).get(),
        NS_ConvertUTF16toUTF8(aid).get());
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->supplyIccPinForApp(serial, NS_ConvertUTF16toUTF8(pin).get(),
                                  NS_ConvertUTF16toUTF8(aid).get());

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SupplyIccPin2ForApp(int32_t serial,
                                               const nsAString& pin,
                                               const nsAString& aid) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_ENTER_SIM_PIN2 pin = %s , aid = %s",
        serial, NS_ConvertUTF16toUTF8(pin).get(),
        NS_ConvertUTF16toUTF8(aid).get());
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->supplyIccPin2ForApp(serial, NS_ConvertUTF16toUTF8(pin).get(),
                                   NS_ConvertUTF16toUTF8(aid).get());

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SupplyIccPukForApp(int32_t serial,
                                              const nsAString& puk,
                                              const nsAString& newPin,
                                              const nsAString& aid) {
  DEBUG(
      "nsRilWorker: [%d] > RIL_REQUEST_ENTER_SIM_PUK puk = %s , newPin = %s "
      ",aid = %s",
      serial, NS_ConvertUTF16toUTF8(puk).get(),
      NS_ConvertUTF16toUTF8(newPin).get(), NS_ConvertUTF16toUTF8(aid).get());
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->supplyIccPukForApp(serial, NS_ConvertUTF16toUTF8(puk).get(),
                                  NS_ConvertUTF16toUTF8(newPin).get(),
                                  NS_ConvertUTF16toUTF8(aid).get());

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SupplyIccPuk2ForApp(int32_t serial,
                                               const nsAString& puk,
                                               const nsAString& newPin,
                                               const nsAString& aid) {
  DEBUG(
      "nsRilWorker: [%d] > RIL_REQUEST_ENTER_SIM_PUK2 puk = %s , newPin = %s "
      ",aid = %s",
      serial, NS_ConvertUTF16toUTF8(puk).get(),
      NS_ConvertUTF16toUTF8(newPin).get(), NS_ConvertUTF16toUTF8(aid).get());
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->supplyIccPuk2ForApp(serial, NS_ConvertUTF16toUTF8(puk).get(),
                                   NS_ConvertUTF16toUTF8(newPin).get(),
                                   NS_ConvertUTF16toUTF8(aid).get());

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetFacilityLockForApp(
    int32_t serial, const nsAString& facility, bool lockState,
    const nsAString& password, int32_t serviceClass, const nsAString& aid) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_FACILITY_LOCK ", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->setFacilityLockForApp(
      serial, NS_ConvertUTF16toUTF8(facility).get(), lockState,
      NS_ConvertUTF16toUTF8(password).get(), serviceClass,
      NS_ConvertUTF16toUTF8(aid).get());

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetFacilityLockForApp(int32_t serial,
                                                 const nsAString& facility,
                                                 const nsAString& password,
                                                 int32_t serviceClass,
                                                 const nsAString& aid) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_FACILITY_LOCK ", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getFacilityLockForApp(
      serial, NS_ConvertUTF16toUTF8(facility).get(),
      NS_ConvertUTF16toUTF8(password).get(), serviceClass,
      NS_ConvertUTF16toUTF8(aid).get());

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::ChangeIccPinForApp(int32_t serial,
                                              const nsAString& oldPin,
                                              const nsAString& newPin,
                                              const nsAString& aid) {
  DEBUG(
      "nsRilWorker: [%d] > RIL_REQUEST_CHANGE_SIM_PIN oldPin = %s , newPin = "
      "%s ,aid = %s",
      serial, NS_ConvertUTF16toUTF8(oldPin).get(),
      NS_ConvertUTF16toUTF8(newPin).get(), NS_ConvertUTF16toUTF8(aid).get());
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->changeIccPinForApp(serial, NS_ConvertUTF16toUTF8(oldPin).get(),
                                  NS_ConvertUTF16toUTF8(newPin).get(),
                                  NS_ConvertUTF16toUTF8(aid).get());

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::ChangeIccPin2ForApp(int32_t serial,
                                               const nsAString& oldPin,
                                               const nsAString& newPin,
                                               const nsAString& aid) {
  DEBUG(
      "nsRilWorker: [%d] > RIL_REQUEST_CHANGE_SIM_PIN2 oldPin = %s , newPin = "
      "%s ,aid = %s",
      serial, NS_ConvertUTF16toUTF8(oldPin).get(),
      NS_ConvertUTF16toUTF8(newPin).get(), NS_ConvertUTF16toUTF8(aid).get());
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->changeIccPin2ForApp(serial, NS_ConvertUTF16toUTF8(oldPin).get(),
                                   NS_ConvertUTF16toUTF8(newPin).get(),
                                   NS_ConvertUTF16toUTF8(aid).get());

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::ReportStkServiceIsRunning(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING ",
        serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->reportStkServiceIsRunning(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetGsmBroadcastActivation(int32_t serial,
                                                     bool activate) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GSM_BROADCAST_ACTIVATION ", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->setGsmBroadcastActivation(serial, activate);
  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetGsmBroadcastConfig(
    int32_t serial, const nsTArray<int32_t>& ranges) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GSM_SET_BROADCAST_CONFIG ", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }

  std::vector<GsmBroadcastSmsConfigInfo> broadcastInfo;
  for (uint32_t i = 0; i < ranges.Length();) {
    GsmBroadcastSmsConfigInfo info;
    // convert [from, to) to [from, to - 1]
    info.fromServiceId = ranges[i++];
    info.toServiceId = ranges[i++] - 1;
    info.fromCodeScheme = 0x00;
    info.toCodeScheme = 0xFF;
    info.selected = 1;
    broadcastInfo.push_back(info);
  }

  mRadioProxy->setGsmBroadcastConfig(serial, broadcastInfo);
  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::GetPreferredVoicePrivacy(int32_t serial) {
  DEBUG(
      "nsRilWorker: [%d] > "
      "RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE ",
      serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }

  mRadioProxy->getPreferredVoicePrivacy(serial);
  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetPreferredVoicePrivacy(int32_t serial,
                                                    bool enable) {
  DEBUG(
      "nsRilWorker: [%d] > RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE "
      "enable = %d",
      serial, enable);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }

  mRadioProxy->setPreferredVoicePrivacy(serial, enable);
  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::RequestIccSimAuthentication(int32_t serial,
                                                       int32_t authContext,
                                                       const nsAString& data,
                                                       const nsAString& aid) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SIM_AUTHENTICATION ", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }

  mRadioProxy->requestIccSimAuthentication(serial, authContext,
                                           NS_ConvertUTF16toUTF8(data).get(),
                                           NS_ConvertUTF16toUTF8(aid).get());
  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SendSMS(int32_t serial, const nsAString& smsc,
                                   const nsAString& pdu) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SEND_SMS ", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }

  GsmSmsMessage smsMessage;
  smsMessage.smscPdu = NS_ConvertUTF16toUTF8(smsc).get();
  smsMessage.pdu = NS_ConvertUTF16toUTF8(pdu).get();
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SEND_SMS %s", serial,
        NS_ConvertUTF16toUTF8(pdu).get());

  mRadioProxy->sendSms(serial, smsMessage);
  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::AcknowledgeLastIncomingGsmSms(int32_t serial,
                                                         bool success,
                                                         int32_t cause) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SMS_ACKNOWLEDGE ", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }

  mRadioProxy->acknowledgeLastIncomingGsmSms(serial, success,
                                             SmsAcknowledgeFailCause(cause));
  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetSuppServiceNotifications(int32_t serial,
                                                    bool enable) {
  DEBUG(
      "nsRilWorker: [%d] > RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION "
      "enable = %d",
      serial, enable);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->setSuppServiceNotifications(serial, enable);
  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::HandleStkCallSetupRequestFromSim(int32_t serial,
                                                            bool accept) {
  DEBUG(
      "nsRilWorker: [%d] > "
      "RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM on = %d",
      serial, accept);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->handleStkCallSetupRequestFromSim(serial, accept);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SendTerminalResponseToSim(
    int32_t serial, const nsAString& contents) {
  DEBUG(
      "nsRilWorker: [%d] > RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE contents = "
      "%s",
      serial, NS_ConvertUTF16toUTF8(contents).get());
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->sendTerminalResponseToSim(serial,
                                         NS_ConvertUTF16toUTF8(contents).get());

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SendEnvelope(
    int32_t serial, const nsAString& contents) {
  DEBUG(
      "nsRilWorker: [%d] > RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND contents = "
      "%s",
      serial, NS_ConvertUTF16toUTF8(contents).get());
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->sendEnvelope(serial,
                            NS_ConvertUTF16toUTF8(contents).get());

  return NS_OK;
}

nsRilWorker::~nsRilWorker() {
  DEBUG("Destructor nsRilWorker");
  mRilResponse = nullptr;
  MOZ_ASSERT(!mRilResponse);
  mRilIndication = nullptr;
  MOZ_ASSERT(!mRilIndication);
  mRadioProxy = nullptr;
  MOZ_ASSERT(!mRadioProxy);
  mDeathRecipient = nullptr;
  MOZ_ASSERT(!mDeathRecipient);
}

void nsRilWorker::RadioProxyDeathRecipient::serviceDied(
    uint64_t, const ::android::wp<IBase>&) {
  INFO("nsRilWorker HAL died, cleanup instance.");
}

void nsRilWorker::GetRadioProxy() {
  if (mRadioProxy != nullptr) {
    return;
  }
  DEBUG("GetRadioProxy");

  mRadioProxy = IRadio::getService(HIDL_SERVICE_NAME[mClientId]);

  if (mRadioProxy != nullptr) {
    if (mDeathRecipient == nullptr) {
      mDeathRecipient = new RadioProxyDeathRecipient();
    }
    if (mDeathRecipient != nullptr) {
      Return<bool> linked =
          mRadioProxy->linkToDeath(mDeathRecipient, 0 /*cookie*/);
      if (!linked || !linked.isOk()) {
        ERROR("Failed to link to radio hal death notifications");
      }
    }
    DEBUG("setResponseFunctions");
    mRadioProxy->setResponseFunctions(mRilResponse, mRilIndication);
  } else {
    ERROR("Get Radio hal failed");
  }
}

void nsRilWorker::processIndication(RadioIndicationType indicationType) {
  DEBUG("processIndication, type= %d", indicationType);
  if (indicationType == RadioIndicationType::UNSOLICITED_ACK_EXP) {
    sendAck();
    DEBUG("Unsol response received; Sending ack to ril.cpp");
  } else {
    // ack is not expected to be sent back. Nothing is required to be done here.
  }
}

void nsRilWorker::processResponse(RadioResponseType responseType) {
  DEBUG("processResponse, type= %d", responseType);
  if (responseType == RadioResponseType::SOLICITED_ACK_EXP) {
    sendAck();
    DEBUG("Solicited response received; Sending ack to ril.cpp");
  } else {
    // ack is not expected to be sent back. Nothing is required to be done here.
  }
}

void nsRilWorker::sendAck() {
  DEBUG("sendAck");
  GetRadioProxy();
  if (mRadioProxy != nullptr) {
    mRadioProxy->responseAcknowledgement();
  } else {
    ERROR("sendAck mRadioProxy == nullptr");
  }
}

// Helper function
MvnoType nsRilWorker::convertToHalMvnoType(const nsAString& mvnoType) {
  if (u"imsi"_ns.Equals(mvnoType)) {
    return MvnoType::IMSI;
  } else if (u"gid"_ns.Equals(mvnoType)) {
    return MvnoType::GID;
  } else if (u"spn"_ns.Equals(mvnoType)) {
    return MvnoType::SPN;
  } else {
    return MvnoType::NONE;
  }
}

DataProfileInfo nsRilWorker::convertToHalDataProfile(nsIDataProfile* profile) {
  DataProfileInfo dataProfileInfo;

  int32_t profileId;
  profile->GetProfileId(&profileId);
  dataProfileInfo.profileId = DataProfileId(profileId);

  nsString apn;
  profile->GetApn(apn);
  dataProfileInfo.apn = NS_ConvertUTF16toUTF8(apn).get();

  nsString protocol;
  profile->GetProtocol(protocol);
  dataProfileInfo.protocol = NS_ConvertUTF16toUTF8(protocol).get();

  nsString roamingProtocol;
  profile->GetRoamingProtocol(roamingProtocol);
  dataProfileInfo.roamingProtocol =
      NS_ConvertUTF16toUTF8(roamingProtocol).get();

  int32_t authType;
  profile->GetAuthType(&authType);
  dataProfileInfo.authType = ApnAuthType(authType);

  nsString user;
  profile->GetUser(user);
  dataProfileInfo.user = NS_ConvertUTF16toUTF8(user).get();

  nsString password;
  profile->GetPassword(password);
  dataProfileInfo.password = NS_ConvertUTF16toUTF8(password).get();

  int32_t type;
  profile->GetType(&type);
  dataProfileInfo.type = DataProfileInfoType(type);

  int32_t maxConnsTime;
  profile->GetMaxConnsTime(&maxConnsTime);
  dataProfileInfo.maxConnsTime = maxConnsTime;

  int32_t maxConns;
  profile->GetMaxConns(&maxConns);
  dataProfileInfo.maxConns = maxConns;

  int32_t waitTime;
  profile->GetWaitTime(&waitTime);
  dataProfileInfo.waitTime = waitTime;

  bool enabled;
  profile->GetEnabled(&enabled);
  dataProfileInfo.enabled = enabled;

  int32_t supportedApnTypesBitmap;
  profile->GetSupportedApnTypesBitmap(&supportedApnTypesBitmap);
  dataProfileInfo.supportedApnTypesBitmap =
      (int32_t)ApnTypes(supportedApnTypesBitmap);

  int32_t bearerBitmap;
  profile->GetBearerBitmap(&bearerBitmap);
  dataProfileInfo.bearerBitmap = (int32_t)RadioAccessFamily(bearerBitmap);

  int32_t mtu;
  profile->GetMtu(&mtu);
  dataProfileInfo.mtu = mtu;

  nsString mvnoType;
  profile->GetMvnoType(mvnoType);
  dataProfileInfo.mvnoType = convertToHalMvnoType(mvnoType);

  nsString mvnoMatchData;
  profile->GetMvnoMatchData(mvnoMatchData);
  dataProfileInfo.mvnoMatchData = NS_ConvertUTF16toUTF8(mvnoMatchData).get();

  return dataProfileInfo;
}

NS_IMETHODIMP nsRilWorker::GetRadioCapability(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_RADIO_CAPABILITY", serial);

  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->getRadioCapability(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::StopNetworkScan(int32_t serial) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_STOP_NETWORK_SCAN", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->stopNetworkScan(serial);

  return NS_OK;
}

NS_IMETHODIMP nsRilWorker::SetUnsolResponseFilter(int32_t serial, int32_t filter) {
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_UNSOLICITED_RESPONSE_FILTER", serial);
  GetRadioProxy();
  if (mRadioProxy == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  mRadioProxy->setIndicationFilter(serial, filter);

  return NS_OK;
}

void nsRilWorker::sendRilIndicationResult(nsRilIndicationResult* aIndication) {
  DEBUG("nsRilWorker: [USOL]< %s",
        NS_LossyConvertUTF16toASCII(aIndication->mRilMessageType).get());

  RefPtr<nsRilIndicationResult> indication = aIndication;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "nsRilWorker::sendRilIndicationResult", [this, indication]() {
        if (mRilCallback) {
          mRilCallback->HandleRilIndication(indication);
        } else {
          DEBUG("sendRilIndicationResult: no callback");
        }
      });
  NS_DispatchToMainThread(r);
}

void nsRilWorker::sendRilResponseResult(nsRilResponseResult* aResponse) {
  DEBUG("nsRilWorker: [%d] < %s", aResponse->mRilMessageToken,
        NS_LossyConvertUTF16toASCII(aResponse->mRilMessageType).get());

  if (aResponse->mRilMessageToken > 0) {
    RefPtr<nsRilResponseResult> response = aResponse;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
        "nsRilWorker::sendRilResponseResult", [this, response]() {
          if (mRilCallback) {
            mRilCallback->HandleRilResponse(response);
          } else {
            DEBUG("sendRilResponseResult: no callback");
          }
        });
    NS_DispatchToMainThread(r);
  } else {
    DEBUG("ResponseResult internal reqeust.");
  }
}
