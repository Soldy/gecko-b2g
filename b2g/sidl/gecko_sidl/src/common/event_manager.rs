/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Helper to manager event listeners.

use crate::common::traits::TrackerId;
use crate::services::core::service::*;
use log::debug;
use moz_task::ThreadPtrHandle;
use parking_lot::Mutex;
use std::collections::HashMap;
use std::sync::Arc;
use xpcom::interfaces::nsISidlEventListener;

pub struct EventManager {
    // Map of event type -> list of (handlers, original address).
    // The original address is stored in a usize as the value of the listener address
    // before it get wrapper by multiple RefPtr<>.
    // TODO: We need a better solution for that!
    pub handlers: HashMap<i32, Vec<(ThreadPtrHandle<nsISidlEventListener>, usize)>>,
    core_service: Arc<Mutex<CoreService>>,
    service_id: TrackerId,
    object_id: TrackerId,
}

impl EventManager {
    pub fn new(
        core_service: Arc<Mutex<CoreService>>,
        service_id: TrackerId,
        object_id: TrackerId,
    ) -> Self {
        Self {
            core_service,
            service_id,
            object_id,
            handlers: HashMap::new(),
        }
    }

    pub fn add_event_listener(
        &mut self,
        event: i32,
        handler: ThreadPtrHandle<nsISidlEventListener>,
        key: usize,
    ) {
        let starts_empty = self.handlers.is_empty();

        match self.handlers.get_mut(&event) {
            None => {
                self.handlers.insert(event, vec![(handler, key)]);
            }
            Some(list) => {
                list.push((handler, key));
            }
        }

        if starts_empty && !self.handlers.is_empty() {
            struct DummyDelegate {}
            impl CoreSimpleReceiverDelegate for DummyDelegate {
                fn on_success(&mut self) {
                    debug!("Success enabling event listeners");
                }
                fn on_error(&mut self) {
                    debug!("Error enabling event listeners");
                }
            }

            let responder = Box::new(CoreAddEventListenerReceiver::new(Box::new(
                DummyDelegate {},
            )));
            let mut core = self.core_service.lock();
            let _ =
                core.enable_event_listeners(self.service_id, self.object_id, event as _, responder);
        }
    }

    pub fn remove_event_listener(&mut self, event: i32, key: usize) {
        if let Some(list) = self.handlers.get_mut(&event) {
            // Remove this handler if we find it in the list of listeners for this event.
            if let Some(pos) = list.iter().position(|item| item.1 == key) {
                list.remove(pos);
            }

            // No listeners for this event, fully remove the entry to know when to disable the service side.
            if list.is_empty() {
                self.handlers.remove(&event);

                // Disable the event.
                struct DummyDelegate {}
                impl CoreSimpleReceiverDelegate for DummyDelegate {
                    fn on_success(&mut self) {
                        debug!("Success disabling event listeners");
                    }
                    fn on_error(&mut self) {
                        debug!("Error disabling event listeners");
                    }
                }

                let responder = Box::new(CoreRemoveEventListenerReceiver::new(Box::new(
                    DummyDelegate {},
                )));
                let mut core = self.core_service.lock();
                let _ = core.disable_event_listeners(
                    self.service_id,
                    self.object_id,
                    event as _,
                    responder,
                );
            }
        }
    }
}

// Helper macros.

// Relay add/remove event listener in concrete implementations using an event_manager field.
macro_rules! impl_event_listener {
    () => {
        fn add_event_listener(
            &mut self,
            event: i32,
            handler: &nsISidlEventListener,
            key: usize,
        ) -> Result<(), nsresult> {
            let handler =
                ThreadPtrHolder::new(cstr!("nsISidlEventListener"), RefPtr::new(handler)).unwrap();

            self.event_manager
                .lock()
                .add_event_listener(event, handler, key);

            Ok(())
        }

        fn remove_event_listener(&mut self, event: i32, key: usize) -> Result<(), nsresult> {
            self.event_manager.lock().remove_event_listener(event, key);

            Ok(())
        }

        fn get_event_listeners_to_reconnect(
            &mut self,
        ) -> Vec<(i32, RefPtr<ThreadPtrHolder<nsISidlEventListener>>, usize)> {
            let mut list = vec![];

            let mut manager = self.event_manager.lock();
            for (event, handlers) in manager.handlers.iter() {
                // handlers is a Vec<(ThreadPtrHandle<nsISidlEventListener>, usize)>
                for (handler, key) in handlers {
                    list.push((*event, handler.clone(), *key));
                }
            }
            // Clear the list of outdated event listeners.
            manager.handlers.clear();

            list
        }
    };
}

// Exposes the xpcom nsISidlEventTarget methods.
macro_rules! xpcom_sidl_event_target {
    () => {
        xpcom_method!(add_event_listener => AddEventListener(event: i32, handler: *const nsISidlEventListener));
        fn add_event_listener(
            &self,
            event: i32,
            handler: &nsISidlEventListener,
        ) -> Result<(), nsresult> {
            let key: usize = unsafe { std::mem::transmute(handler) };

            if let Some(inner) = self.inner.lock().as_ref() {
                return inner.lock().add_event_listener(event, handler, key);
            } else {
                debug!(
                    "Settings add_event_listener {}: Unable to get event target.",
                    event
                );
                self.pending_listeners.lock().push((
                    event,
                    ThreadPtrHolder::new(cstr!("nsISidlEventListener"), RefPtr::new(handler)).unwrap(),
                    key,
                ));
            }
            Ok(())
        }

        xpcom_method!(remove_event_listener => RemoveEventListener(event: i32, handler: *const nsISidlEventListener));
        fn remove_event_listener(
            &self,
            event: i32,
            handler: &nsISidlEventListener,
        ) -> Result<(), nsresult> {
            let key: usize = unsafe { std::mem::transmute(handler) };

            if let Some(inner) = self.inner.lock().as_ref() {
                return inner.lock().remove_event_listener(event, key);
            } else {
                debug!(
                    "Settings remove_event_listener {}: Unable to get event target.",
                    event
                );
                // Remove this listener from the list of pending event listeners.
                let mut listeners = self.pending_listeners.lock();
                if let Some(pos) = listeners.iter().position(|item| item.2 == key) {
                    listeners.remove(pos);
                }
            }
            Ok(())
        }
    }
}
