/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "EditableUtils.h"

#include "nsString.h"
#include "nsGenericHTMLElement.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "mozilla/dom/HTMLTextAreaElement.h"

namespace mozilla {
namespace dom {

bool isIgnoredInputTypes(nsAString& inputType) {
  return inputType.EqualsIgnoreCase("button") ||
         inputType.EqualsIgnoreCase("file") ||
         inputType.EqualsIgnoreCase("checkbox") ||
         inputType.EqualsIgnoreCase("radio") ||
         inputType.EqualsIgnoreCase("reset") ||
         inputType.EqualsIgnoreCase("submit") ||
         inputType.EqualsIgnoreCase("image") ||
         inputType.EqualsIgnoreCase("range");
}

bool EditableUtils::isContentEditable(Element* aElement) {
  if (!aElement) {
    return false;
  }
  bool isContentEditable = false;
  bool isDesignModeOn = false;
  nsAutoString designMode;
  RefPtr<nsGenericHTMLElement> htmlElement =
      nsGenericHTMLElement::FromNodeOrNull(aElement);
  if (htmlElement) {
    isContentEditable = htmlElement->IsContentEditable();
  }
  RefPtr<Document> document = aElement->OwnerDoc();
  if (document) {
    document->GetDesignMode(designMode);
    isDesignModeOn = designMode.EqualsLiteral("on");
  }
  if (isContentEditable || isDesignModeOn) {
    return true;
  }

  document = document->OwnerDoc();
  if (!document) {
    return false;
  }
  document->GetDesignMode(designMode);
  isDesignModeOn = designMode.EqualsLiteral("on");
  return isDesignModeOn;
}

bool EditableUtils::isFocusableElement(Element* aElement) {
  if (!aElement) {
    return false;
  }
  RefPtr<HTMLSelectElement> selectElement =
      HTMLSelectElement::FromNodeOrNull(aElement);
  if (selectElement) {
    return true;
  }
  RefPtr<HTMLTextAreaElement> textAreaElement =
      HTMLTextAreaElement::FromNodeOrNull(aElement);
  if (textAreaElement) {
    return true;
  }
  RefPtr<HTMLOptionElement> optionElement =
      HTMLOptionElement::FromNodeOrNull(aElement);
  if (optionElement) {
    RefPtr<HTMLSelectElement> optionParent =
        HTMLSelectElement::FromNodeOrNull(optionElement->GetParentNode());
    if (optionParent) {
      return true;
    }
  }
  nsAutoString inputType;
  RefPtr<HTMLInputElement> inputElement =
      HTMLInputElement::FromNodeOrNull(aElement);
  if (inputElement) {
    if (inputElement->ReadOnly()) {
      return false;
    }
    inputElement->GetType(inputType);
    if (isIgnoredInputTypes(inputType)) {
      return false;
    }
    return true;
  }
  return false;
}

}  // namespace dom
}  // namespace mozilla
