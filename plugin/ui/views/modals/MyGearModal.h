// Fork (TK3J): the "My Gear" picker. Opens on + / swap before the TONE3000
// search: the player's pinned, recent and on-disk tones in one scrolling
// list. Picking a row loads that file or folder into the slot; "Browse
// TONE3000" falls through to the stock search for the same slot.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

#include "services/MyGear.h"
#include "widgets/ModalLayer.h"

namespace t3k::ui {

class MyGearModal : public ModalLayer {
public:
  static constexpr int kCardW = 600, kMaxCardH = 520, kCardPad = 20, kCardRadius = 16;
  static constexpr int kRowH = 46, kSectionH = 30, kGap = 12;
  static constexpr float kTitlePx = 16, kNamePx = 13, kMetaPx = 11, kSectionPx = 11;

  MyGearModal(Backdrop backdrop, MyGear& gear);
  ~MyGearModal() override;

  std::function<void(const MyGear::Entry&)> onPick;
  std::function<void()> onBrowse;
  std::function<void()> onClose;

  bool keyPressed(const juce::KeyPress& key) override;

private:
  class Card;
  std::unique_ptr<Card> card_;
};

}  // namespace t3k::ui
