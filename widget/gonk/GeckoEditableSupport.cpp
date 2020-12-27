/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoEditableSupport.h"

#include "PuppetWidget.h"
#include "nsIContent.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_intl.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEventDispatcherListener.h"
#include "mozilla/TextEvents.h"
#include "mozilla/ToString.h"
#include "mozilla/dom/BrowserChild.h"
#include "nsIDocShell.h"
#include "mozilla/dom/IMELog.h"
#include "mozilla/dom/InputMethodService.h"
#include "mozilla/dom/InputMethodServiceChild.h"
#include "nsFocusManager.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "mozilla/dom/HTMLOptGroupElement.h"
#include "mozilla/dom/HTMLOptionElement.h"
#include "mozilla/dom/Document.h"
#include "nsGenericHTMLElement.h"
#include "mozilla/dom/Element.h"
#include "nsIDocumentEncoder.h"
#include "nsRange.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/nsInputContext.h"
#include "mozilla/dom/EditableUtils.h"
#include "nsIEditor.h"  // for nsIEditor, etc.
#include "nsIEditingSession.h"

using namespace mozilla::dom;

namespace mozilla {

namespace widget {

bool isDateTimeTypes(nsAString& inputType) {
  return inputType.EqualsIgnoreCase("datetime") ||
         inputType.EqualsIgnoreCase("datetime-local") ||
         inputType.EqualsIgnoreCase("time") ||
         inputType.EqualsIgnoreCase("date");
}

bool isPlainTextField(Element* aElement) {
  if (!aElement) {
    return false;
  }
  RefPtr<HTMLTextAreaElement> textArea =
      HTMLTextAreaElement::FromNodeOrNull(aElement);
  if (textArea) {
    return true;
  }
  RefPtr<HTMLInputElement> input = HTMLInputElement::FromNodeOrNull(aElement);
  if (input) {
    return input->MozIsTextField(false);
  }
  return false;
}

already_AddRefed<nsIDocumentEncoder> getDocumentEncoder(Element* aElement) {
  nsCOMPtr<Document> document = aElement->OwnerDoc();
  nsAutoString contentType;
  document->GetContentType(contentType);
  nsCOMPtr<nsIDocumentEncoder> docEncoder = do_createDocumentEncoder(
      PromiseFlatCString(NS_ConvertUTF16toUTF8(contentType)).get());
  uint32_t flags = nsIDocumentEncoder::SkipInvisibleContent |
                   nsIDocumentEncoder::OutputRaw |
                   nsIDocumentEncoder::OutputDropInvisibleBreak |
                   nsIDocumentEncoder::OutputLFLineBreak;
  docEncoder->Init(document, u"text/plain"_ns, flags);
  return docEncoder.forget();
}

nsresult getContentEditableText(Element* aElement, nsAString& aText) {
  if (!aElement || !EditableUtils::isContentEditable(aElement)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<Document> document = aElement->OwnerDoc();
  ErrorResult rv;
  RefPtr<nsRange> range = document->CreateRange(rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }
  range->SelectNodeContents(*aElement, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }
  nsCOMPtr<nsIDocumentEncoder> docEncoder = getDocumentEncoder(aElement);
  if (!docEncoder) {
    return NS_ERROR_FAILURE;
  }
  docEncoder->SetRange(range);
  return docEncoder->EncodeToString(aText);
}

uint32_t getSelectionStart(Element* aElement) {
  uint32_t start = 0;
  nsAutoString attributeValue;
  if (isPlainTextField(aElement)) {
    ErrorResult rv;
    Nullable<uint32_t> _start;
    RefPtr<HTMLTextAreaElement> textArea =
        HTMLTextAreaElement::FromNodeOrNull(aElement);
    if (textArea) {
      _start = textArea->GetSelectionStart(rv);
    }
    RefPtr<HTMLInputElement> input = HTMLInputElement::FromNodeOrNull(aElement);
    if (input) {
      _start = input->GetSelectionStartIgnoringType(rv);
    }
    if (!_start.IsNull()) {
      start = _start.Value();
    }
  } else if (EditableUtils::isContentEditable(aElement)) {
    nsCOMPtr<Document> document = aElement->OwnerDoc();
    nsCOMPtr<nsPIDOMWindowOuter> window = document->GetWindow();
    RefPtr<Selection> selection = window->GetSelection();
    if (selection && selection->RangeCount() > 0) {
      ErrorResult rv;
      RefPtr<nsRange> range = document->CreateRange(rv);
      if (NS_WARN_IF(rv.Failed())) {
        return 0;
      }
      range->SetStart(aElement, 0);
      range->SetEnd(selection->GetAnchorNode(), selection->AnchorOffset());
      nsCOMPtr<nsIDocumentEncoder> docEncoder = getDocumentEncoder(aElement);
      if (!docEncoder) {
        return 0;
      }
      docEncoder->SetRange(range);
      nsAutoString buffer;
      docEncoder->EncodeToString(buffer);
      start = buffer.Length();
    }
  }
  return start;
}

uint32_t getSelectionEnd(Element* aElement) {
  uint32_t end = 0;
  nsAutoString attributeValue;
  if (isPlainTextField(aElement)) {
    ErrorResult rv;
    Nullable<uint32_t> _end;
    RefPtr<HTMLTextAreaElement> textArea =
        HTMLTextAreaElement::FromNodeOrNull(aElement);
    if (textArea) {
      _end = textArea->GetSelectionEnd(rv);
    }
    RefPtr<HTMLInputElement> input = HTMLInputElement::FromNodeOrNull(aElement);
    if (input) {
      _end = input->GetSelectionEndIgnoringType(rv);
    }
    if (!_end.IsNull()) {
      end = _end.Value();
    }
  } else if (EditableUtils::isContentEditable(aElement)) {
    nsCOMPtr<Document> document = aElement->OwnerDoc();
    nsCOMPtr<nsPIDOMWindowOuter> window = document->GetWindow();
    RefPtr<Selection> selection = window->GetSelection();
    if (selection && selection->RangeCount() > 0) {
      uint32_t start = getSelectionStart(aElement);
      nsCOMPtr<nsIDocumentEncoder> docEncoder = getDocumentEncoder(aElement);
      if (!docEncoder) {
        return 0;
      }
      ErrorResult rv;
      docEncoder->SetRange(selection->GetRangeAt(0, rv));
      nsAutoString buffer;
      docEncoder->EncodeToString(buffer);
      uint32_t length = buffer.Length();
      end = start + length;
    }
  }
  return end;
}

nsIEditor* getEditor(Element* aElement) {
  nsCOMPtr<nsIEditor> editor = nullptr;
  if (isPlainTextField(aElement)) {
    RefPtr<TextControlElement> textControlElement =
        TextControlElement::FromNodeOrNull(aElement);
    if (!textControlElement) {
      return nullptr;
    }
    editor = textControlElement->GetTextEditor();
  } else if (EditableUtils::isContentEditable(aElement)) {
    nsCOMPtr<Document> document = aElement->OwnerDoc();
    nsCOMPtr<nsPIDOMWindowOuter> window = document->GetWindow();
    nsCOMPtr<nsIDocShell> docShell = window->GetDocShell();
    nsCOMPtr<nsIEditingSession> editSession;
    nsresult rv = docShell->GetEditingSession(getter_AddRefs(editSession));
    if (NS_FAILED(rv)) {
      return nullptr;
    }
    editSession->GetEditorForWindow(window, getter_AddRefs(editor));
  }

  return editor;
}

TextEventDispatcher* getTextEventDispatcherFromFocus() {
  RefPtr<nsFocusManager> focusManager = nsFocusManager::GetFocusManager();
  if (!focusManager) {
    return nullptr;
  }
  RefPtr<Element> focusedElement = focusManager->GetFocusedElement();
  if (!focusedElement) {
    return nullptr;
  }
  nsCOMPtr<Document> doc = focusedElement->GetComposedDoc();
  if (!doc) {
    return nullptr;
  }
  PresShell* presShell = doc->GetPresShell();
  if (!presShell) {
    return nullptr;
  }
  nsPresContext* presContext = presShell->GetPresContext();
  if (!presContext) {
    return nullptr;
  }
  RefPtr<nsIWidget> widget = presContext->GetTextInputHandlingWidget();
  if (!widget) {
    return nullptr;
  }
  return widget->GetTextEventDispatcher();
}

HandleFocusRequest CreateFocusRequestFromInputContext(
    nsIInputContext* aInputContext) {
  nsAutoString typeValue, inputTypeValue, valueValue, maxValue, minValue,
      langValue, inputModeValue, nameValue;
  uint32_t selectionStartValue, selectionEndValue;
  bool voiceInputSupportedValue;

  aInputContext->GetType(typeValue);
  aInputContext->GetInputType(inputTypeValue);
  aInputContext->GetValue(valueValue);
  aInputContext->GetMax(maxValue);
  aInputContext->GetMin(minValue);
  aInputContext->GetLang(langValue);
  aInputContext->GetInputMode(inputModeValue);
  aInputContext->GetVoiceInputSupported(&voiceInputSupportedValue);
  aInputContext->GetName(nameValue);
  aInputContext->GetSelectionStart(&selectionStartValue);
  aInputContext->GetSelectionEnd(&selectionEndValue);
  nsCOMPtr<nsIInputContextChoices> choices;
  aInputContext->GetChoices(getter_AddRefs(choices));
  SelectionChoices selectionChoices;
  nsTArray<OptionDetailCollection> optionDetailArray;
  if (choices) {
    bool isMultiple;
    choices->GetMultiple(&isMultiple);
    nsTArray<RefPtr<nsIInputContextChoicesInfo>> infos;
    choices->GetChoices(infos);

    for (const auto& info : infos) {
      bool isGroup;
      info->GetGroup(&isGroup);
      if (isGroup) {  // OptionGroupDetail
        OptionGroupDetail groupDetail;
        bool isGroupDisabled;
        info->GetDisabled(&isGroupDisabled);
        nsAutoString label;
        info->GetLabel(label);
        groupDetail.group() = isGroup;
        groupDetail.label() = nsString(label);
        groupDetail.disabled() = isGroupDisabled;
        IME_LOGD("groupDetail.group:[%s]", isGroup ? "true" : "false");
        IME_LOGD("groupDetail.disabled:[%s]",
                 isGroupDisabled ? "true" : "false");
        IME_LOGD("groupDetail.label:[%s]", NS_ConvertUTF16toUTF8(label).get());
        optionDetailArray.AppendElement(groupDetail);
      } else {  // OptionDetail
        OptionDetail optionDetail;
        bool isInGroup, isDisabled, isSelected, isDefaultSelected;
        nsAutoString text;
        int32_t optionIndex;
        info->GetInGroup(&isInGroup);
        info->GetDisabled(&isDisabled);
        info->GetSelected(&isSelected);
        info->GetDefaultSelected(&isDefaultSelected);
        info->GetText(text);
        info->GetOptionIndex(&optionIndex);
        optionDetail.group() = isGroup;
        optionDetail.inGroup() = isInGroup;
        optionDetail.disabled() = isDisabled;
        optionDetail.selected() = isSelected;
        optionDetail.defaultSelected() = isDefaultSelected;
        optionDetail.text() = nsString(text);
        optionDetail.optionIndex() = optionIndex;
        optionDetailArray.AppendElement(optionDetail);
        IME_LOGD("optionDetail.group:[%s]", isGroup ? "true" : "false");
        IME_LOGD("optionDetail.inGroup:[%s]", isInGroup ? "true" : "false");
        IME_LOGD("optionDetail.disabled:[%s]", isDisabled ? "true" : "false");
        IME_LOGD("optionDetail.selected:[%s]", isSelected ? "true" : "false");
        IME_LOGD("optionDetail.defaultSelected:[%s]",
                 isDefaultSelected ? "true" : "false");
        IME_LOGD("optionDetail.text:[%s]", NS_ConvertUTF16toUTF8(text).get());
        IME_LOGD("optionDetail.optionIndex:[%ld]", optionIndex);
      }
    }
    selectionChoices.multiple() = isMultiple;
    selectionChoices.choices() = optionDetailArray.Clone();
  }

  HandleFocusRequest request(
      nsString(typeValue), nsString(inputTypeValue), nsString(valueValue),
      nsString(maxValue), nsString(minValue), nsString(langValue),
      nsString(inputModeValue), voiceInputSupportedValue, nsString(nameValue),
      selectionStartValue, selectionEndValue, selectionChoices);
  return request;
}

NS_IMPL_ISUPPORTS(GeckoEditableSupport, TextEventDispatcherListener,
                  nsIEditableSupport, nsIDOMEventListener, nsIObserver,
                  nsISupportsWeakReference)

GeckoEditableSupport::GeckoEditableSupport(nsPIDOMWindowOuter* aDOMWindow)
    : mIsFocused(false) {
  IME_LOGD("GeckoEditableSupport::Constructor[%p]", this);
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "outer-window-destroyed", false);
  }
  nsCOMPtr<EventTarget> mChromeEventHandler =
      aDOMWindow->GetChromeEventHandler();
  if (mChromeEventHandler) {
    IME_LOGD("IME: AddEventListener: focus & blur");
    mChromeEventHandler->AddEventListener(u"focus"_ns, this,
                                          /* useCapture = */ true);
    mChromeEventHandler->AddEventListener(u"blur"_ns, this,
                                          /* useCapture = */ true);
  }
}

GeckoEditableSupport::~GeckoEditableSupport() {
  IME_LOGD("GeckoEditableSupport::Destructor[%p]", this);
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, "outer-window-destroyed");
  }
}

