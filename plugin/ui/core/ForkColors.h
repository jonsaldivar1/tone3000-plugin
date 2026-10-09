// Fork (TK3J): the scene and footswitch palette. Each scene letter and
// footswitch number keeps one color everywhere it shows (scene bar, tile
// outline and badge, picker), so "orange 1" means the same switch on screen
// and on the floor.
#pragma once

#include <juce_graphics/juce_graphics.h>

namespace t3k::ui::fork_colors {

// Scenes A-D.
inline juce::Colour scene(int index) {
  static const juce::Colour colours[] = {juce::Colour(0xffff5a4f), juce::Colour(0xff4c8dff),
                                         juce::Colour(0xffffd23f), juce::Colour(0xffb76bff)};
  return colours[juce::jlimit(0, 3, index)];
}

inline juce::String sceneLetter(int index) {
  return juce::String::charToString(static_cast<juce::juce_wchar>('A' + juce::jlimit(0, 3, index)));
}

// Footswitches 1-8; transparent for anything else.
inline juce::Colour footswitch(const juce::String& number) {
  static const juce::Colour colours[] = {
      juce::Colour(0xffff8a3d),  // 1 orange
      juce::Colour(0xff3fd0c0),  // 2 teal
      juce::Colour(0xff4ade80),  // 3 green
      juce::Colour(0xfff472b6),  // 4 pink
      juce::Colour(0xffa3e635),  // 5 lime
      juce::Colour(0xff38bdf8),  // 6 sky
      juce::Colour(0xfffb7185),  // 7 rose
      juce::Colour(0xffe5e7eb),  // 8 silver
  };
  if (number.length() != 1 || number[0] < '1' || number[0] > '8') return juce::Colours::transparentBlack;
  return colours[number[0] - '1'];
}

inline const char* const kFootswitches[] = {"1", "2", "3", "4", "5", "6", "7", "8"};

}  // namespace t3k::ui::fork_colors
