/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is generated. Do not edit.
// @generated

#[allow(unused_imports)]
use crate::common::{JsonValue, ObjectRef, SystemTime};
use serde::{Deserialize, Serialize};

pub static SERVICE_FINGERPRINT: &str =
    "9f69c36e87a5b62281b68e5a0fa75f5d6f764a686862173c1dca72e07c715";

#[derive(Clone, PartialEq, Deserialize, Serialize, Debug)]
pub enum ChangeReason {
    Create, // #0
    Update, // #1
    Remove, // #2
}
impl Copy for ChangeReason {}

#[derive(Clone, PartialEq, Deserialize, Serialize, Debug)]
pub enum FilterByOption {
    Name,       // #0
    GivenName,  // #1
    FamilyName, // #2
    Tel,        // #3
    Email,      // #4
    Category,   // #5
}
impl Copy for FilterByOption {}

#[derive(Clone, PartialEq, Deserialize, Serialize, Debug)]
pub enum FilterOption {
    Equals,     // #0
    Contains,   // #1
    Match,      // #2
    StartsWith, // #3
    FuzzyMatch, // #4
}
impl Copy for FilterOption {}

#[derive(Clone, PartialEq, Deserialize, Serialize, Debug)]
pub enum Order {
    Ascending,  // #0
    Descending, // #1
}
impl Copy for Order {}