NS_IMETHODIMP
GeckoEditableSupport::Observe(nsISupports* aSubject, const char* aTopic,
                              const char16_t* aData) {
  IME_LOGD("GeckoEditableSupport[%p], Observe[%s]", this, aTopic);
  if (!strcmp(aTopic, "outer-window-destroyed")) {
    if (!mChromeEventHandler) {
      return NS_ERROR_FAILURE;
    }
    mChromeEventHandler->RemoveEventListener(u"focus"_ns, this,
                                             /* useCapture = */ true);
    mChromeEventHandler->RemoveEventListener(u"blur"_ns, this,
                                             /* useCapture = */ true);
  }
  return NS_OK;
}

/* static */
void GeckoEditableSupport::SetOnBrowserChild(dom::BrowserChild* aBrowserChild,
                                             nsPIDOMWindowOuter* aDOMWindow) {
  MOZ_ASSERT(!XRE_IsParentProcess());
  NS_ENSURE_TRUE_VOID(aBrowserChild);
  RefPtr<PuppetWidget> widget(aBrowserChild->WebWidget());
  RefPtr<TextEventDispatcherListener> listener =
      widget->GetNativeTextEventDispatcherListener();

  IME_LOGD("-- GeckoEditableSupport::SetOnBrowserChild : listener %p widget %p",
           listener.get(), widget.get());
  if (!listener ||
      listener.get() == static_cast<TextEventDispatcherListener*>(widget)) {
    // We need to set a new listener.
    RefPtr<GeckoEditableSupport> editableSupport =
        new GeckoEditableSupport(aDOMWindow);

    IME_LOGD("-- GeckoEditableSupport::SetOnBrowserChild : listener %p",
             editableSupport.get());
    // Tell PuppetWidget to use our listener for IME operations.
    widget->SetNativeTextEventDispatcherListener(editableSupport);
  }
}

