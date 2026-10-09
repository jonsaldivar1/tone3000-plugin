// Fork (TK3J): the scene and footswitch palette. Each letter keeps one
// color everywhere it shows (scene bar, tile outline and badge, MIDI page),
// so "orange E" means the same switch on screen and on the floor.
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

// Footswitches E-H; transparent for anything else.
inline juce::Colour footswitch(const juce::String& letter) {
  if (letter == "E") return juce::Colour(0xffff8a3d);
  if (letter == "F") return juce::Colour(0xff3fd0c0);
  if (letter == "G") return juce::Colour(0xff4ade80);
  if (letter == "H") return juce::Colour(0xfff472b6);
  return juce::Colours::transparentBlack;
}

inline const char* const kFootswitches[] = {"E", "F", "G", "H"};

}  // namespace t3k::ui::fork_colors
