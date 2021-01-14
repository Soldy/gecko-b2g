/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Wrapper for the nsIPreferenceDelegate interface.

use super::messages::*;
use crate::common::core::{BaseMessage, BaseMessageKind};
use crate::common::traits::TrackerId;
use crate::common::uds_transport::UdsTransport;
use crate::common::uds_transport::{from_base_message, SessionObject, XpcomSessionObject};
use bincode::Options;
use log::error;
use moz_task::{Task, TaskRunnable, ThreadPtrHandle};
use nserror::{nsresult, NS_OK};
use nsstring::*;
use std::any::Any;
use xpcom::interfaces::nsIPreferenceDelegate;

pub struct PreferenceDelegate {
    xpcom: ThreadPtrHandle<nsIPreferenceDelegate>,
    service_id: TrackerId,
    object_id: TrackerId,
    transport: UdsTransport,
}

impl PreferenceDelegate {
    pub fn new(
        xpcom: ThreadPtrHandle<nsIPreferenceDelegate>,
        service_id: TrackerId,
        object_id: TrackerId,
        transport: &UdsTransport,
    ) -> Self {
        Self {
            xpcom,
            service_id,
            object_id,
            transport: transport.clone(),
        }
    }

    fn post_task(&mut self, command: PreferenceCommand, request_id: u64) {
        let task = PreferenceDelegateTask {
            xpcom: self.xpcom.clone(),
            command,
            transport: self.transport.clone(),
            service_id: self.service_id,
            object_id: self.object_id,
            request_id,
        };
        let _ = TaskRunnable::new("PreferenceDelegate", Box::new(task))
            .and_then(|r| TaskRunnable::dispatch(r, self.xpcom.owning_thread()));
    }
}

impl SessionObject for PreferenceDelegate {
    fn on_request(&mut self, request: BaseMessage, request_id: u64) -> Option<BaseMessage> {
        // Unpack the request.
        match from_base_message(&request) {
            Ok(GeckoBridgeToClient::PreferenceDelegateGetInt(key)) => {
                self.post_task(PreferenceCommand::GetInt(key), request_id);
            }
            Ok(GeckoBridgeToClient::PreferenceDelegateGetChar(key)) => {
                self.post_task(PreferenceCommand::GetChar(key), request_id);
            }
            Ok(GeckoBridgeToClient::PreferenceDelegateGetBool(key)) => {
                self.post_task(PreferenceCommand::GetBool(key), request_id);
            }
            Ok(GeckoBridgeToClient::PreferenceDelegateSetInt(key, value)) => {
                self.post_task(PreferenceCommand::SetInt(key, value), request_id);
            }
            Ok(GeckoBridgeToClient::PreferenceDelegateSetChar(key, value)) => {
                self.post_task(PreferenceCommand::SetChar(key, value), request_id);
            }
            Ok(GeckoBridgeToClient::PreferenceDelegateSetBool(key, value)) => {
                self.post_task(PreferenceCommand::SetBool(key, value), request_id);
            }
            _ => error!(
                "PreferenceDelegate::on_request unexpected message: {:?}",
                request.content
            ),
        }
        None
    }

    fn on_event(&mut self, _event: Vec<u8>) {}

    fn get_ids(&self) -> (u32, u32) {
        (self.service_id, self.object_id)
    }
}

impl XpcomSessionObject for PreferenceDelegate {
    fn as_xpcom(&self) -> &dyn Any {
        &self.xpcom
    }
}

// Commands supported by the card info manager delegate.
#[derive(Clone)]
enum PreferenceCommand {
    GetInt(
        String, // key
    ),
    GetChar(
        String, // key
    ),
    GetBool(
        String, // key
    ),
    SetInt(
        String, // key
        i64,    // value
    ),
    SetChar(
        String, // key
        String, // value
    ),
    SetBool(
        String, // key
        bool,   // value
    ),
}

// A Task to dispatch commands to the delegate.
#[derive(Clone)]
struct PreferenceDelegateTask {
    xpcom: ThreadPtrHandle<nsIPreferenceDelegate>,
    command: PreferenceCommand,
    service_id: TrackerId,
    object_id: TrackerId,
    transport: UdsTransport,
    request_id: u64,
}

impl PreferenceDelegateTask {
    fn reply(&self, payload: GeckoBridgeFromClient) {
        let message = BaseMessage {
            service: self.service_id,
            object: self.object_id,
            kind: BaseMessageKind::Response(self.request_id),
            content: crate::common::get_bincode().serialize(&payload).unwrap(),
        };
        let mut t = self.transport.clone();
        let _ = t.send_message(&message);
    }
}

impl Task for PreferenceDelegateTask {
    fn run(&self) {
        // Call the method on the initial thread.
        if let Some(object) = self.xpcom.get() {
            let payload = match &self.command {
                PreferenceCommand::GetInt(key) => {
                    let key = nsString::from(key);
                    let mut value = 0;
                    let status = unsafe { object.GetInt(&*key as &nsAString, &mut value) };
                    match status {
                        NS_OK => {
                            GeckoBridgeFromClient::PreferenceDelegateGetIntSuccess(value.into())
                        }
                        _ => GeckoBridgeFromClient::PreferenceDelegateGetIntError,
                    }
                }
                PreferenceCommand::GetChar(key) => {
                    let key = nsString::from(key);
                    let mut value = nsString::new();
                    let status = unsafe { object.GetChar(&*key as &nsAString, &mut *value) };
                    match status {
                        NS_OK => GeckoBridgeFromClient::PreferenceDelegateGetCharSuccess(
                            value.to_string(),
                        ),
                        _ => GeckoBridgeFromClient::PreferenceDelegateGetCharError,
                    }
                }
                PreferenceCommand::GetBool(key) => {
                    let key = nsString::from(key);
                    let mut value = false;
                    let status = unsafe { object.GetBool(&*key as &nsAString, &mut value) };
                    match status {
                        NS_OK => GeckoBridgeFromClient::PreferenceDelegateGetBoolSuccess(value),
                        _ => GeckoBridgeFromClient::PreferenceDelegateGetBoolError,
                    }
                }

                PreferenceCommand::SetInt(key, value) => {
                    let key = nsString::from(key);
                    let status = unsafe { object.SetInt(&*key as &nsAString, (*value) as i32) };
                    match status {
                        NS_OK => GeckoBridgeFromClient::PreferenceDelegateSetIntSuccess,
                        _ => GeckoBridgeFromClient::PreferenceDelegateSetIntError,
                    }
                }
                PreferenceCommand::SetChar(key, value) => {
                    let key = nsString::from(key);
                    let value = nsString::from(value);
                    let status =
                        unsafe { object.SetChar(&*key as &nsAString, &*value as &nsAString) };
                    match status {
                        NS_OK => GeckoBridgeFromClient::PreferenceDelegateSetCharSuccess,
                        _ => GeckoBridgeFromClient::PreferenceDelegateSetCharError,
                    }
                }
                PreferenceCommand::SetBool(key, value) => {
                    let key = nsString::from(key);
                    let status = unsafe { object.SetBool(&*key as &nsAString, *value) };
                    match status {
                        NS_OK => GeckoBridgeFromClient::PreferenceDelegateSetBoolSuccess,
                        _ => GeckoBridgeFromClient::PreferenceDelegateSetBoolError,
                    }
                }
            };
            self.reply(payload);
        }
    }

    fn done(&self) -> Result<(), nsresult> {
        // We don't return a result to the calling thread, so nothing to do.
        Ok(())
    }
}
