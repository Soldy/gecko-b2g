/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AccessibleCaretGonk_h__
#define AccessibleCaretGonk_h__

#include "AccessibleCaret.h"

class nsIFrame;

namespace mozilla {

class AccessibleCaretGonk : public AccessibleCaret {
 public:
  using AccessibleCaret::AccessibleCaret;
  ~AccessibleCaretGonk() override;

  void SetAppearance(Appearance aAppearance) override;
  AccessibleCaret::PositionChangedResult SetPosition(nsIFrame* aFrame,
                                                     int32_t aOffset) override;

  void SetCaretElementStyle(const nsRect& aRect, float aZoomLevel);

 private:
  void SetForceAppearance(Appearance aForceAppearance);

  Appearance mForceAppearance = Appearance::None;
};  // class AccessibleCaretGonk

}  // namespace mozilla

#endif  // AccessibleCaretGonk_h__