#[derive(Clone, PartialEq, Deserialize, Serialize, Debug)]
pub enum SortOption {
    GivenName,  // #0
    FamilyName, // #1
    Name,       // #2
}
impl Copy for SortOption {}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct Address {
    pub atype: String,
    pub street_address: Option<String>,
    pub locality: Option<String>,
    pub region: Option<String>,
    pub postal_code: Option<String>,
    pub country_name: Option<String>,
    pub pref: Option<bool>,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct BlockedNumberChangeEvent {
    pub reason: ChangeReason,
    pub number: String,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct BlockedNumberFindOptions {
    pub filter_value: String,
    pub filter_option: FilterOption,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct ContactField {
    pub atype: String,
    pub value: String,
    pub pref: bool,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct ContactFindSortOptions {
    pub sort_by: SortOption,
    pub sort_order: Order,
    pub sort_language: String,
    pub filter_value: String,
    pub filter_option: FilterOption,
    pub filter_by: Vec<FilterByOption>,
    pub only_main_data: bool,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct ContactInfo {
    pub id: String,
    pub published: SystemTime,
    pub updated: SystemTime,
    pub bday: SystemTime,
    pub anniversary: SystemTime,
    pub sex: String,
    pub gender_identity: String,
    pub ringtone: String,
    pub photo_type: String,
    pub photo_blob: Vec<u8>,
    pub addresses: Option<Vec<Address>>,
    pub email: Option<Vec<ContactField>>,
    pub url: Option<Vec<ContactField>>,
    pub name: String,
    pub tel: Option<Vec<ContactTelField>>,
    pub honorific_prefix: Option<Vec<String>>,
    pub given_name: String,
    pub phonetic_given_name: String,
    pub additional_name: Option<Vec<String>>,
    pub family_name: String,
    pub phonetic_family_name: String,
    pub honorific_suffix: Option<Vec<String>>,
    pub nickname: Option<Vec<String>>,
    pub category: Option<Vec<String>>,
    pub org: Option<Vec<String>>,
    pub job_title: Option<Vec<String>>,
    pub note: Option<Vec<String>>,
    pub groups: Option<Vec<String>>,
    pub ice_position: i64,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct ContactSortOptions {
    pub sort_by: SortOption,
    pub sort_order: Order,
    pub sort_language: String,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct ContactTelField {
    pub atype: String,
    pub value: String,
    pub pref: bool,
    pub carrier: String,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct ContactsChangeEvent {
    pub reason: ChangeReason,
    pub contacts: Option<Vec<ContactInfo>>,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct GroupChangeEvent {
    pub reason: ChangeReason,
    pub group: GroupInfo,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct GroupInfo {
    pub id: String,
    pub name: String,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct IceInfo {
    pub position: i64,
    pub contact_id: String,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct SimContactInfo {
    pub id: String,
    pub tel: String,
    pub email: String,
    pub name: String,
    pub category: String,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct SimContactLoadedEvent {
    pub remove_count: i64,
    pub update_count: i64,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct SpeedDialChangeEvent {
    pub reason: ChangeReason,
    pub speeddial: SpeedDialInfo,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct SpeedDialInfo {
    pub dial_key: String,
    pub tel: String,
    pub contact_id: String,
}

#[derive(Debug, Deserialize, Serialize)]
pub enum ContactsManagerFromClient {
    ContactCursorNext,                                            // 0
    ContactsFactoryAdd(Vec<ContactInfo>),                         // 1
    ContactsFactoryAddBlockedNumber(String),                      // 2
    ContactsFactoryAddGroup(String),                              // 3
    ContactsFactoryAddSpeedDial(String, String, String),          // 4
    ContactsFactoryClearContacts,                                 // 5
    ContactsFactoryFind(ContactFindSortOptions, i64),             // 6
    ContactsFactoryFindBlockedNumbers(BlockedNumberFindOptions),  // 7
    ContactsFactoryGet(String, bool),                             // 8
    ContactsFactoryGetAll(ContactSortOptions, i64, bool),         // 9
    ContactsFactoryGetAllBlockedNumbers,                          // 10
    ContactsFactoryGetAllGroups,                                  // 11
    ContactsFactoryGetAllIce,                                     // 12
    ContactsFactoryGetContactidsFromGroup(String),                // 13
    ContactsFactoryGetCount,                                      // 14
    ContactsFactoryGetSpeedDials,                                 // 15
    ContactsFactoryImportVcf(String),                             // 16
    ContactsFactoryMatches(FilterByOption, FilterOption, String), // 17
    ContactsFactoryRemove(Vec<String>),                           // 18
    ContactsFactoryRemoveBlockedNumber(String),                   // 19
    ContactsFactoryRemoveGroup(String),                           // 20
    ContactsFactoryRemoveIce(String),                             // 21
    ContactsFactoryRemoveSpeedDial(String),                       // 22
    ContactsFactorySetIce(String, i64),                           // 23
    ContactsFactoryUpdate(Vec<ContactInfo>),                      // 24
    ContactsFactoryUpdateGroup(String, String),                   // 25
    ContactsFactoryUpdateSpeedDial(String, String, String),       // 26
}

#[derive(Debug, Deserialize)]
pub enum ContactsManagerToClient {
    ContactCursorNextSuccess(Vec<ContactInfo>), // 0
    ContactCursorNextError,                     // 1
    ContactsFactoryAddSuccess,                  // 2
    ContactsFactoryAddError,                    // 3
    ContactsFactoryAddBlockedNumberSuccess,     // 4
    ContactsFactoryAddBlockedNumberError,       // 5
    ContactsFactoryAddGroupSuccess,             // 6
    ContactsFactoryAddGroupError,               // 7
    ContactsFactoryAddSpeedDialSuccess,         // 8
    ContactsFactoryAddSpeedDialError,           // 9
    ContactsFactoryClearContactsSuccess,        // 10
    ContactsFactoryClearContactsError,          // 11
    ContactsFactoryFindSuccess(u32),            // 12
    ContactsFactoryFindError,                   // 13
    ContactsFactoryFindBlockedNumbersSuccess(Option<Vec<String>>), // 14
    ContactsFactoryFindBlockedNumbersError,     // 15
    ContactsFactoryGetSuccess(ContactInfo),     // 16
    ContactsFactoryGetError,                    // 17
    ContactsFactoryGetAllSuccess(u32),          // 18
    ContactsFactoryGetAllError,                 // 19
    ContactsFactoryGetAllBlockedNumbersSuccess(Option<Vec<String>>), // 20
    ContactsFactoryGetAllBlockedNumbersError,   // 21
    ContactsFactoryGetAllGroupsSuccess(Option<Vec<GroupInfo>>), // 22
    ContactsFactoryGetAllGroupsError,           // 23
    ContactsFactoryGetAllIceSuccess(Option<Vec<IceInfo>>), // 24
    ContactsFactoryGetAllIceError,              // 25
    ContactsFactoryGetContactidsFromGroupSuccess(Option<Vec<String>>), // 26
    ContactsFactoryGetContactidsFromGroupError, // 27
    ContactsFactoryGetCountSuccess(i64),        // 28
    ContactsFactoryGetCountError,               // 29
    ContactsFactoryGetSpeedDialsSuccess(Option<Vec<SpeedDialInfo>>), // 30
    ContactsFactoryGetSpeedDialsError,          // 31
    ContactsFactoryImportVcfSuccess(i64),       // 32
    ContactsFactoryImportVcfError,              // 33
    ContactsFactoryMatchesSuccess(bool),        // 34
    ContactsFactoryMatchesError,                // 35
    ContactsFactoryRemoveSuccess,               // 36
    ContactsFactoryRemoveError,                 // 37
    ContactsFactoryRemoveBlockedNumberSuccess,  // 38
    ContactsFactoryRemoveBlockedNumberError,    // 39
    ContactsFactoryRemoveGroupSuccess,          // 40
    ContactsFactoryRemoveGroupError,            // 41
    ContactsFactoryRemoveIceSuccess,            // 42
    ContactsFactoryRemoveIceError,              // 43
    ContactsFactoryRemoveSpeedDialSuccess,      // 44
    ContactsFactoryRemoveSpeedDialError,        // 45
    ContactsFactorySetIceSuccess,               // 46
    ContactsFactorySetIceError,                 // 47
    ContactsFactoryUpdateSuccess,               // 48
    ContactsFactoryUpdateError,                 // 49
    ContactsFactoryUpdateGroupSuccess,          // 50
    ContactsFactoryUpdateGroupError,            // 51
    ContactsFactoryUpdateSpeedDialSuccess,      // 52
    ContactsFactoryUpdateSpeedDialError,        // 53
    ContactsFactoryBlockednumberChangeEvent(BlockedNumberChangeEvent), // 54
    ContactsFactoryContactsChangeEvent(ContactsChangeEvent), // 55
    ContactsFactoryGroupChangeEvent(GroupChangeEvent), // 56
    ContactsFactorySimContactLoadedEvent(SimContactLoadedEvent), // 57
    ContactsFactorySpeeddialChangeEvent(SpeedDialChangeEvent), // 58
}
