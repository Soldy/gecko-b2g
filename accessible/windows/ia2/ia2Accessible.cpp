/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleWrap.h"

#include "Accessible2_i.c"
#include "Accessible2_2_i.c"
#include "Accessible2_3_i.c"
#include "AccessibleRole.h"
#include "AccessibleStates.h"

#include "Compatibility.h"
#include "ia2AccessibleRelation.h"
#include "IUnknownImpl.h"
#include "nsCoreUtils.h"
#include "nsIAccessibleTypes.h"
#include "mozilla/a11y/PDocAccessible.h"
#include "Relation.h"
#include "TextRange-inl.h"
#include "nsAccessibilityService.h"

#include "mozilla/PresShell.h"
#include "nsIPersistentProperties2.h"
#include "nsISimpleEnumerator.h"

using namespace mozilla;
using namespace mozilla::a11y;

template <typename String>
static void EscapeAttributeChars(String& aStr);

////////////////////////////////////////////////////////////////////////////////
// ia2Accessible
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP
ia2Accessible::QueryInterface(REFIID iid, void** ppv) {
  if (!ppv) return E_INVALIDARG;

  *ppv = nullptr;

  // NOTE: If any new versions of IAccessible2 are added here, they should
  // also be added to the IA2 Handler in
  // /accessible/ipc/win/handler/AccessibleHandler.cpp

  if (IID_IAccessible2_3 == iid)
    *ppv = static_cast<IAccessible2_3*>(this);
  else if (IID_IAccessible2_2 == iid)
    *ppv = static_cast<IAccessible2_2*>(this);
  else if (IID_IAccessible2 == iid)
    *ppv = static_cast<IAccessible2*>(this);

  if (*ppv) {
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

////////////////////////////////////////////////////////////////////////////////
// IAccessible2

STDMETHODIMP
ia2Accessible::get_nRelations(long* aNRelations) {
  if (!aNRelations) return E_INVALIDARG;
  *aNRelations = 0;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct()) return CO_E_OBJNOTCONNECTED;

  MOZ_ASSERT(!acc->IsProxy());

  for (uint32_t idx = 0; idx < ArrayLength(sRelationTypePairs); idx++) {
    if (sRelationTypePairs[idx].second == IA2_RELATION_NULL) continue;

    Relation rel = acc->RelationByType(sRelationTypePairs[idx].first);
    if (rel.Next()) (*aNRelations)++;
  }
  return S_OK;
}

STDMETHODIMP
ia2Accessible::get_relation(long aRelationIndex,
                            IAccessibleRelation** aRelation) {
  if (!aRelation || aRelationIndex < 0) return E_INVALIDARG;
  *aRelation = nullptr;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct()) return CO_E_OBJNOTCONNECTED;

  MOZ_ASSERT(!acc->IsProxy());

  long relIdx = 0;
  for (uint32_t idx = 0; idx < ArrayLength(sRelationTypePairs); idx++) {
    if (sRelationTypePairs[idx].second == IA2_RELATION_NULL) continue;

    RelationType relationType = sRelationTypePairs[idx].first;
    Relation rel = acc->RelationByType(relationType);
    RefPtr<ia2AccessibleRelation> ia2Relation =
        new ia2AccessibleRelation(relationType, &rel);
    if (ia2Relation->HasTargets()) {
      if (relIdx == aRelationIndex) {
        acc->AssociateCOMObjectForDisconnection(ia2Relation);
        ia2Relation.forget(aRelation);
        return S_OK;
      }

      relIdx++;
    }
  }

  return E_INVALIDARG;
}

STDMETHODIMP
ia2Accessible::get_relations(long aMaxRelations,
                             IAccessibleRelation** aRelation,
                             long* aNRelations) {
  if (!aRelation || !aNRelations || aMaxRelations <= 0) return E_INVALIDARG;
  *aNRelations = 0;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct()) return CO_E_OBJNOTCONNECTED;

  MOZ_ASSERT(!acc->IsProxy());

  for (uint32_t idx = 0;
       idx < ArrayLength(sRelationTypePairs) && *aNRelations < aMaxRelations;
       idx++) {
    if (sRelationTypePairs[idx].second == IA2_RELATION_NULL) continue;

    RelationType relationType = sRelationTypePairs[idx].first;
    Relation rel = acc->RelationByType(relationType);
    RefPtr<ia2AccessibleRelation> ia2Rel =
        new ia2AccessibleRelation(relationType, &rel);
    if (ia2Rel->HasTargets()) {
      acc->AssociateCOMObjectForDisconnection(ia2Rel);
      ia2Rel.forget(aRelation + (*aNRelations));
      (*aNRelations)++;
    }
  }
  return S_OK;
}

