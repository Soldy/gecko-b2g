/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobileconnection_MobileConnectionIPCSerialiser_h
#define mozilla_dom_mobileconnection_MobileConnectionIPCSerialiser_h

#include "ipc/IPCMessageUtils.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "mozilla/dom/mobileconnection/MobileCallForwardingOptions.h"
#include "mozilla/dom/MobileCellInfo.h"
#include "mozilla/dom/MobileDeviceIdentities.h"
#include "mozilla/dom/MobileConnectionInfo.h"
#include "mozilla/dom/MobileNetworkInfo.h"
#include "mozilla/dom/MobileSignalStrength.h"
#include "mozilla/dom/MobileConnectionBinding.h"
#include "mozilla/dom/ScriptSettings.h"

using mozilla::AutoJSContext;
using mozilla::dom::MobileCellInfo;
using mozilla::dom::MobileConnectionInfo;
using mozilla::dom::MobileDeviceIdentities;
using mozilla::dom::MobileNetworkInfo;
using mozilla::dom::MobileSignalStrength;
using mozilla::dom::mobileconnection::MobileCallForwardingOptions;

typedef nsIMobileCellInfo* nsMobileCellInfo;
typedef nsIMobileConnectionInfo* nsMobileConnectionInfo;
typedef nsIMobileNetworkInfo* nsMobileNetworkInfo;
typedef nsIMobileCallForwardingOptions* nsMobileCallForwardingOptions;
typedef nsIMobileDeviceIdentities* nsMobileDeviceIdentities;
typedef nsIMobileSignalStrength* nsMobileSignalStrength;

namespace IPC {
template <>
struct ParamTraits<nsIMobileCallForwardingOptions*> {
  typedef nsIMobileCallForwardingOptions* paramType;

  // Function to serialize a MobileCallForwardingOptions.
  static void Write(Message* aMsg, const paramType& aParam) {
    bool isNull = !aParam;
    WriteParam(aMsg, isNull);
    // If it is a null object, then we are done.
    if (isNull) {
      return;
    }

    int16_t pShort;
    nsString pString;
    bool pBool;

    aParam->GetActive(&pBool);
    WriteParam(aMsg, pBool);

    aParam->GetAction(&pShort);
    WriteParam(aMsg, pShort);

    aParam->GetReason(&pShort);
    WriteParam(aMsg, pShort);

    aParam->GetNumber(pString);
    WriteParam(aMsg, pString);

    aParam->GetTimeSeconds(&pShort);
    WriteParam(aMsg, pShort);

    aParam->GetServiceClass(&pShort);
    WriteParam(aMsg, pShort);
  }

  // Function to de-serialize a MobileCallForwardingOptions.
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    // Check if is the null pointer we have transfered.
    bool isNull;
    if (!ReadParam(aMsg, aIter, &isNull)) {
      return false;
    }

    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    bool active;
    int16_t action;
    int16_t reason;
    nsString number;
    int16_t timeSeconds;
    int16_t serviceClass;

    // It's not important to us where it fails, but rather if it fails
    if (!(ReadParam(aMsg, aIter, &active) && ReadParam(aMsg, aIter, &action) &&
          ReadParam(aMsg, aIter, &reason) && ReadParam(aMsg, aIter, &number) &&
          ReadParam(aMsg, aIter, &timeSeconds) &&
          ReadParam(aMsg, aIter, &serviceClass))) {
      return false;
    }

    *aResult = new MobileCallForwardingOptions(active, action, reason, number,
                                               timeSeconds, serviceClass);

    // We release this ref after receiver finishes processing.
    NS_ADDREF(*aResult);

    return true;
  }
};

/**
 * nsIMobileNetworkInfo Serialize/De-serialize.
 */
template <>
struct ParamTraits<nsIMobileNetworkInfo*> {
  typedef nsIMobileNetworkInfo* paramType;

  // Function to serialize a MobileNetworkInfo.
  static void Write(Message* aMsg, const paramType& aParam) {
    bool isNull = !aParam;
    WriteParam(aMsg, isNull);
    // If it is a null object, then we are done.
    if (isNull) {
      return;
    }

    nsString pString;
    aParam->GetShortName(pString);
    WriteParam(aMsg, pString);

    aParam->GetLongName(pString);
    WriteParam(aMsg, pString);

    aParam->GetMcc(pString);
    WriteParam(aMsg, pString);

    aParam->GetMnc(pString);
    WriteParam(aMsg, pString);

    aParam->GetState(pString);
    WriteParam(aMsg, pString);

    // We release the ref here given that ipdl won't handle reference counting.
    aParam->Release();
  }

