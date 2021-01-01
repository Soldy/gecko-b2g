/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_nsInputContext_h
#define mozilla_dom_nsInputContext_h

#include "nsIInputContext.h"

namespace mozilla {
namespace dom {

class nsInputContextChoicesInfo final : public nsIInputContextChoicesInfo {
 public:
  nsInputContextChoicesInfo() = default;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTCONTEXTCHOICESINFO

  void Init(bool group, bool inGroup, bool disabled, bool selected,
            bool defaultSelected, const nsAString& text, const nsAString& label,
            const nsAString& value, int32_t optionIndex);

  void SetGroup(bool aGroup);
  void SetInGroup(bool aInGroup);
  void SetDisabled(bool aDisabled);
  void SetSelected(bool aSelected);
  void SetDefaultSelected(bool aDefaultSelected);
  void SetText(const nsAString& aText);
  void SetLabel(const nsAString& aLabel);
  void SetValue(const nsAString& aValue);
  void SetOptionIndex(int32_t aOptionIndex);

 private:
  virtual ~nsInputContextChoicesInfo() = default;

  bool mGroup;
  bool mInGroup;
  bool mDisabled;
  bool mSelected;
  bool mDefaultSelected;
  nsString mText;
  nsString mLabel;
  nsString mValue;
  int32_t mOptionIndex;
};

class nsInputContextChoices final : public nsIInputContextChoices {
 public:
  nsInputContextChoices() = default;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTCONTEXTCHOICES
  friend class nsInputContext;
  void Init(bool multiple,
            const nsTArray<RefPtr<nsIInputContextChoicesInfo>>& choices);

  void SetMultiple(bool aMultiple);
  void SetChoices(const nsTArray<RefPtr<nsIInputContextChoicesInfo>>& aChoices);

 private:
  virtual ~nsInputContextChoices() = default;

  bool mMultiple;
  nsTArray<RefPtr<nsIInputContextChoicesInfo>> mChoices;
};

class nsInputContext final : public nsIInputContext {
 public:
  nsInputContext() = default;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTCONTEXT

  void Init(const nsAString& type, const nsAString& inputType,
            const nsAString& inputMode, const nsAString& lang,
            const nsAString& name, const nsAString& max, const nsAString& min,
            const nsAString& value, bool voiceInputSupported,
            uint32_t selectionStart, uint32_t selectionEnd,
            nsInputContextChoices* choices, const nsAString& aMaxLength,
            nsIEditableSupport* editable);

  void SetType(const nsAString& aType);
  void SetInputType(const nsAString& aInputType);
  void SetInputMode(const nsAString& aInputMode);
  void SetLang(const nsAString& aLang);
  void SetName(const nsAString& aName);
  void SetMax(const nsAString& aMax);
  void SetMin(const nsAString& aMin);
  void SetValue(const nsAString& aValue);
  void SetVoiceInputSupported(bool aVoiceInputSupported);
  void SetSelectionStart(uint32_t aSelectionStart);
  void SetSelectionEnd(uint32_t aSelectionEnd);
  void SetInputContextChoices(nsInputContextChoices* aChoices);
  void SetMaxLength(const nsAString& aMaxLength);
  void SetEditableSupport(nsIEditableSupport* aEditableSupport);

 private:
  virtual ~nsInputContext() = default;

  nsString mType;
  nsString mInputType;
  nsString mInputMode;
  nsString mLang;
  nsString mName;
  nsString mMax;
  nsString mMin;
  nsString mValue;
  bool mVoiceInputSupported;
  uint32_t mSelectionStart;
  uint32_t mSelectionEnd;
  RefPtr<nsInputContextChoices> mChoices;
  nsString mMaxLength;
  RefPtr<nsIEditableSupport> mEditableSupport;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_nsInputContext_h */