NS_IMETHODIMP
GeckoEditableSupport::HandleEvent(Event* aEvent) {
  if (!aEvent) {
    return NS_OK;
  }
  nsCOMPtr<EventTarget> target = aEvent->GetComposedTarget();
  nsCOMPtr<nsINode> node(do_QueryInterface(target));
  if (!node || !node->IsHTMLElement()) {
    return NS_OK;
  }
  nsCOMPtr<Element> ele = node->AsElement();
  if (!ele) {
    return NS_OK;
  }

  bool isTargetEditable = EditableUtils::isContentEditable(ele) ||
                          EditableUtils::isFocusableElement(ele);

  nsAutoString eventType;
  aEvent->GetType(eventType);

  InternalFocusEvent* focusEvent = aEvent->WidgetEventPtr()->AsFocusEvent();
  if (!focusEvent) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> relatedTarget =
      do_QueryInterface(focusEvent->mRelatedTarget);
  nsIContent* nonChromeTarget =
      relatedTarget ? relatedTarget->FindFirstNonChromeOnlyAccessContent()
                    : relatedTarget.get();

  IME_LOGD(
      "GeckoEditableSupport::HandleEvent %s target %p editable %d related %p",
      NS_ConvertUTF16toUTF8(eventType).get(), node.get(), isTargetEditable,
      nonChromeTarget);

  if (node == nonChromeTarget) {
    return NS_OK;
  }

  if (eventType.EqualsLiteral("focus")) {
    if (!isTargetEditable) {
      // Focusing on other non-editable element and the blur event is missing.
      if (mIsFocused) {
        HandleBlur();
      }
      return NS_OK;
    }

    HandleFocus();
    RefPtr<TextEventDispatcher> dispatcher = getTextEventDispatcherFromFocus();
    if (dispatcher) {
      nsresult result = dispatcher->BeginInputTransaction(this);
      if (NS_WARN_IF(NS_FAILED(result))) {
        IME_LOGD("Fails to BeginInputTransaction");
      }
    }
  } else if (eventType.EqualsLiteral("blur")) {
    if (!isTargetEditable) {
      return NS_OK;
    }
    HandleBlur();
    RefPtr<TextEventDispatcher> dispatcher = getTextEventDispatcherFromFocus();
    if (dispatcher) {
      dispatcher->EndInputTransaction(this);
      OnRemovedFrom(dispatcher);
    }
  }
  return NS_OK;
}

void GeckoEditableSupport::HandleFocus() {
  RefPtr<nsInputContext> inputContext = new nsInputContext();
  nsresult rv = GetInputContextBag(inputContext);
  if (NS_FAILED(rv)) {
    return;
  }

  MOZ_ASSERT(!mIsFocused);
  mIsFocused = true;

  // oop
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (contentChild) {
    IME_LOGD("-- GeckoEditableSupport::HandleFocus on content");
    InputMethodServiceChild* child = new InputMethodServiceChild();
    child->SetEditableSupport(this);
    contentChild->SendPInputMethodServiceConstructor(child);
    child->SendRequest(CreateFocusRequestFromInputContext(inputContext));
    // TODO Unable to delete here, find somewhere else to do so.
  } else {
    IME_LOGD("-- GeckoEditableSupport::HandleFocus on chrome");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = dom::InputMethodService::GetInstance();
    service->HandleFocus(this, static_cast<nsIInputContext*>(inputContext));
  }
}