  // Function to de-serialize a MobileNetworkInfo.
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    // Check if is the null pointer we have transfered.
    bool isNull;
    if (!ReadParam(aMsg, aIter, &isNull)) {
      return false;
    }

    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    nsString shortName;
    nsString longName;
    nsString mcc;
    nsString mnc;
    nsString state;

    // It's not important to us where it fails, but rather if it fails
    if (!(ReadParam(aMsg, aIter, &shortName) &&
          ReadParam(aMsg, aIter, &longName) && ReadParam(aMsg, aIter, &mcc) &&
          ReadParam(aMsg, aIter, &mnc) && ReadParam(aMsg, aIter, &state))) {
      return false;
    }

    *aResult = new MobileNetworkInfo(shortName, longName, mcc, mnc, state);
    // We release this ref after receiver finishes processing.
    NS_ADDREF(*aResult);

    return true;
  }
};

/**
 * nsIMobileCellInfo Serialize/De-serialize.
 */
template <>
struct ParamTraits<nsIMobileCellInfo*> {
  typedef nsIMobileCellInfo* paramType;

  // Function to serialize a MobileCellInfo.
  static void Write(Message* aMsg, const paramType& aParam) {
    bool isNull = !aParam;
    WriteParam(aMsg, isNull);
    // If it is a null object, then we are done.
    if (isNull) {
      return;
    }

    int32_t pLong;
    int64_t pLongLong;
    int16_t pShort;
    bool pBool;

    aParam->GetGsmLocationAreaCode(&pLong);
    WriteParam(aMsg, pLong);

    aParam->GetGsmCellId(&pLongLong);
    WriteParam(aMsg, pLongLong);

    aParam->GetCdmaBaseStationId(&pLong);
    WriteParam(aMsg, pLong);

    aParam->GetCdmaBaseStationLatitude(&pLong);
    WriteParam(aMsg, pLong);

    aParam->GetCdmaBaseStationLongitude(&pLong);
    WriteParam(aMsg, pLong);

    aParam->GetCdmaSystemId(&pLong);
    WriteParam(aMsg, pLong);

    aParam->GetCdmaNetworkId(&pLong);
    WriteParam(aMsg, pLong);

    aParam->GetCdmaRoamingIndicator(&pShort);
    WriteParam(aMsg, pShort);

    aParam->GetCdmaDefaultRoamingIndicator(&pShort);
    WriteParam(aMsg, pShort);

    aParam->GetCdmaSystemIsInPRL(&pBool);
    WriteParam(aMsg, pBool);

    // We release the ref here given that ipdl won't handle reference counting.
    aParam->Release();
  }

  // Function to de-serialize a MobileCellInfo.
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    // Check if is the null pointer we have transfered.
    bool isNull;
    if (!ReadParam(aMsg, aIter, &isNull)) {
      return false;
    }

    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    int32_t gsmLac;
    int64_t gsmCellId;
    int32_t cdmaBsId;
    int32_t cdmaBsLat;
    int32_t cdmaBsLong;
    int32_t cdmaSystemId;
    int32_t cdmaNetworkId;
    int16_t cdmaRoamingIndicator;
    int16_t cdmaDefaultRoamingIndicator;
    bool cdmaSystemIsInPRL;

    // It's not important to us where it fails, but rather if it fails
    if (!(ReadParam(aMsg, aIter, &gsmLac) &&
          ReadParam(aMsg, aIter, &gsmCellId) &&
          ReadParam(aMsg, aIter, &cdmaBsId) &&
          ReadParam(aMsg, aIter, &cdmaBsLat) &&
          ReadParam(aMsg, aIter, &cdmaBsLong) &&
          ReadParam(aMsg, aIter, &cdmaSystemId) &&
          ReadParam(aMsg, aIter, &cdmaNetworkId) &&
          ReadParam(aMsg, aIter, &cdmaRoamingIndicator) &&
          ReadParam(aMsg, aIter, &cdmaDefaultRoamingIndicator) &&
          ReadParam(aMsg, aIter, &cdmaSystemIsInPRL))) {
      return false;
    }

    *aResult =
        new MobileCellInfo(gsmLac, gsmCellId, cdmaBsId, cdmaBsLat, cdmaBsLong,
                           cdmaSystemId, cdmaNetworkId, cdmaRoamingIndicator,
                           cdmaDefaultRoamingIndicator, cdmaSystemIsInPRL);
    // We release this ref after receiver finishes processing.
    NS_ADDREF(*aResult);

    return true;
  }
};

/**
 * nsIMobileConnectionInfo Serialize/De-serialize.
 */
template <>
struct ParamTraits<nsIMobileConnectionInfo*> {
  typedef nsIMobileConnectionInfo* paramType;

