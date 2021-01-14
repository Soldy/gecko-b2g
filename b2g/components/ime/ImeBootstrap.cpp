/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImeBootstrap.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/IMELog.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/WidgetUtils.h"
#include "PuppetWidget.h"

using namespace mozilla::dom;
using namespace mozilla::widget;

namespace mozilla::ime {

NS_IMPL_ISUPPORTS(ImeBootstrap, nsIObserver)

ImeBootstrap::ImeBootstrap() {
  IME_LOGD("ImeBootstrap::Constructor[%p]", this);
  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  if (obsServ) {
    obsServ->AddObserver(this, "content-document-global-created", false);
    obsServ->AddObserver(this, "chrome-document-global-created", false);
  }
}

ImeBootstrap::~ImeBootstrap() {
  IME_LOGD("ImeBootstrap::Destructor[%p]", this);
  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  if (obsServ) {
    obsServ->RemoveObserver(this, "content-document-global-created");
    obsServ->RemoveObserver(this, "chrome-document-global-created");
  }
}

NS_IMETHODIMP
ImeBootstrap::Observe(nsISupports* aSubject, const char* aTopic,
                      const char16_t* aData) {
  IME_LOGD("ImeBootstrap[%p], Observe[%s]", this, aTopic);

  if (strcmp(aTopic, "content-document-global-created") &&
      strcmp(aTopic, "chrome-document-global-created")) {
    // We will also receive app-startup because of the category registration,
    // but it's fine to ignore it.
    return NS_OK;
  }
  nsCOMPtr<mozIDOMWindowProxy> domWindow = do_QueryInterface(aSubject);
  MOZ_ASSERT(domWindow);
  nsPIDOMWindowOuter* outer = nsPIDOMWindowOuter::From(domWindow);

  nsCOMPtr<nsIWidget> domWidget = WidgetUtils::DOMWindowToWidget(outer);
  NS_ENSURE_TRUE(domWidget, NS_OK);

  if (XRE_IsParentProcess() && outer->GetBrowsingContext()->IsTop()) {
    nsBaseWidget* widget = static_cast<nsBaseWidget*>(domWidget.get());
    RefPtr<TextEventDispatcherListener> listener =
        widget->GetNativeTextEventDispatcherListener();
    if (!listener) {
      RefPtr<GeckoEditableSupport> editableSupport =
          new GeckoEditableSupport(outer);
      widget->SetNativeTextEventDispatcherListener(editableSupport);
      IME_LOGD("GeckoEditableSupport set on widget");
    }
  } else if (XRE_IsContentProcess() &&
             outer->GetBrowsingContext()->IsTopContent()) {
    // Associate the PuppetWidget of the newly-created BrowserChild with a
    // B2GEditableChild instance.
    ImeBootstrap::SetOnBrowserChild(domWidget->GetOwningBrowserChild(), outer);
  }

  return NS_OK;
}

/* static */
void ImeBootstrap::SetOnBrowserChild(dom::BrowserChild* aBrowserChild,
                                     nsPIDOMWindowOuter* aDOMWindow) {
  MOZ_ASSERT(!XRE_IsParentProcess());
  NS_ENSURE_TRUE_VOID(aBrowserChild);
  RefPtr<PuppetWidget> widget(aBrowserChild->WebWidget());
  RefPtr<TextEventDispatcherListener> listener =
      widget->GetNativeTextEventDispatcherListener();

  IME_LOGD("-- ImeBootstrap::SetOnBrowserChild : listener %p widget %p",
           listener.get(), widget.get());
  if (!listener ||
      listener.get() ==
          static_cast<widget::TextEventDispatcherListener*>(widget)) {
    // We need to set a new listener.
    RefPtr<GeckoEditableSupport> editableSupport =
        new GeckoEditableSupport(aDOMWindow);

    IME_LOGD("-- ImeBootstrap::SetOnBrowserChild : listener %p",
             editableSupport.get());
    // Tell PuppetWidget to use our listener for IME operations.
    widget->SetNativeTextEventDispatcherListener(editableSupport);
  }
}

/* static */
already_AddRefed<ImeBootstrap> ImeBootstrap::GetSingleton() {
  if (!StaticPrefs::dom_inputmethod_enabled()) {
    return nullptr;
  }

  static RefPtr<ImeBootstrap> sSingleton;
  if (!sSingleton) {
    sSingleton = new ImeBootstrap();
    ClearOnShutdown(&sSingleton);
  }
  return do_AddRef(sSingleton);
}

}  // namespace mozilla::ime