void GeckoEditableSupport::HandleBlur() {
  MOZ_ASSERT(mIsFocused);
  mIsFocused = false;

  // oop
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (contentChild) {
    InputMethodServiceChild* child = new InputMethodServiceChild();
    child->SetEditableSupport(this);
    contentChild->SendPInputMethodServiceConstructor(child);
    HandleBlurRequest request(contentChild->GetID());
    child->SendRequest(request);
    Unused << child->Send__delete__(child);
  } else {
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = dom::InputMethodService::GetInstance();
    service->HandleBlur(this);
  }
}

// nsIEditableSupport methods.
NS_IMETHODIMP
GeckoEditableSupport::SetComposition(uint32_t aId,
                                     nsIEditableSupportListener* aListener,
                                     const nsAString& aText) {
  RefPtr<TextEventDispatcher> dispatcher = getTextEventDispatcherFromFocus();
  if (!dispatcher) {
    IME_LOGD("Invalid dispatcher");
    return NS_ERROR_ABORT;
  }
  IME_LOGD("-- GeckoEditableSupport::SetComposition");
  IME_LOGD("-- EditableSupport aText:[%s]", NS_ConvertUTF16toUTF8(aText).get());
  IME_LOGD("-- EditableSupport dispatcher:[%p]", dispatcher.get());
  // Handle value
  nsresult rv = dispatcher->GetState();
  do {
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }
    if (aText.Length() == 0) {
      dispatcher->SetPendingCompositionString(EmptyString());
    } else {
      dispatcher->SetPendingCompositionString(aText);
    }

    nsEventStatus status = nsEventStatus_eIgnore;
    rv = dispatcher->FlushPendingComposition(status);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }
  } while (0);

  if (aListener) {
    aListener->OnSetComposition(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::EndComposition(uint32_t aId,
                                     nsIEditableSupportListener* aListener,
                                     const nsAString& aText) {
  RefPtr<TextEventDispatcher> dispatcher = getTextEventDispatcherFromFocus();
  if (!dispatcher) {
    IME_LOGD("Invalid dispatcher");
    return NS_ERROR_ABORT;
  }
  IME_LOGD("-- GeckoEditableSupport::EndComposition");
  IME_LOGD("-- EditableSupport aText:[%s]", NS_ConvertUTF16toUTF8(aText).get());
  IME_LOGD("-- EditableSupport dispatcher:[%p]", dispatcher.get());
  // Handle value
  nsresult rv = dispatcher->GetState();
  do {
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }

    if (!dispatcher->IsComposing()) {
      rv = NS_ERROR_FAILURE;
      break;
    }

    nsEventStatus status = nsEventStatus_eIgnore;
    rv = dispatcher->CommitComposition(status, &aText);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }
  } while (0);

  if (aListener) {
    aListener->OnEndComposition(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::Keydown(uint32_t aId,
                              nsIEditableSupportListener* aListener,
                              const nsAString& aKey) {
  RefPtr<TextEventDispatcher> dispatcher = getTextEventDispatcherFromFocus();
  if (!dispatcher) {
    IME_LOGD("Invalid dispatcher");
    return NS_ERROR_ABORT;
  }
  IME_LOGD("-- GeckoEditableSupport::Keydown");
  IME_LOGD("-- EditableSupport aKey:[%s]", NS_ConvertUTF16toUTF8(aKey).get());
  IME_LOGD("-- EditableSupport dispatcher:[%p]", dispatcher.get());
  // Handle value
  nsresult rv = dispatcher->GetState();
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIWidget> widget = dispatcher->GetWidget();
    WidgetKeyboardEvent event(true, eKeyDown, widget);
    event.mHandledByIME = true;
    event.mKeyNameIndex = WidgetKeyboardEvent::GetKeyNameIndex(aKey);
    if (event.mKeyNameIndex == KEY_NAME_INDEX_USE_STRING) {
      event.mKeyValue = aKey;
    } else if (event.mCodeNameIndex == CODE_NAME_INDEX_UNKNOWN) {
      event.mKeyCode = WidgetKeyboardEvent::ComputeKeyCodeFromKeyNameIndex(
          event.mKeyNameIndex);
      IME_LOGD("-- EditableSupport OnKeydown: mKeyCode:[%d]", event.mKeyCode);
    }
    nsEventStatus status = nsEventStatus_eIgnore;
    if (dispatcher->DispatchKeyboardEvent(eKeyDown, event, status)) {
      dispatcher->MaybeDispatchKeypressEvents(event, status);
    }
  }

  if (aListener) {
    aListener->OnKeydown(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::Keyup(uint32_t aId, nsIEditableSupportListener* aListener,
                            const nsAString& aKey) {
  RefPtr<TextEventDispatcher> dispatcher = getTextEventDispatcherFromFocus();
  if (!dispatcher) {
    IME_LOGD("Invalid dispatcher");
    return NS_ERROR_ABORT;
  }
  IME_LOGD("-- GeckoEditableSupport::Keyup");
  IME_LOGD("-- EditableSupport aKey:[%s]", NS_ConvertUTF16toUTF8(aKey).get());
  IME_LOGD("-- EditableSupport dispatcher:[%p]", dispatcher.get());
  // Handle value
  nsresult rv = dispatcher->GetState();
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIWidget> widget = dispatcher->GetWidget();
    WidgetKeyboardEvent event(true, eKeyUp, widget);
    event.mHandledByIME = true;
    event.mKeyNameIndex = WidgetKeyboardEvent::GetKeyNameIndex(aKey);
    if (event.mKeyNameIndex == KEY_NAME_INDEX_USE_STRING) {
      event.mKeyValue = aKey;
    } else if (event.mCodeNameIndex == CODE_NAME_INDEX_UNKNOWN) {
      event.mKeyCode = WidgetKeyboardEvent::ComputeKeyCodeFromKeyNameIndex(
          event.mKeyNameIndex);
      IME_LOGD("-- EditableSupport OnKeyup: mKeyCode:[%d]", event.mKeyCode);
    }
    nsEventStatus status = nsEventStatus_eIgnore;
    dispatcher->DispatchKeyboardEvent(eKeyUp, event, status);
  }

  if (aListener) {
    aListener->OnKeyup(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::SendKey(uint32_t aId,
                              nsIEditableSupportListener* aListener,
                              const nsAString& aKey) {
  RefPtr<TextEventDispatcher> dispatcher = getTextEventDispatcherFromFocus();
  if (!dispatcher) {
    IME_LOGD("Invalid dispatcher");
    return NS_ERROR_ABORT;
  }
  IME_LOGD("-- EditableSupport SendKey");
  IME_LOGD("-- EditableSupport aKey:[%s]", NS_ConvertUTF16toUTF8(aKey).get());
  IME_LOGD("-- EditableSupport dispatcher:[%p]", dispatcher.get());
  RefPtr<TextEventDispatcher> kungFuDeathGrip(dispatcher);
  nsresult rv = kungFuDeathGrip->GetState();
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIWidget> widget = kungFuDeathGrip->GetWidget();
    WidgetKeyboardEvent event(true, eVoidEvent, widget);
    event.mHandledByIME = true;
    event.mKeyNameIndex = WidgetKeyboardEvent::GetKeyNameIndex(aKey);
    if (event.mKeyNameIndex == KEY_NAME_INDEX_USE_STRING) {
      event.mKeyValue = aKey;
    } else if (event.mCodeNameIndex == CODE_NAME_INDEX_UNKNOWN) {
      event.mKeyCode = WidgetKeyboardEvent::ComputeKeyCodeFromKeyNameIndex(
          event.mKeyNameIndex);
      IME_LOGD("-- EditableSupport SendKey: mKeyCode:[%d]", event.mKeyCode);
    }
    nsEventStatus status = nsEventStatus_eIgnore;
    if (kungFuDeathGrip->DispatchKeyboardEvent(eKeyDown, event, status)) {
      kungFuDeathGrip->MaybeDispatchKeypressEvents(event, status);
    }
    kungFuDeathGrip->DispatchKeyboardEvent(eKeyUp, event, status);
  }

  if (aListener) {
    aListener->OnSendKey(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::DeleteBackward(uint32_t aId,
                                     nsIEditableSupportListener* aListener) {
  IME_LOGD("-- GeckoEditableSupport::DeleteBackward");

  nsresult rv = NS_ERROR_ABORT;
  do {
    nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
    if (!focusManager) {
      break;
    }

    Element* focusedElement = focusManager->GetFocusedElement();
    if (!focusedElement) {
      break;
    }

    nsCOMPtr<nsIEditor> editor = getEditor(focusedElement);
    if (!editor) {
      break;
    }

    rv = editor->DeleteSelection(nsIEditor::ePrevious, nsIEditor::eStrip);
  } while (0);

  if (aListener) {
    aListener->OnDeleteBackward(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::SetSelectedOption(uint32_t aId,
                                        nsIEditableSupportListener* aListener,
                                        int32_t aOptionIndex) {
  IME_LOGD("-- EditableSupport SetSelectedOption");
  IME_LOGD("-- EditableSupport aOptionIndex:[%ld]", aOptionIndex);
  nsresult rv = NS_ERROR_ABORT;
  do {
    nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
    if (!focusManager) {
      break;
    }

    Element* focusedElement = focusManager->GetFocusedElement();
    if (!focusedElement) {
      break;
    }

    if (aOptionIndex < 0) {
      break;
    }

    RefPtr<HTMLSelectElement> selectElement =
        HTMLSelectElement::FromNodeOrNull(focusedElement);
    if (!selectElement) {
      break;
    }
    // Safe to convert to unit since index is not negative
    if ((uint32_t)aOptionIndex > selectElement->Length()) {
      break;
    }

    rv = NS_OK;
    if (selectElement->SelectedIndex() == aOptionIndex) {
      break;
    }

    selectElement->SetSelectedIndex(aOptionIndex);

    nsCOMPtr<Document> document = selectElement->OwnerDoc();
    RefPtr<Event> event = NS_NewDOMEvent(document, nullptr, nullptr);
    event->InitEvent(u"change"_ns, true, true);
    event->SetTrusted(true);

    ErrorResult result;
    selectElement->DispatchEvent(*event, result);
    if (result.Failed()) {
      IME_LOGD("Failed to dispatch change event!!!");
      rv = NS_ERROR_ABORT;
    }
  } while (0);

  if (aListener) {
    aListener->OnSetSelectedOption(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::SetSelectedOptions(
    uint32_t aId, nsIEditableSupportListener* aListener,
    const nsTArray<int32_t>& aOptionIndexes) {
  IME_LOGD("-- EditableSupport SetSelectedOption");
  IME_LOGD("-- EditableSupport aOptionIndexes.Length():[%ld]",
           aOptionIndexes.Length());

  nsresult rv = NS_ERROR_ABORT;
  do {
    nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
    if (!focusManager) {
      break;
    }

    Element* focusedElement = focusManager->GetFocusedElement();
    if (!focusedElement) {
      break;
    }

    RefPtr<HTMLSelectElement> selectElement =
        HTMLSelectElement::FromNodeOrNull(focusedElement);
    if (!selectElement) {
      break;
    }

    // only fire onchange event if any selected option is changed
    bool changed = false;
    RefPtr<dom::HTMLOptionsCollection> options = selectElement->GetOptions();
    uint32_t numOptions = options->Length();
    for (int32_t idx = 0; idx < int(numOptions); idx++) {
      bool newValue = (aOptionIndexes.IndexOf(idx) != aOptionIndexes.NoIndex);
      bool oldValue = options->ItemAsOption(idx)->Selected();
      IME_LOGD("-- SetSelectedOptions options[%ld] old:[%d] -> new:[%d]", idx,
               oldValue, newValue);
      if (oldValue != newValue) {
        options->ItemAsOption(idx)->SetSelected(newValue);
        changed = true;
      }
    }
    rv = NS_OK;
    if (!changed) {
      break;
    }

    nsCOMPtr<Document> document = selectElement->OwnerDoc();
    RefPtr<Event> event = NS_NewDOMEvent(document, nullptr, nullptr);
    event->InitEvent(u"change"_ns, true, true);
    event->SetTrusted(true);

    ErrorResult result;
    selectElement->DispatchEvent(*event, result);
    if (result.Failed()) {
      IME_LOGD("Failed to dispatch change event!!!");
      rv = NS_ERROR_ABORT;
      break;
    }
  } while (0);

  if (aListener) {
    aListener->OnSetSelectedOptions(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::RemoveFocus(uint32_t aId,
                                  nsIEditableSupportListener* aListener) {
  IME_LOGD("-- GeckoEditableSupport::RemoveFocus");
  nsresult rv = NS_ERROR_ABORT;
  do {
    nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
    if (!focusManager) {
      break;
    }

    Element* focusedElement = focusManager->GetFocusedElement();
    if (!focusedElement) {
      break;
    }

    ErrorResult result;
    focusedElement->Blur(result);
    rv = NS_OK;
    if (NS_WARN_IF(result.Failed())) {
      rv = result.StealNSResult();
    }
  } while (0);

  if (aListener) {
    aListener->OnRemoveFocus(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::GetSelectionRange(uint32_t aId,
                                        nsIEditableSupportListener* aListener) {
  IME_LOGD("-- GeckoEditableSupport GetSelectionRange");

  nsresult rv = NS_ERROR_ABORT;
  int32_t start = 0, end = 0;
  do {
    nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
    if (!focusManager) {
      break;
    }

    Element* focusedElement = focusManager->GetFocusedElement();
    if (!focusedElement) {
      break;
    }

    rv = NS_OK;
    start = (int32_t)getSelectionStart(focusedElement);
    end = (int32_t)getSelectionEnd(focusedElement);
  } while (0);

  IME_LOGD("GeckoEditableSupport: GetSelectionRange:[rv:%d start:%lu end:%lu]",
           rv, start, end);
  if (aListener) {
    aListener->OnGetSelectionRange(aId, rv, start, end);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::GetText(uint32_t aId,
                              nsIEditableSupportListener* aListener,
                              int32_t aOffset, int32_t aLength) {
  IME_LOGD("-- GeckoEditableSupport GetText");

  nsresult rv = NS_ERROR_ABORT;
  nsAutoString text;
  do {
    nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
    if (!focusManager) {
      break;
    }

    Element* focusedElement = focusManager->GetFocusedElement();
    if (!focusedElement) {
      break;
    }
    if (isPlainTextField(focusedElement)) {
      RefPtr<HTMLTextAreaElement> textArea =
          HTMLTextAreaElement::FromNodeOrNull(focusedElement);
      RefPtr<HTMLInputElement> input =
          HTMLInputElement::FromNodeOrNull(focusedElement);
      if (textArea) {
        textArea->GetValue(text);
        rv = NS_OK;
      } else if (input) {
        input->GetValue(text, CallerType::NonSystem);
        rv = NS_OK;
      } else {
        IME_LOGD("GeckoEditableSupport: GetText: Fail, unknow plain text");
      }
    } else if (EditableUtils::isContentEditable(focusedElement)) {
      rv = getContentEditableText(focusedElement, text);
    } else {
      IME_LOGD(
          "GeckoEditableSupport: GetText: Fail due to not supported input");
    }
  } while (0);
  text = Substring(text, aOffset, aLength);
  IME_LOGD("GeckoEditableSupport: GetText: rv:[%d] text:[%s]", rv,
           NS_ConvertUTF16toUTF8(text).get());
  if (aListener) {
    aListener->OnGetText(aId, rv, text);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::SetValue(uint32_t aId,
                               nsIEditableSupportListener* aListener,
                               const nsAString& aValue) {
  RefPtr<TextEventDispatcher> dispatcher = getTextEventDispatcherFromFocus();
  if (!dispatcher) {
    IME_LOGD("Invalid dispatcher");
    return NS_ERROR_ABORT;
  }
  IME_LOGD("-- GeckoEditableSupport::SetValue dispatcher:[%p]",
           dispatcher.get());
  nsresult rv = NS_ERROR_ABORT;
  do {
    // End current composition
    if (dispatcher->IsComposing()) {
      rv = EndComposition(0, nullptr, EmptyString());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        break;
      }
    }

    RefPtr<nsFocusManager> focusManager = nsFocusManager::GetFocusManager();
    if (!focusManager) {
      break;
    }
    RefPtr<Element> focusedElement = focusManager->GetFocusedElement();
    if (!focusedElement) {
      break;
    }
    nsCOMPtr<Document> doc = focusedElement->GetComposedDoc();
    if (!doc) {
      break;
    }
    RefPtr<Element> activeElement = doc->GetActiveElement();
    if (!activeElement) {
      break;
    }
    RefPtr<HTMLInputElement> inputElement =
        HTMLInputElement::FromNodeOrNull(activeElement);
    RefPtr<HTMLTextAreaElement> textArea =
        HTMLTextAreaElement::FromNodeOrNull(activeElement);
    ErrorResult erv;
    if (inputElement) {
      // Only file input need systemtype.
      inputElement->SetValue(aValue, CallerType::NonSystem, erv);
    } else if (textArea) {
      textArea->SetValue(aValue, erv);
    } else {
      IME_LOGD("GeckoEditableSupport::SetValue, element type not supported.");
      break;
    }
    if (NS_WARN_IF(erv.Failed())) {
      IME_LOGD("-- GeckoEditableSupport::SetValue, Fail to set value");
      break;
    }
    RefPtr<Event> event = NS_NewDOMEvent(doc, nullptr, nullptr);
    event->InitEvent(u"input"_ns, true, false);
    event->SetTrusted(true);
    activeElement->DispatchEvent(*event, erv);
    if (NS_WARN_IF(erv.Failed())) {
      IME_LOGD(
          "GeckoEditableSupport::SetValue, Failed to dispatch input event.");
      break;
    }
    nsAutoString type;
    activeElement->GetAttribute(u"type"_ns, type);
    if (isDateTimeTypes(type)) {
      RefPtr<Event> changeEvent = NS_NewDOMEvent(doc, nullptr, nullptr);
      changeEvent->InitEvent(u"change"_ns, true, false);
      changeEvent->SetTrusted(true);
      activeElement->DispatchEvent(*changeEvent, erv);
      if (NS_WARN_IF(erv.Failed())) {
        IME_LOGD(
            "GeckoEditableSupport::DoSetValue, Failed to dispatch change "
            "event.");
        break;
      }
    }
    rv = NS_OK;
  } while (0);
  IME_LOGD("GeckoEditableSupport: SetValue:[rv:%d, value:[%s]", rv,
           NS_ConvertUTF16toUTF8(aValue).get());
  if (aListener) {
    aListener->OnSetValue(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::ClearAll(uint32_t aId,
                               nsIEditableSupportListener* aListener) {
  IME_LOGD("-- GeckoEditableSupport::ClearAll");
  nsresult rv = NS_ERROR_ABORT;
  do {
    nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
    if (!focusManager) {
      break;
    }

    Element* focusedElement = focusManager->GetFocusedElement();
    if (!focusedElement) {
      break;
    }
    nsCOMPtr<Document> doc = focusedElement->GetComposedDoc();
    if (!doc) {
      break;
    }
    RefPtr<Element> activeElement = doc->GetActiveElement();
    if (!activeElement) {
      break;
    }
    RefPtr<HTMLInputElement> inputElement =
        HTMLInputElement::FromNodeOrNull(activeElement);
    if (inputElement) {
      rv = SetValue(0, nullptr, EmptyString());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        break;
      }
    } else {
      // non-input type such as TextAreaElement
      nsCOMPtr<nsIEditor> editor = getEditor(focusedElement);
      if (!editor) {
        break;
      }
      rv = editor->SelectAll();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        break;
      }
      rv = editor->DeleteSelection(nsIEditor::ePrevious, nsIEditor::eStrip);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        break;
      }
    }
    // Fire change event.
    ErrorResult erv;
    RefPtr<Event> changeEvent = NS_NewDOMEvent(doc, nullptr, nullptr);
    changeEvent->InitEvent(u"change"_ns, true, false);
    changeEvent->SetTrusted(true);
    activeElement->DispatchEvent(*changeEvent, erv);
    if (NS_WARN_IF(erv.Failed())) {
      IME_LOGD(
          "GeckoEditableSupport::ClearAll, Failed to dispatch change "
          "event.");
      break;
    }

  } while (0);

  if (aListener) {
    aListener->OnClearAll(aId, rv);
  }
  return rv;
}

nsresult GeckoEditableSupport::GetInputContextBag(
    nsInputContext* aInputContext) {
  nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
  if (!focusManager) return NS_ERROR_ABORT;
  Element* focusedElement = focusManager->GetFocusedElement();
  if (!focusedElement) return NS_ERROR_ABORT;
  nsCOMPtr<Document> doc = focusedElement->GetComposedDoc();
  Element* activeElement = doc->GetActiveElement();
  RefPtr<HTMLInputElement> inputElement =
      HTMLInputElement::FromNodeOrNull(activeElement);
  RefPtr<HTMLTextAreaElement> textArea =
      HTMLTextAreaElement::FromNodeOrNull(activeElement);
  nsAutoString attributeValue;

  // type
  activeElement->GetTagName(attributeValue);
  aInputContext->SetType(attributeValue);
  IME_LOGD("InputContext: type:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get());

  // inputType
  activeElement->GetAttribute(u"type"_ns, attributeValue);
  aInputContext->SetInputType(attributeValue);
  IME_LOGD("InputContext: inputType:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get());

  // value
  if (inputElement) {
    inputElement->GetValue(attributeValue, CallerType::NonSystem);
  } else if (textArea) {
    textArea->GetValue(attributeValue);
  } else {
    attributeValue = EmptyString();
  }
  aInputContext->SetValue(attributeValue);
  IME_LOGD("InputContext: value:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get());

  // max Using string since if inputType is date then max could be string.
  focusedElement->GetAttribute(u"max"_ns, attributeValue);
  aInputContext->SetMax(attributeValue);
  IME_LOGD("InputContext: max:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get());

  // min Same as max
  focusedElement->GetAttribute(u"min"_ns, attributeValue);
  aInputContext->SetMin(attributeValue);
  IME_LOGD("InputContext: min:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get());

  // lang
  focusedElement->GetAttribute(u"lang"_ns, attributeValue);
  aInputContext->SetLang(attributeValue);
  IME_LOGD("InputContext: lang:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get());

  // inputMode
  focusedElement->GetAttribute(u"x-inputmode"_ns, attributeValue);
  aInputContext->SetInputMode(attributeValue);
  IME_LOGD("InputContext: inputMode:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get());

  // voiceInputSupported
  focusedElement->GetAttribute(u"voiceInputSupported"_ns, attributeValue);
  aInputContext->SetVoiceInputSupported(
      attributeValue.EqualsIgnoreCase("true"));
  IME_LOGD("InputContext: voiceInputSupported:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get());

  // name
  focusedElement->GetAttribute(u"name"_ns, attributeValue);
  aInputContext->SetName(attributeValue);
  IME_LOGD("InputContext: name:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get());

  // selectionStart
  uint32_t start = getSelectionStart(focusedElement);
  aInputContext->SetSelectionStart(start);
  IME_LOGD("InputContext: selectionStart:[%lu]", start);

  // selectionEnd
  uint32_t end = getSelectionEnd(focusedElement);
  aInputContext->SetSelectionEnd(end);
  IME_LOGD("InputContext: selectionEnd:[%lu]", end);

  // Treat contenteditable element as a special text area field
  if (EditableUtils::isContentEditable(focusedElement)) {
    aInputContext->SetType(u"contenteditable"_ns);
    aInputContext->SetInputType(u"textarea"_ns);
    nsresult rv = getContentEditableText(focusedElement, attributeValue);
    if (rv != NS_OK) {
      return rv;
    }
    aInputContext->SetValue(attributeValue);
    IME_LOGD("InputContext: value:[%s]",
             NS_ConvertUTF16toUTF8(attributeValue).get());
  }

  // Choices
  RefPtr<HTMLSelectElement> selectElement =
      HTMLSelectElement::FromNodeOrNull(focusedElement);
  if (!selectElement) {
    return NS_OK;
  }

  RefPtr<nsInputContextChoices> inputContextChoices =
      new nsInputContextChoices();
  nsTArray<RefPtr<nsIInputContextChoicesInfo>> choices;
  choices.Clear();

  bool isMultiple = selectElement->Multiple();
  inputContextChoices->SetMultiple(isMultiple);
  IME_LOGD("HTMLSelectElement: multiple:[%s]", isMultiple ? "true" : "false");
  nsCOMPtr<nsINodeList> children;
  children = focusedElement->ChildNodes();
  for (uint32_t i = 0; i < children->Length(); i++) {
    nsCOMPtr<nsINode> child = children->Item(i);
    RefPtr<HTMLOptGroupElement> optGroupElement =
        HTMLOptGroupElement::FromNodeOrNull(child);
    RefPtr<HTMLOptionElement> optElement =
        HTMLOptionElement::FromNodeOrNull(child);
    if (optGroupElement) {
      RefPtr<nsInputContextChoicesInfo> groupInfo =
          new nsInputContextChoicesInfo();
      groupInfo->SetGroup(true);
      IME_LOGD("groupInfo: group:[true]");
      optGroupElement->GetAttribute(u"label"_ns, attributeValue);
      groupInfo->SetLabel(attributeValue);
      IME_LOGD("groupInfo: label:[%s]",
               NS_ConvertUTF16toUTF8(attributeValue).get());
      groupInfo->SetDisabled(optGroupElement->Disabled());
      IME_LOGD("groupInfo: disabled:[%s]",
               optGroupElement->Disabled() ? "true" : "false");
      choices.AppendElement(groupInfo);
      nsCOMPtr<nsINodeList> subChildren;
      subChildren = optGroupElement->ChildNodes();
      for (uint32_t j = 0; j < subChildren->Length(); j++) {
        nsCOMPtr<nsINode> subChild = subChildren->Item(j);
        RefPtr<HTMLOptionElement> subOptElement =
            HTMLOptionElement::FromNodeOrNull(subChild);
        if (!subOptElement) {
          continue;
        }
        RefPtr<nsInputContextChoicesInfo> subOptInfo =
            new nsInputContextChoicesInfo();
        subOptInfo->SetGroup(false);
        IME_LOGD("subOptInfo: group:[false]");
        subOptInfo->SetInGroup(true);
        IME_LOGD("subOptInfo: inGroup:[true]");
        subOptElement->GetAttribute(u"label"_ns, attributeValue);
        subOptInfo->SetLabel(attributeValue);
        IME_LOGD("subOptInfo: label:[%s]",
                 NS_ConvertUTF16toUTF8(attributeValue).get());
        subOptElement->GetAttribute(u"value"_ns, attributeValue);
        subOptInfo->SetValue(attributeValue);
        IME_LOGD("subOptInfo: value:[%s]",
                 NS_ConvertUTF16toUTF8(attributeValue).get());
        subOptElement->GetText(attributeValue);
        subOptInfo->SetText(attributeValue);
        IME_LOGD("subOptInfo: text:[%s]",
                 NS_ConvertUTF16toUTF8(attributeValue).get());
        subOptInfo->SetDisabled(subOptElement->Disabled());
        IME_LOGD("subOptInfo: Disabled:[%s]",
                 subOptElement->Disabled() ? "true" : "false");
        subOptInfo->SetSelected(subOptElement->Selected());
        IME_LOGD("subOptInfo: Selected:[%s]",
                 subOptElement->Selected() ? "true" : "false");
        subOptInfo->SetDefaultSelected(subOptElement->DefaultSelected());
        IME_LOGD("subOptInfo: DefaultSelected:[%s]",
                 subOptElement->DefaultSelected() ? "true" : "false");
        int32_t subIndex = subOptElement->Index();
        subOptInfo->SetOptionIndex(subIndex);
        IME_LOGD("subOptInfo: optionIndex:[%ld]", subIndex);
        choices.AppendElement(subOptInfo);
      }
    } else if (optElement) {
      RefPtr<nsInputContextChoicesInfo> optInfo =
          new nsInputContextChoicesInfo();
      optInfo->SetGroup(false);
      IME_LOGD("optInfo: group:[false]");
      optInfo->SetInGroup(false);
      IME_LOGD("optInfo: inGroup:[false]");
      optElement->GetAttribute(u"label"_ns, attributeValue);
      optInfo->SetLabel(attributeValue);
      IME_LOGD("optInfo: label:[%s]",
               NS_ConvertUTF16toUTF8(attributeValue).get());
      optElement->GetAttribute(u"value"_ns, attributeValue);
      optInfo->SetValue(attributeValue);
      IME_LOGD("optInfo: value:[%s]",
               NS_ConvertUTF16toUTF8(attributeValue).get());
      optElement->GetText(attributeValue);
      optInfo->SetText(attributeValue);
      IME_LOGD("optInfo: text:[%s]",
               NS_ConvertUTF16toUTF8(attributeValue).get());
      optInfo->SetDisabled(optElement->Disabled());
      IME_LOGD("optInfo: Disabled:[%s]",
               optElement->Disabled() ? "true" : "false");
      optInfo->SetSelected(optElement->Selected());
      IME_LOGD("optInfo: Selected:[%s]",
               optElement->Selected() ? "true" : "false");
      optInfo->SetDefaultSelected(optElement->DefaultSelected());
      IME_LOGD("optInfo: DefaultSelected:[%s]",
               optElement->DefaultSelected() ? "true" : "false");
      int32_t optionIndex = optElement->Index();
      optInfo->SetOptionIndex(optionIndex);
      IME_LOGD("optInfo: optionIndex:[%ld]", optionIndex);
      choices.AppendElement(optInfo);
    }
  }
  inputContextChoices->SetChoices(choices);
  aInputContext->SetInputContextChoices(inputContextChoices);
  aInputContext->SetEditableSupport(this);
  return NS_OK;
}

}  // namespace widget
}  // namespace mozilla
