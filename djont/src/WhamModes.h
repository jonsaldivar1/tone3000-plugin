// WHAM's INTERVAL list, after the classic whammy pedal's mode knob.
//   Custom   your own Heel / Toe (±24 st).
//   Whammy   heel = no shift, toe = the interval; the shifted signal
//            replaces the dry (MIX blends the dry back in).
//   Harmony  the dry note plus a shifted voice that moves from the heel
//            interval to the toe interval (MIX = the voice's level).
//   Detune   the dry plus a slightly detuned copy, for thickness; the pedal
//            does nothing here.
#pragma once

#include <juce_core/juce_core.h>

namespace djont {

struct WhamInterval {
  enum class Kind { custom, whammy, harmony, detune };
  const char* name;
  Kind kind;
  float heel, toe;  // semitones
};

inline const WhamInterval kWhamIntervals[] = {
    {"Custom", WhamInterval::Kind::custom, 0, 0},
    {"Whammy: 2 Oct Up", WhamInterval::Kind::whammy, 0, 24},
    {"Whammy: Oct Up", WhamInterval::Kind::whammy, 0, 12},
    {"Whammy: 5th Up", WhamInterval::Kind::whammy, 0, 7},
    {"Whammy: 4th Up", WhamInterval::Kind::whammy, 0, 5},
    {"Whammy: 2nd Down", WhamInterval::Kind::whammy, 0, -2},
    {"Whammy: 4th Down", WhamInterval::Kind::whammy, 0, -5},
    {"Whammy: 5th Down", WhamInterval::Kind::whammy, 0, -7},
    {"Whammy: Oct Down", WhamInterval::Kind::whammy, 0, -12},
    {"Whammy: 2 Oct Down", WhamInterval::Kind::whammy, 0, -24},
    {"Whammy: Dive Bomb", WhamInterval::Kind::whammy, 0, -36},
    {"Harmony: 2nd > 3rd Up", WhamInterval::Kind::harmony, 2, 4},
    {"Harmony: b3 > 3rd Up", WhamInterval::Kind::harmony, 3, 4},
    {"Harmony: 3rd > 4th Up", WhamInterval::Kind::harmony, 4, 5},
    {"Harmony: 4th > 5th Up", WhamInterval::Kind::harmony, 5, 7},
    {"Harmony: 5th > 6th Up", WhamInterval::Kind::harmony, 7, 9},
    {"Harmony: 5th > 7th Up", WhamInterval::Kind::harmony, 7, 11},
    {"Harmony: 4th > 3rd Down", WhamInterval::Kind::harmony, -5, -4},
    {"Harmony: 5th > 4th Down", WhamInterval::Kind::harmony, -7, -5},
    {"Harmony: Oct Down > Oct Up", WhamInterval::Kind::harmony, -12, 12},
    {"Detune: Shallow", WhamInterval::Kind::detune, 0.1f, 0.1f},
    {"Detune: Deep", WhamInterval::Kind::detune, 0.25f, 0.25f},
};
inline constexpr int kNumWhamIntervals = static_cast<int>(sizeof(kWhamIntervals) / sizeof(kWhamIntervals[0]));

inline juce::StringArray whamIntervalNames() {
  juce::StringArray names;
  for (const auto& i : kWhamIntervals) names.add(i.name);
  return names;
}

}  // namespace djont