STDMETHODIMP
ia2Accessible::role(long* aRole) {
  if (!aRole) return E_INVALIDARG;
  *aRole = 0;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct()) return CO_E_OBJNOTCONNECTED;

#define ROLE(_geckoRole, stringRole, atkRole, macRole, macSubrole, msaaRole, \
             ia2Role, androidClass, nameRule)                                \
  case roles::_geckoRole:                                                    \
    *aRole = ia2Role;                                                        \
    break;

  a11y::role geckoRole;
  MOZ_ASSERT(!acc->IsProxy());
  geckoRole = acc->Role();
  switch (geckoRole) {
#include "RoleMap.h"
    default:
      MOZ_CRASH("Unknown role.");
  }

#undef ROLE

  // Special case, if there is a ROLE_ROW inside of a ROLE_TREE_TABLE, then call
  // the IA2 role a ROLE_OUTLINEITEM.
  MOZ_ASSERT(!acc->IsProxy());
  if (geckoRole == roles::ROW) {
    LocalAccessible* xpParent = acc->LocalParent();
    if (xpParent && xpParent->Role() == roles::TREE_TABLE)
      *aRole = ROLE_SYSTEM_OUTLINEITEM;
  }

  return S_OK;
}

// XXX Use MOZ_CAN_RUN_SCRIPT_BOUNDARY for now due to bug 1543294.
MOZ_CAN_RUN_SCRIPT_BOUNDARY STDMETHODIMP
ia2Accessible::scrollTo(enum IA2ScrollType aScrollType) {
  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct()) return CO_E_OBJNOTCONNECTED;

  MOZ_ASSERT(!acc->IsProxy());
  RefPtr<PresShell> presShell = acc->Document()->PresShellPtr();
  nsCOMPtr<nsIContent> content = acc->GetContent();
  nsCoreUtils::ScrollTo(presShell, content, aScrollType);

  return S_OK;
}

STDMETHODIMP
ia2Accessible::scrollToPoint(enum IA2CoordinateType aCoordType, long aX,
                             long aY) {
  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct()) return CO_E_OBJNOTCONNECTED;

  uint32_t geckoCoordType =
      (aCoordType == IA2_COORDTYPE_SCREEN_RELATIVE)
          ? nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE
          : nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE;

  MOZ_ASSERT(!acc->IsProxy());
  acc->ScrollToPoint(geckoCoordType, aX, aY);

  return S_OK;
}

STDMETHODIMP
ia2Accessible::get_groupPosition(long* aGroupLevel, long* aSimilarItemsInGroup,
                                 long* aPositionInGroup) {
  if (!aGroupLevel || !aSimilarItemsInGroup || !aPositionInGroup)
    return E_INVALIDARG;

  *aGroupLevel = 0;
  *aSimilarItemsInGroup = 0;
  *aPositionInGroup = 0;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct()) return CO_E_OBJNOTCONNECTED;

  GroupPos groupPos = acc->GroupPosition();

  // Group information for accessibles having level only (like html headings
  // elements) isn't exposed by this method. AT should look for 'level' object
  // attribute.
  if (!groupPos.setSize && !groupPos.posInSet) return S_FALSE;

  *aGroupLevel = groupPos.level;
  *aSimilarItemsInGroup = groupPos.setSize;
  *aPositionInGroup = groupPos.posInSet;

  return S_OK;
}

