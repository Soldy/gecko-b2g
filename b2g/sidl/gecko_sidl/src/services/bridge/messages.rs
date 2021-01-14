/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is generated. Do not edit.
// @generated

#[allow(unused_imports)]
use crate::common::{JsonValue, ObjectRef, SystemTime};
use serde::{Deserialize, Serialize};

pub static SERVICE_FINGERPRINT: &str =
    "c3459eb31483ae8ab5299bc3e983399f45953b5b04c2b2991ecaa61c34c761e";

#[derive(Clone, PartialEq, Deserialize, Serialize, Debug)]
pub enum CardInfoType {
    Imei,   // #0
    Imsi,   // #1
    Msisdn, // #2
}
impl Copy for CardInfoType {}

#[derive(Clone, PartialEq, Deserialize, Serialize, Debug)]
pub enum NetworkState {
    NetworkStateUnknown,       // #0
    NetworkStateConnecting,    // #1
    NetworkStateConnected,     // #2
    NetworkStateDisconnecting, // #3
    NetworkStateDisconnected,  // #4
    NetworkStateEnabled,       // #5
    NetworkStateDisabled,      // #6
    NetworkStateSuspended,     // #7
}
impl Copy for NetworkState {}

#[derive(Clone, PartialEq, Deserialize, Serialize, Debug)]
pub enum NetworkType {
    NetworkTypeUnknown,     // #0
    NetworkTypeWifi,        // #1
    NetworkTypeMobile,      // #2
    NetworkTypeMobileMms,   // #3
    NetworkTypeMobileSupl,  // #4
    NetworkTypeWifiP2p,     // #5
    NetworkTypeMobileIms,   // #6
    NetworkTypeMobileDun,   // #7
    NetworkTypeMobileFota,  // #8
    NetworkTypeEthernet,    // #9
    NetworkTypeMobileHipri, // #10
    NetworkTypeMobileCbs,   // #11
    NetworkTypeMobileIa,    // #12
    NetworkTypeMobileEcc,   // #13
    NetworkTypeMobileXcap,  // #14
}
impl Copy for NetworkType {}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct NetworkInfo {
    pub network_state: NetworkState,
    pub network_type: NetworkType,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct NetworkOperator {
    pub mnc: String,
    pub mcc: String,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct SimContactInfo {
    pub id: String,
    pub tel: String,
    pub email: String,
    pub name: String,
    pub category: String,
}

#[derive(Debug, Deserialize, Serialize)]
pub enum GeckoBridgeFromClient {
    GeckoFeaturesBoolPrefChanged(String, bool),   // 0
    GeckoFeaturesCharPrefChanged(String, String), // 1
    GeckoFeaturesImportSimContacts(Option<Vec<SimContactInfo>>), // 2
    GeckoFeaturesIntPrefChanged(String, i64),     // 3
    GeckoFeaturesRegisterToken(String, String, Option<Vec<String>>), // 4
    GeckoFeaturesSetAppsServiceDelegate(ObjectRef), // 5
    GeckoFeaturesSetMobileManagerDelegate(ObjectRef), // 6
    GeckoFeaturesSetNetworkManagerDelegate(ObjectRef), // 7
    GeckoFeaturesSetPowerManagerDelegate(ObjectRef), // 8
    GeckoFeaturesSetPreferenceDelegate(ObjectRef), // 9
    AppsServiceDelegateOnBootSuccess,             // 10
    AppsServiceDelegateOnBootError,               // 11
    AppsServiceDelegateOnBootDoneSuccess,         // 12
    AppsServiceDelegateOnBootDoneError,           // 13
    AppsServiceDelegateOnClearSuccess,            // 14
    AppsServiceDelegateOnClearError,              // 15
    AppsServiceDelegateOnInstallSuccess,          // 16
    AppsServiceDelegateOnInstallError,            // 17
    AppsServiceDelegateOnUninstallSuccess,        // 18
    AppsServiceDelegateOnUninstallError,          // 19
    AppsServiceDelegateOnUpdateSuccess,           // 20
    AppsServiceDelegateOnUpdateError,             // 21
    MobileManagerDelegateGetCardInfoSuccess(String), // 22
    MobileManagerDelegateGetCardInfoError,        // 23
    MobileManagerDelegateGetMncMccSuccess(NetworkOperator), // 24
    MobileManagerDelegateGetMncMccError,          // 25
    NetworkManagerDelegateGetNetworkInfoSuccess(NetworkInfo), // 26
    NetworkManagerDelegateGetNetworkInfoError,    // 27
    PowerManagerDelegateRequestWakelockSuccess(ObjectRef), // 28
    PowerManagerDelegateRequestWakelockError,     // 29
    PowerManagerDelegateSetScreenEnabledSuccess,  // 30
    PowerManagerDelegateSetScreenEnabledError,    // 31
    PreferenceDelegateGetBoolSuccess(bool),       // 32
    PreferenceDelegateGetBoolError,               // 33
    PreferenceDelegateGetCharSuccess(String),     // 34
    PreferenceDelegateGetCharError,               // 35
    PreferenceDelegateGetIntSuccess(i64),         // 36
    PreferenceDelegateGetIntError,                // 37
    PreferenceDelegateSetBoolSuccess,             // 38
    PreferenceDelegateSetBoolError,               // 39
    PreferenceDelegateSetCharSuccess,             // 40
    PreferenceDelegateSetCharError,               // 41
    PreferenceDelegateSetIntSuccess,              // 42
    PreferenceDelegateSetIntError,                // 43
    WakelockGetTopicSuccess(String),              // 44
    WakelockGetTopicError,                        // 45
    WakelockUnlockSuccess,                        // 46
    WakelockUnlockError,                          // 47
}

#[derive(Debug, Deserialize)]
pub enum GeckoBridgeToClient {
    GeckoFeaturesBoolPrefChangedSuccess,                 // 0
    GeckoFeaturesBoolPrefChangedError,                   // 1
    GeckoFeaturesCharPrefChangedSuccess,                 // 2
    GeckoFeaturesCharPrefChangedError,                   // 3
    GeckoFeaturesImportSimContactsSuccess,               // 4
    GeckoFeaturesImportSimContactsError,                 // 5
    GeckoFeaturesIntPrefChangedSuccess,                  // 6
    GeckoFeaturesIntPrefChangedError,                    // 7
    GeckoFeaturesRegisterTokenSuccess,                   // 8
    GeckoFeaturesRegisterTokenError,                     // 9
    GeckoFeaturesSetAppsServiceDelegateSuccess,          // 10
    GeckoFeaturesSetAppsServiceDelegateError,            // 11
    GeckoFeaturesSetMobileManagerDelegateSuccess,        // 12
    GeckoFeaturesSetMobileManagerDelegateError,          // 13
    GeckoFeaturesSetNetworkManagerDelegateSuccess,       // 14
    GeckoFeaturesSetNetworkManagerDelegateError,         // 15
    GeckoFeaturesSetPowerManagerDelegateSuccess,         // 16
    GeckoFeaturesSetPowerManagerDelegateError,           // 17
    GeckoFeaturesSetPreferenceDelegateSuccess,           // 18
    GeckoFeaturesSetPreferenceDelegateError,             // 19
    AppsServiceDelegateOnBoot(String, JsonValue),        // 20
    AppsServiceDelegateOnBootDone,                       // 21
    AppsServiceDelegateOnClear(String, String),          // 22
    AppsServiceDelegateOnInstall(String, JsonValue),     // 23
    AppsServiceDelegateOnUninstall(String),              // 24
    AppsServiceDelegateOnUpdate(String, JsonValue),      // 25
    MobileManagerDelegateGetCardInfo(i64, CardInfoType), // 26
    MobileManagerDelegateGetMncMcc(i64, bool),           // 27
    NetworkManagerDelegateGetNetworkInfo,                // 28
    PowerManagerDelegateRequestWakelock(String),         // 29
    PowerManagerDelegateSetScreenEnabled(bool, bool),    // 30
    PreferenceDelegateGetBool(String),                   // 31
    PreferenceDelegateGetChar(String),                   // 32
    PreferenceDelegateGetInt(String),                    // 33
    PreferenceDelegateSetBool(String, bool),             // 34
    PreferenceDelegateSetChar(String, String),           // 35
    PreferenceDelegateSetInt(String, i64),               // 36
    WakelockGetTopic,                                    // 37
    WakelockUnlock,                                      // 38
}