  // Function to serialize a MobileConnectionInfo.
  static void Write(Message* aMsg, const paramType& aParam) {
    bool isNull = !aParam;
    WriteParam(aMsg, isNull);
    // If it is a null object, then we are done.
    if (isNull) {
      return;
    }

    AutoJSContext cx;
    nsString pString;
    bool pBool;
    nsCOMPtr<nsIMobileNetworkInfo> pNetworkInfo;
    nsCOMPtr<nsIMobileCellInfo> pCellInfo;
    int32_t pLong;
    JS::Rooted<JS::Value> pJsval(cx);

    aParam->GetState(pString);
    WriteParam(aMsg, pString);

    aParam->GetConnected(&pBool);
    WriteParam(aMsg, pBool);

    aParam->GetEmergencyCallsOnly(&pBool);
    WriteParam(aMsg, pBool);

    aParam->GetRoaming(&pBool);
    WriteParam(aMsg, pBool);

    aParam->GetType(pString);
    WriteParam(aMsg, pString);

    aParam->GetNetwork(getter_AddRefs(pNetworkInfo));
    // Release ref when WriteParam is finished.
    WriteParam(aMsg, pNetworkInfo.forget().take());

    aParam->GetCell(getter_AddRefs(pCellInfo));
    // Release ref when WriteParam is finished.
    WriteParam(aMsg, pCellInfo.forget().take());

    aParam->GetReasonDataDenied(&pLong);
    WriteParam(aMsg, pLong);

    // We release the ref here given that ipdl won't handle reference counting.
    aParam->Release();
  }

  // Function to de-serialize a MobileConectionInfo.
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    // Check if is the null pointer we have transfered.
    bool isNull;
    if (!ReadParam(aMsg, aIter, &isNull)) {
      return false;
    }

    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    AutoJSContext cx;
    nsString state;
    bool connected;
    bool emergencyOnly;
    bool roaming;
    nsString type;
    nsIMobileNetworkInfo* networkInfo = nullptr;
    nsIMobileCellInfo* cellInfo = nullptr;
    int32_t reasonDataDenied;

    // It's not important to us where it fails, but rather if it fails
    if (!(ReadParam(aMsg, aIter, &state) &&
          ReadParam(aMsg, aIter, &connected) &&
          ReadParam(aMsg, aIter, &emergencyOnly) &&
          ReadParam(aMsg, aIter, &roaming) && ReadParam(aMsg, aIter, &type) &&
          ReadParam(aMsg, aIter, &networkInfo) &&
          ReadParam(aMsg, aIter, &cellInfo) &&
          ReadParam(aMsg, aIter, &reasonDataDenied))) {
      return false;
    }

    *aResult =
        new MobileConnectionInfo(state, connected, emergencyOnly, roaming,
                                 networkInfo, type, cellInfo, reasonDataDenied);
    // We release this ref after receiver finishes processing.
    NS_ADDREF(*aResult);
    // We already clone the data into MobileConnectionInfo, so release the ref
    // of networkInfo and cellInfo here.
    NS_IF_RELEASE(networkInfo);
    NS_IF_RELEASE(cellInfo);

    return true;
  }
};

/**
 * nsIMobileDeviceIdentities Serialize/De-serialize.
 */
template <>
struct ParamTraits<nsIMobileDeviceIdentities*> {
  typedef nsIMobileDeviceIdentities* paramType;

  // Function to serialize a MobileDeviceIdentities.
  static void Write(Message* aMsg, const paramType& aParam) {
    bool isNull = !aParam;
    WriteParam(aMsg, isNull);

    // If it is a null object, then we are done.
    if (isNull) {
      return;
    }

    AutoJSContext cx;
    nsString pString;
    JS::Rooted<JS::Value> pJsval(cx);

    aParam->GetImei(pString);
    WriteParam(aMsg, pString);

    aParam->GetImeisv(pString);
    WriteParam(aMsg, pString);

    aParam->GetEsn(pString);
    WriteParam(aMsg, pString);

    aParam->GetMeid(pString);
    WriteParam(aMsg, pString);

    // We release the ref here given that ipdl won't handle reference counting.
    aParam->Release();
  }

  // Function to de-serialize a MobileDeviceIdentities.
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    // Check if is the null pointer we have transfered.
    bool isNull;
    if (!ReadParam(aMsg, aIter, &isNull)) {
      return false;
    }

    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    AutoJSContext cx;
    nsString imei;
    nsString imeisv;
    nsString esn;
    nsString meid;

    // It's not important to us where it fails, but rather if it fails
    if (!(ReadParam(aMsg, aIter, &imei) && ReadParam(aMsg, aIter, &imeisv) &&
          ReadParam(aMsg, aIter, &esn) && ReadParam(aMsg, aIter, &meid))) {
      return false;
    }

    *aResult = new MobileDeviceIdentities(imei, imeisv, esn, meid);