STDMETHODIMP
ia2Accessible::get_states(AccessibleStates* aStates) {
  if (!aStates) return E_INVALIDARG;
  *aStates = 0;

  // XXX: bug 344674 should come with better approach that we have here.

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct()) {
    *aStates = IA2_STATE_DEFUNCT;
    return S_OK;
  }

  uint64_t state;
  MOZ_ASSERT(!acc->IsProxy());
  state = acc->State();

  if (state & states::INVALID) *aStates |= IA2_STATE_INVALID_ENTRY;
  if (state & states::REQUIRED) *aStates |= IA2_STATE_REQUIRED;

  // The following IA2 states are not supported by Gecko
  // IA2_STATE_ARMED
  // IA2_STATE_MANAGES_DESCENDANTS
  // IA2_STATE_ICONIFIED
  // IA2_STATE_INVALID // This is not a state, it is the absence of a state

  if (state & states::ACTIVE) *aStates |= IA2_STATE_ACTIVE;
  if (state & states::DEFUNCT) *aStates |= IA2_STATE_DEFUNCT;
  if (state & states::EDITABLE) *aStates |= IA2_STATE_EDITABLE;
  if (state & states::HORIZONTAL) *aStates |= IA2_STATE_HORIZONTAL;
  if (state & states::MODAL) *aStates |= IA2_STATE_MODAL;
  if (state & states::MULTI_LINE) *aStates |= IA2_STATE_MULTI_LINE;
  if (state & states::OPAQUE1) *aStates |= IA2_STATE_OPAQUE;
  if (state & states::SELECTABLE_TEXT) *aStates |= IA2_STATE_SELECTABLE_TEXT;
  if (state & states::SINGLE_LINE) *aStates |= IA2_STATE_SINGLE_LINE;
  if (state & states::STALE) *aStates |= IA2_STATE_STALE;
  if (state & states::SUPPORTS_AUTOCOMPLETION)
    *aStates |= IA2_STATE_SUPPORTS_AUTOCOMPLETION;
  if (state & states::TRANSIENT) *aStates |= IA2_STATE_TRANSIENT;
  if (state & states::VERTICAL) *aStates |= IA2_STATE_VERTICAL;
  if (state & states::CHECKED) *aStates |= IA2_STATE_CHECKABLE;
  if (state & states::PINNED) *aStates |= IA2_STATE_PINNED;

  return S_OK;
}

STDMETHODIMP
ia2Accessible::get_extendedRole(BSTR* aExtendedRole) {
  if (!aExtendedRole) return E_INVALIDARG;

  *aExtendedRole = nullptr;
  return E_NOTIMPL;
}

STDMETHODIMP
ia2Accessible::get_localizedExtendedRole(BSTR* aLocalizedExtendedRole) {
  if (!aLocalizedExtendedRole) return E_INVALIDARG;

  *aLocalizedExtendedRole = nullptr;
  return E_NOTIMPL;
}

STDMETHODIMP
ia2Accessible::get_nExtendedStates(long* aNExtendedStates) {
  if (!aNExtendedStates) return E_INVALIDARG;

  *aNExtendedStates = 0;
  return E_NOTIMPL;
}

STDMETHODIMP
ia2Accessible::get_extendedStates(long aMaxExtendedStates,
                                  BSTR** aExtendedStates,
                                  long* aNExtendedStates) {
  if (!aExtendedStates || !aNExtendedStates) return E_INVALIDARG;

  *aExtendedStates = nullptr;
  *aNExtendedStates = 0;
  return E_NOTIMPL;
}

STDMETHODIMP
ia2Accessible::get_localizedExtendedStates(long aMaxLocalizedExtendedStates,
                                           BSTR** aLocalizedExtendedStates,
                                           long* aNLocalizedExtendedStates) {
  if (!aLocalizedExtendedStates || !aNLocalizedExtendedStates)
    return E_INVALIDARG;

  *aLocalizedExtendedStates = nullptr;
  *aNLocalizedExtendedStates = 0;
  return E_NOTIMPL;
}

STDMETHODIMP
ia2Accessible::get_uniqueID(long* aUniqueID) {
  if (!aUniqueID) return E_INVALIDARG;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  *aUniqueID = MsaaAccessible::GetChildIDFor(acc);
  return S_OK;
}

STDMETHODIMP
ia2Accessible::get_windowHandle(HWND* aWindowHandle) {
  if (!aWindowHandle) return E_INVALIDARG;
  *aWindowHandle = 0;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct()) return CO_E_OBJNOTCONNECTED;

  *aWindowHandle = AccessibleWrap::GetHWNDFor(acc);
  return S_OK;
}

STDMETHODIMP
ia2Accessible::get_indexInParent(long* aIndexInParent) {
  if (!aIndexInParent) return E_INVALIDARG;
  *aIndexInParent = -1;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct()) return CO_E_OBJNOTCONNECTED;

  MOZ_ASSERT(!acc->IsProxy());
  *aIndexInParent = acc->IndexInParent();

  if (*aIndexInParent == -1) return S_FALSE;

  return S_OK;
}

