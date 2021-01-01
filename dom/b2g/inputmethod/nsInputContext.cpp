/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsInputContext.h"
#include "nsIMutableArray.h"
#include "mozilla/dom/IMELog.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(nsInputContextChoicesInfo, nsIInputContextChoicesInfo)

void nsInputContextChoicesInfo::Init(bool aGroup, bool aInGroup, bool aDisabled,
                                     bool aSelected, bool aDefaultSelected,
                                     const nsAString& aText,
                                     const nsAString& aLabel,
                                     const nsAString& aValue,
                                     int32_t aOptionIndex) {
  mGroup = aGroup;
  mInGroup = aInGroup;
  mDisabled = aDisabled;
  mSelected = aSelected;
  mDefaultSelected = aDefaultSelected;
  mText.Assign(aText);
  mLabel.Assign(aLabel);
  mValue.Assign(aValue);
  mOptionIndex = aOptionIndex;
}

void nsInputContextChoicesInfo::SetGroup(bool aGroup) { mGroup = aGroup; }
void nsInputContextChoicesInfo::SetInGroup(bool aInGroup) {
  mInGroup = aInGroup;
}
void nsInputContextChoicesInfo::SetDisabled(bool aDisabled) {
  mDisabled = aDisabled;
}
void nsInputContextChoicesInfo::SetSelected(bool aSelected) {
  mSelected = aSelected;
}
void nsInputContextChoicesInfo::SetDefaultSelected(bool aDefaultSelected) {
  mDefaultSelected = aDefaultSelected;
}
void nsInputContextChoicesInfo::SetText(const nsAString& aText) {
  mText = aText;
}
void nsInputContextChoicesInfo::SetLabel(const nsAString& aLabel) {
  mLabel = aLabel;
}
void nsInputContextChoicesInfo::SetValue(const nsAString& aValue) {
  mValue = aValue;
}
void nsInputContextChoicesInfo::SetOptionIndex(int32_t aOptionIndex) {
  mOptionIndex = aOptionIndex;
}

// nsIInputContextChoicesInfo methods

NS_IMETHODIMP
nsInputContextChoicesInfo::GetGroup(bool* aResult) {
  *aResult = mGroup;
  return NS_OK;
}

NS_IMETHODIMP
nsInputContextChoicesInfo::GetInGroup(bool* aResult) {
  *aResult = mInGroup;
  return NS_OK;
}

NS_IMETHODIMP
nsInputContextChoicesInfo::GetDisabled(bool* aResult) {
  *aResult = mDisabled;
  return NS_OK;
}

NS_IMETHODIMP
nsInputContextChoicesInfo::GetSelected(bool* aResult) {
  *aResult = mSelected;
  return NS_OK;
}

NS_IMETHODIMP
nsInputContextChoicesInfo::GetDefaultSelected(bool* aResult) {
  *aResult = mDefaultSelected;
  return NS_OK;
}

NS_IMETHODIMP
nsInputContextChoicesInfo::GetText(nsAString& aResult) {
  aResult.Assign(mText);
  return NS_OK;
}

NS_IMETHODIMP
nsInputContextChoicesInfo::GetLabel(nsAString& aResult) {
  aResult.Assign(mLabel);
  return NS_OK;
}

NS_IMETHODIMP
nsInputContextChoicesInfo::GetValue(nsAString& aResult) {
  aResult.Assign(mValue);
  return NS_OK;
}

NS_IMETHODIMP
nsInputContextChoicesInfo::GetOptionIndex(int32_t* aResult) {
  *aResult = mOptionIndex;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsInputContextChoices, nsIInputContextChoices)

void nsInputContextChoices::Init(
    bool aMultiple,
    const nsTArray<RefPtr<nsIInputContextChoicesInfo>>& aChoices) {
  mMultiple = aMultiple;
  mChoices.Clear();
  for (auto& item : aChoices) {
    mChoices.AppendElement(item);
  }
}

void nsInputContextChoices::SetMultiple(bool aMultiple) {
  mMultiple = aMultiple;
}

void nsInputContextChoices::SetChoices(
    const nsTArray<RefPtr<nsIInputContextChoicesInfo>>& aChoices) {
  mChoices = aChoices.Clone();
}

// nsIInputContextChoices methods

NS_IMETHODIMP
nsInputContextChoices::GetMultiple(bool* aResult) {
  *aResult = mMultiple;
  return NS_OK;
}

NS_IMETHODIMP
nsInputContextChoices::GetChoices(
    nsTArray<RefPtr<nsIInputContextChoicesInfo>>& aResult) {
  aResult = mChoices.Clone();
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsInputContext, nsIInputContext)