    // We release this ref after receiver finishes processing.
    NS_ADDREF(*aResult);

    return true;
  }
};

/**
 * nsIMobileSignalStrength Serialize/De-serialize.
 */
template <>
struct ParamTraits<nsIMobileSignalStrength*> {
  typedef nsIMobileSignalStrength* paramType;

  // Function to serialize a nsIMobileSignalStrength.
  static void Write(Message* aMsg, const paramType& aParam) {
    bool isNull = !aParam;
    WriteParam(aMsg, isNull);
    // If it is a null object, then we are done.
    if (isNull) {
      return;
    }

    int16_t pShort;
    int32_t pLong;

    aParam->GetLevel(&pShort);
    WriteParam(aMsg, pShort);

    aParam->GetGsmSignalStrength(&pShort);
    WriteParam(aMsg, pShort);
    aParam->GetGsmBitErrorRate(&pShort);
    WriteParam(aMsg, pShort);

    aParam->GetCdmaDbm(&pShort);
    WriteParam(aMsg, pShort);
    aParam->GetCdmaEcio(&pShort);
    WriteParam(aMsg, pShort);
    aParam->GetCdmaEvdoDbm(&pShort);
    WriteParam(aMsg, pShort);
    aParam->GetCdmaEvdoEcio(&pShort);
    WriteParam(aMsg, pShort);
    aParam->GetCdmaEvdoSNR(&pShort);
    WriteParam(aMsg, pShort);

    aParam->GetLteSignalStrength(&pShort);
    WriteParam(aMsg, pShort);
    aParam->GetLteRsrp(&pLong);
    WriteParam(aMsg, pLong);
    aParam->GetLteRsrq(&pLong);
    WriteParam(aMsg, pLong);
    aParam->GetLteRssnr(&pLong);
    WriteParam(aMsg, pLong);
    aParam->GetLteCqi(&pLong);
    WriteParam(aMsg, pLong);
    aParam->GetLteTimingAdvance(&pLong);
    WriteParam(aMsg, pLong);

    aParam->GetTdscdmaRscp(&pLong);
    WriteParam(aMsg, pLong);
  }

  // Function to de-serialize a MobileDeviceIdentities.
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    // Check if is the null pointer we have transfered.
    bool isNull;
    if (!ReadParam(aMsg, aIter, &isNull)) {
      return false;
    }

    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    int16_t level;
    int16_t gsmSignalStrength;
    int16_t gsmBitErrorRate;
    int16_t cdmaDbm;
    int16_t cdmaEcio;
    int16_t cdmaEvdoDbm;
    int16_t cdmaEvdoEcio;
    int16_t cdmaEvdoSNR;
    int16_t lteSignalStrength;
    int32_t lteRsrp;
    int32_t lteRsrq;
    int32_t lteRssnr;
    int32_t lteCqi;
    int32_t lteTimingAdvance;
    int32_t tdscdmaRscp;

    // It's not important to us where it fails, but rather if it fails
    if (!(ReadParam(aMsg, aIter, &level) &&
          ReadParam(aMsg, aIter, &gsmSignalStrength) &&
          ReadParam(aMsg, aIter, &gsmBitErrorRate) &&
          ReadParam(aMsg, aIter, &cdmaDbm) &&
          ReadParam(aMsg, aIter, &cdmaEcio) &&
          ReadParam(aMsg, aIter, &cdmaEvdoDbm) &&
          ReadParam(aMsg, aIter, &cdmaEvdoEcio) &&
          ReadParam(aMsg, aIter, &cdmaEvdoSNR) &&
          ReadParam(aMsg, aIter, &lteSignalStrength) &&
          ReadParam(aMsg, aIter, &lteRsrp) &&
          ReadParam(aMsg, aIter, &lteRsrq) &&
          ReadParam(aMsg, aIter, &lteRssnr) &&
          ReadParam(aMsg, aIter, &lteCqi) &&
          ReadParam(aMsg, aIter, &lteTimingAdvance) &&
          ReadParam(aMsg, aIter, &tdscdmaRscp))) {
      return false;
    }

    *aResult = new MobileSignalStrength(
        level, gsmSignalStrength, gsmBitErrorRate, cdmaDbm, cdmaEcio,
        cdmaEvdoDbm, cdmaEvdoEcio, cdmaEvdoSNR, lteSignalStrength, lteRsrp,
        lteRsrq, lteRssnr, lteCqi, lteTimingAdvance, tdscdmaRscp);

    // We release this ref after receiver finishes processing.
    NS_ADDREF(*aResult);

    return true;
  }
};

}  // namespace IPC

#endif  // mozilla_dom_mobileconnection_MobileConnectionIPCSerialiser_h
