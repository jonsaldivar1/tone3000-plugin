// Fork (TK3J): the header wordmark. "TOAN" spelled three ways at once, a
// word-search grid reading across, down and diagonally from one T, then
// "3000", in the stock logo's yellow / red / blue stacked print.
//
//   T O A N
//   O O
//   A   A      3000
//   N     N
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace t3k::ui::fork_logo {

// Natural size at `height` px tall (the width follows).
float widthFor(float height);
void draw(juce::Graphics& g, juce::Rectangle<float> box);

}  // namespace t3k::ui::fork_logo