STDMETHODIMP
ia2Accessible::get_locale(IA2Locale* aLocale) {
  if (!aLocale) return E_INVALIDARG;

  // Language codes consist of a primary code and a possibly empty series of
  // subcodes: language-code = primary-code ( "-" subcode )*
  // Two-letter primary codes are reserved for [ISO639] language abbreviations.
  // Any two-letter subcode is understood to be a [ISO3166] country code.

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct()) return CO_E_OBJNOTCONNECTED;

  nsAutoString lang;
  acc->Language(lang);

  // If primary code consists from two letters then expose it as language.
  int32_t offset = lang.FindChar('-', 0);
  if (offset == -1) {
    if (lang.Length() == 2) {
      aLocale->language = ::SysAllocString(lang.get());
      return S_OK;
    }
  } else if (offset == 2) {
    aLocale->language = ::SysAllocStringLen(lang.get(), 2);

    // If the first subcode consists from two letters then expose it as
    // country.
    offset = lang.FindChar('-', 3);
    if (offset == -1) {
      if (lang.Length() == 5) {
        aLocale->country = ::SysAllocString(lang.get() + 3);
        return S_OK;
      }
    } else if (offset == 5) {
      aLocale->country = ::SysAllocStringLen(lang.get() + 3, 2);
    }
  }

  // Expose as a string if primary code or subcode cannot point to language or
  // country abbreviations or if there are more than one subcode.
  aLocale->variant = ::SysAllocString(lang.get());
  return S_OK;
}

STDMETHODIMP
ia2Accessible::get_attributes(BSTR* aAttributes) {
  if (!aAttributes) return E_INVALIDARG;
  *aAttributes = nullptr;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct()) return CO_E_OBJNOTCONNECTED;

  // The format is name:value;name:value; with \ for escaping these
  // characters ":;=,\".
  if (!acc->IsProxy()) {
    nsCOMPtr<nsIPersistentProperties> attributes = acc->Attributes();
    return ConvertToIA2Attributes(attributes, aAttributes);
  }

  MOZ_ASSERT(!acc->IsProxy());
  return E_UNEXPECTED;
}

////////////////////////////////////////////////////////////////////////////////
// IAccessible2_2

STDMETHODIMP
ia2Accessible::get_attribute(BSTR name, VARIANT* aAttribute) {
  if (!aAttribute) return E_INVALIDARG;

  return E_NOTIMPL;
}

STDMETHODIMP
ia2Accessible::get_accessibleWithCaret(IUnknown** aAccessible,
                                       long* aCaretOffset) {
  if (!aAccessible || !aCaretOffset) return E_INVALIDARG;

  *aAccessible = nullptr;
  *aCaretOffset = -1;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct()) return CO_E_OBJNOTCONNECTED;

  int32_t caretOffset = -1;
  LocalAccessible* accWithCaret =
      SelectionMgr()->AccessibleWithCaret(&caretOffset);
  if (!accWithCaret || acc->Document() != accWithCaret->Document())
    return S_FALSE;

  LocalAccessible* child = accWithCaret;
  while (!child->IsDoc() && child != acc) child = child->LocalParent();

  if (child != acc) return S_FALSE;

  *aAccessible =
      static_cast<IAccessible2*>(static_cast<AccessibleWrap*>(accWithCaret));
  (*aAccessible)->AddRef();
  *aCaretOffset = caretOffset;
  return S_OK;
}

STDMETHODIMP
ia2Accessible::get_relationTargetsOfType(BSTR aType, long aMaxTargets,
                                         IUnknown*** aTargets,
                                         long* aNTargets) {
  if (!aTargets || !aNTargets || aMaxTargets < 0) return E_INVALIDARG;
  *aNTargets = 0;

  Maybe<RelationType> relationType;
  for (uint32_t idx = 0; idx < ArrayLength(sRelationTypePairs); idx++) {
    if (wcscmp(aType, sRelationTypePairs[idx].second) == 0) {
      relationType.emplace(sRelationTypePairs[idx].first);
      break;
    }
  }
  if (!relationType) return E_INVALIDARG;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct()) return CO_E_OBJNOTCONNECTED;

  nsTArray<LocalAccessible*> targets;
  MOZ_ASSERT(!acc->IsProxy());
  Relation rel = acc->RelationByType(*relationType);
  LocalAccessible* target = nullptr;
  while (
      (target = rel.Next()) &&
      (aMaxTargets == 0 || static_cast<long>(targets.Length()) < aMaxTargets)) {
    targets.AppendElement(target);
  }

  *aNTargets = targets.Length();
  *aTargets =
      static_cast<IUnknown**>(::CoTaskMemAlloc(sizeof(IUnknown*) * *aNTargets));
  if (!*aTargets) return E_OUTOFMEMORY;

  for (int32_t i = 0; i < *aNTargets; i++) {
    AccessibleWrap* target = static_cast<AccessibleWrap*>(targets[i]);
    (*aTargets)[i] = static_cast<IAccessible2*>(target);
    (*aTargets)[i]->AddRef();
  }

  return S_OK;
}

