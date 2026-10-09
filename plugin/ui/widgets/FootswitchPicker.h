// Fork (TK3J): the one-row footswitch picker a tile's "Footswitch..." menu
// row opens: E F G H in their colors (the current one filled) and None.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

#include "Popover.h"

namespace t3k::ui {

class FootswitchPicker : public Popover {
public:
  static constexpr int kButton = 40, kPad = 6, kGap = 4, kNoneW = 64;

  explicit FootswitchPicker(const juce::String& current);
  ~FootswitchPicker() override;

  // A letter ("E".."H") or "" for None.
  std::function<void(const juce::String& letter)> onPick;

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  class Choice;
  std::vector<std::unique_ptr<Choice>> choices_;
};

}  // namespace t3k::ui
