/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Wrapper for the nsIAppsServiceDelegate interface.

use super::messages::*;
use crate::common::core::{BaseMessage, BaseMessageKind};
use crate::common::traits::TrackerId;
use crate::common::uds_transport::{from_base_message, SessionObject, XpcomSessionObject};
use bincode::Options;
use log::{debug, error};
use moz_task::{Task, TaskRunnable, ThreadPtrHandle};
use nserror::nsresult;
use nsstring::*;
use std::any::Any;
use xpcom::interfaces::nsIAppsServiceDelegate;

pub struct AppsServiceDelegate {
    xpcom: ThreadPtrHandle<nsIAppsServiceDelegate>,
    service_id: TrackerId,
    object_id: TrackerId,
}

impl AppsServiceDelegate {
    pub fn new(
        xpcom: ThreadPtrHandle<nsIAppsServiceDelegate>,
        service_id: TrackerId,
        object_id: TrackerId,
    ) -> Self {
        Self {
            xpcom,
            service_id,
            object_id,
        }
    }

    fn post_task(&mut self, command: AppsServiceCommand, request_id: u64) -> BaseMessage {
        // Unconditionally Return a success response for now.
        let payload = match command {
            AppsServiceCommand::OnBoot(_, _) => {
                GeckoBridgeFromClient::AppsServiceDelegateOnBootSuccess
            }
            AppsServiceCommand::OnBootDone() => {
                GeckoBridgeFromClient::AppsServiceDelegateOnBootDoneSuccess
            }
            AppsServiceCommand::OnClear(_, _, _) => {
                GeckoBridgeFromClient::AppsServiceDelegateOnClearSuccess
            }
            AppsServiceCommand::OnInstall(_, _) => {
                GeckoBridgeFromClient::AppsServiceDelegateOnInstallSuccess
            }
            AppsServiceCommand::OnUninstall(_) => {
                GeckoBridgeFromClient::AppsServiceDelegateOnUninstallSuccess
            }
            AppsServiceCommand::OnUpdate(_, _) => {
                GeckoBridgeFromClient::AppsServiceDelegateOnUpdateSuccess
            }
        };

        // Dispatch the setting change to the xpcom observer.
        let task = AppsServiceDelegateTask {
            xpcom: self.xpcom.clone(),
            command,
        };

        let _ = TaskRunnable::new("AppsServiceDelegate", Box::new(task))
            .and_then(|r| TaskRunnable::dispatch(r, self.xpcom.owning_thread()));

        // Wrap the payload in a base message and send it.
        let message = BaseMessage {
            service: self.service_id,
            object: self.object_id,
            kind: BaseMessageKind::Response(request_id),
            content: crate::common::get_bincode().serialize(&payload).unwrap(),
        };
        message
    }
}

impl SessionObject for AppsServiceDelegate {
    fn on_request(&mut self, request: BaseMessage, request_id: u64) -> Option<BaseMessage> {
        debug!("AppsServiceDelegate on_request id: {}", request_id);
        // Unpack the request.
        match from_base_message(&request) {
            Ok(GeckoBridgeToClient::AppsServiceDelegateOnBoot(manifest_url, value)) => {
                Some(self.post_task(
                    AppsServiceCommand::OnBoot(manifest_url, value.into()),
                    request_id,
                ))
            }
            Ok(GeckoBridgeToClient::AppsServiceDelegateOnBootDone) => {
                Some(self.post_task(AppsServiceCommand::OnBootDone(), request_id))
            }
            Ok(GeckoBridgeToClient::AppsServiceDelegateOnClear(
                manifest_url,
                clear_type,
                value,
            )) => Some(self.post_task(
                AppsServiceCommand::OnClear(manifest_url, clear_type, value.into()),
                request_id,
            )),
            Ok(GeckoBridgeToClient::AppsServiceDelegateOnInstall(manifest_url, value)) => {
                Some(self.post_task(
                    AppsServiceCommand::OnInstall(manifest_url, value.into()),
                    request_id,
                ))
            }
            Ok(GeckoBridgeToClient::AppsServiceDelegateOnUpdate(manifest_url, value)) => {
                Some(self.post_task(
                    AppsServiceCommand::OnUpdate(manifest_url, value.into()),
                    request_id,
                ))
            }
            Ok(GeckoBridgeToClient::AppsServiceDelegateOnUninstall(manifest_url)) => {
                Some(self.post_task(AppsServiceCommand::OnUninstall(manifest_url), request_id))
            }
            _ => {
                error!(
                    "AppsServiceDelegate::on_request unexpected message: {:?}",
                    request.content
                );
                None
            }
        }
    }