STDMETHODIMP
ia2Accessible::get_selectionRanges(IA2Range** aRanges, long* aNRanges) {
  if (!aRanges || !aNRanges) return E_INVALIDARG;

  *aNRanges = 0;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct()) return CO_E_OBJNOTCONNECTED;

  AutoTArray<TextRange, 1> ranges;
  acc->Document()->SelectionRanges(&ranges);
  ranges.RemoveElementsBy([acc](auto& range) { return !range.Crop(acc); });

  *aNRanges = ranges.Length();
  *aRanges =
      static_cast<IA2Range*>(::CoTaskMemAlloc(sizeof(IA2Range) * *aNRanges));
  if (!*aRanges) return E_OUTOFMEMORY;

  for (uint32_t idx = 0; idx < static_cast<uint32_t>(*aNRanges); idx++) {
    AccessibleWrap* anchor =
        static_cast<AccessibleWrap*>(ranges[idx].StartContainer());
    (*aRanges)[idx].anchor = static_cast<IAccessible2*>(anchor);
    anchor->AddRef();

    (*aRanges)[idx].anchorOffset = ranges[idx].StartOffset();

    AccessibleWrap* active =
        static_cast<AccessibleWrap*>(ranges[idx].EndContainer());
    (*aRanges)[idx].active = static_cast<IAccessible2*>(active);
    active->AddRef();

    (*aRanges)[idx].activeOffset = ranges[idx].EndOffset();
  }

  return S_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Helpers

template <typename String>
static inline void EscapeAttributeChars(String& aStr) {
  int32_t offset = 0;
  static const char kCharsToEscape[] = ":;=,\\";
  while ((offset = aStr.FindCharInSet(kCharsToEscape, offset)) != kNotFound) {
    aStr.Insert('\\', offset);
    offset += 2;
  }
}

HRESULT
ia2Accessible::ConvertToIA2Attributes(nsTArray<Attribute>* aAttributes,
                                      BSTR* aIA2Attributes) {
  nsString attrStr;
  size_t attrCount = aAttributes->Length();
  for (size_t i = 0; i < attrCount; i++) {
    EscapeAttributeChars(aAttributes->ElementAt(i).Name());
    EscapeAttributeChars(aAttributes->ElementAt(i).Value());
    AppendUTF8toUTF16(aAttributes->ElementAt(i).Name(), attrStr);
    attrStr.Append(':');
    attrStr.Append(aAttributes->ElementAt(i).Value());
    attrStr.Append(';');
  }

  if (attrStr.IsEmpty()) return S_FALSE;

  *aIA2Attributes = ::SysAllocStringLen(attrStr.get(), attrStr.Length());
  return *aIA2Attributes ? S_OK : E_OUTOFMEMORY;
}

HRESULT
ia2Accessible::ConvertToIA2Attributes(nsIPersistentProperties* aAttributes,
                                      BSTR* aIA2Attributes) {
  *aIA2Attributes = nullptr;

  // The format is name:value;name:value; with \ for escaping these
  // characters ":;=,\".

  if (!aAttributes) return S_FALSE;

  nsCOMPtr<nsISimpleEnumerator> propEnum;
  aAttributes->Enumerate(getter_AddRefs(propEnum));
  if (!propEnum) return E_FAIL;

  nsAutoString strAttrs;

  bool hasMore = false;
  while (NS_SUCCEEDED(propEnum->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> propSupports;
    propEnum->GetNext(getter_AddRefs(propSupports));

    nsCOMPtr<nsIPropertyElement> propElem(do_QueryInterface(propSupports));
    if (!propElem) return E_FAIL;

    nsAutoCString name;
    if (NS_FAILED(propElem->GetKey(name))) return E_FAIL;

    EscapeAttributeChars(name);

    nsAutoString value;
    if (NS_FAILED(propElem->GetValue(value))) return E_FAIL;

    EscapeAttributeChars(value);

    AppendUTF8toUTF16(name, strAttrs);
    strAttrs.Append(':');
    strAttrs.Append(value);
    strAttrs.Append(';');
  }

  if (strAttrs.IsEmpty()) return S_FALSE;

  *aIA2Attributes = ::SysAllocStringLen(strAttrs.get(), strAttrs.Length());
  return *aIA2Attributes ? S_OK : E_OUTOFMEMORY;
}