void nsInputContext::Init(const nsAString& aType, const nsAString& aInputType,
                          const nsAString& aInputMode, const nsAString& aLang,
                          const nsAString& aName, const nsAString& aMax,
                          const nsAString& aMin, const nsAString& aValue,
                          bool aVoiceInputSupported, uint32_t aSelectionStart,
                          uint32_t aSelectionEnd,
                          nsInputContextChoices* aChoices,
                          const nsAString& aMaxLength,
                          nsIEditableSupport* aEditableSupport) {
  mType.Assign(aType);
  mInputType.Assign(aInputType);
  mInputMode.Assign(aInputMode);
  mLang.Assign(aLang);
  mName.Assign(aName);
  mMax.Assign(aMax);
  mMin.Assign(aMin);
  mValue.Assign(aValue);
  mVoiceInputSupported = aVoiceInputSupported;
  mSelectionStart = aSelectionStart;
  mSelectionEnd = aSelectionEnd;
  mChoices = aChoices;
  mEditableSupport = aEditableSupport;
  mMaxLength.Assign(aMaxLength);
}

void nsInputContext::SetType(const nsAString& aType) { mType = aType; }
void nsInputContext::SetInputType(const nsAString& aInputType) {
  mInputType = aInputType;
}
void nsInputContext::SetInputMode(const nsAString& aInputMode) {
  mInputMode = aInputMode;
}
void nsInputContext::SetLang(const nsAString& aLang) { mLang = aLang; }
void nsInputContext::SetName(const nsAString& aName) { mName = aName; }
void nsInputContext::SetMax(const nsAString& aMax) { mMax = aMax; }
void nsInputContext::SetMin(const nsAString& aMin) { mMin = aMin; }
void nsInputContext::SetValue(const nsAString& aValue) { mValue = aValue; }
void nsInputContext::SetVoiceInputSupported(bool aVoiceInputSupported) {
  mVoiceInputSupported = aVoiceInputSupported;
}
void nsInputContext::SetSelectionStart(uint32_t aSelectionStart) {
  mSelectionStart = aSelectionStart;
}
void nsInputContext::SetSelectionEnd(uint32_t aSelectionEnd) {
  mSelectionEnd = aSelectionEnd;
}
void nsInputContext::SetInputContextChoices(nsInputContextChoices* aChoices) {
  mChoices = aChoices;
}

void nsInputContext::SetMaxLength(const nsAString& aMaxLength) {
  mMaxLength = aMaxLength;
}

void nsInputContext::SetEditableSupport(nsIEditableSupport* aEditableSupport) {
  mEditableSupport = aEditableSupport;
}

// nsIInputContext methods

NS_IMETHODIMP
nsInputContext::GetType(nsAString& aResult) {
  aResult.Assign(mType);
  return NS_OK;
}

NS_IMETHODIMP
nsInputContext::GetInputType(nsAString& aResult) {
  aResult.Assign(mInputType);
  return NS_OK;
}

NS_IMETHODIMP
nsInputContext::GetInputMode(nsAString& aResult) {
  aResult.Assign(mInputMode);
  return NS_OK;
}

NS_IMETHODIMP
nsInputContext::GetLang(nsAString& aResult) {
  aResult.Assign(mLang);
  return NS_OK;
}

NS_IMETHODIMP
nsInputContext::GetName(nsAString& aResult) {
  aResult.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
nsInputContext::GetMax(nsAString& aResult) {
  aResult.Assign(mMax);
  return NS_OK;
}

NS_IMETHODIMP
nsInputContext::GetMin(nsAString& aResult) {
  aResult.Assign(mMin);
  return NS_OK;
}

NS_IMETHODIMP
nsInputContext::GetValue(nsAString& aResult) {
  aResult.Assign(mValue);
  return NS_OK;
}

NS_IMETHODIMP
nsInputContext::GetVoiceInputSupported(bool* aResult) {
  *aResult = mVoiceInputSupported;
  return NS_OK;
}

NS_IMETHODIMP
nsInputContext::GetSelectionStart(uint32_t* aResult) {
  *aResult = mSelectionStart;
  return NS_OK;
}

NS_IMETHODIMP
nsInputContext::GetSelectionEnd(uint32_t* aResult) {
  *aResult = mSelectionEnd;
  return NS_OK;
}

NS_IMETHODIMP
nsInputContext::GetChoices(nsIInputContextChoices** aResult) {
  *aResult = do_AddRef(mChoices).take();
  return NS_OK;
}

NS_IMETHODIMP
nsInputContext::GetMaxLength(nsAString& aResult) {
  aResult.Assign(mMaxLength);
  return NS_OK;
}

NS_IMETHODIMP
nsInputContext::GetEditableSupport(nsIEditableSupport** aResult) {
  *aResult = do_AddRef(mEditableSupport).take();
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