    fn on_event(&mut self, _event: Vec<u8>) {
        debug!("AppsServiceDelegate on_request event");
    }

    fn get_ids(&self) -> (u32, u32) {
        debug!(
            "AppsServiceDelegate get_ids, {}, {}",
            self.service_id, self.object_id
        );
        (self.service_id, self.object_id)
    }
}

impl XpcomSessionObject for AppsServiceDelegate {
    fn as_xpcom(&self) -> &dyn Any {
        &self.xpcom
    }
}

// Commands supported by the power manager delegate.
#[derive(Clone)]
enum AppsServiceCommand {
    OnBoot(
        String, // manifest_url
        String, // b2g_features
    ),
    OnBootDone(),
    OnClear(
        String, // manifest_url
        String, // clear_type
        String, // b2g_features
    ),
    OnInstall(
        String, // manifest_url
        String, // b2g_features
    ),
    OnUpdate(
        String, // manifest_url
        String, // b2g_features
    ),
    OnUninstall(
        String, // manifest_url
    ),
}

// A Task to dispatch commands to the delegate.
#[derive(Clone)]
struct AppsServiceDelegateTask {
    xpcom: ThreadPtrHandle<nsIAppsServiceDelegate>,
    command: AppsServiceCommand,
}

impl Task for AppsServiceDelegateTask {
    fn run(&self) {
        // Call the method on the initial thread.
        debug!("AppsServiceDelegateTask::run");
        if let Some(object) = self.xpcom.get() {
            match &self.command {
                AppsServiceCommand::OnBoot(manifest_url, value) => {
                    let manifest_url = nsString::from(manifest_url);
                    let value = nsString::from(value);
                    debug!(
                        "AppsServiceDelegateTask OnBoot manifest_url: {}",
                        manifest_url
                    );
                    debug!("AppsServiceDelegateTask OnBoot value: {:?}", value);
                    unsafe {
                        object.OnBoot(&*manifest_url as &nsAString, &*value as &nsAString);
                    }
                }
                AppsServiceCommand::OnBootDone() => {
                    debug!("AppsServiceDelegateTask OnBootDone");
                    unsafe {
                        object.OnBootDone();
                    }
                }
                AppsServiceCommand::OnClear(manifest_url, clear_type, value) => {
                    let manifest_url = nsString::from(manifest_url);
                    let clear_type = nsString::from(clear_type);
                    let value = nsString::from(value);
                    unsafe {
                        object.OnClear(
                            &*manifest_url as &nsAString,
                            &*clear_type as &nsAString,
                            &*value as &nsAString,
                        );
                    }
                }
                AppsServiceCommand::OnInstall(manifest_url, value) => {
                    let manifest_url = nsString::from(manifest_url);
                    let value = nsString::from(value);
                    unsafe {
                        object.OnInstall(&*manifest_url as &nsAString, &*value as &nsAString);
                    }
                }
                AppsServiceCommand::OnUpdate(manifest_url, value) => {
                    let manifest_url = nsString::from(manifest_url);
                    let value = nsString::from(value);
                    unsafe {
                        object.OnUpdate(&*manifest_url as &nsAString, &*value as &nsAString);
                    }
                }
                AppsServiceCommand::OnUninstall(manifest_url) => {
                    let url = nsString::from(manifest_url);
                    unsafe {
                        object.OnUninstall(&*url as &nsAString);
                    }
                }
            }
        }
    }

    fn done(&self) -> Result<(), nsresult> {
        debug!("AppsServiceDelegateTask::Done");
        // We don't return a result to the calling thread, so nothing to do.
        Ok(())
    }
}
