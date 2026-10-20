// Fork (TK3J): skins. A skin re-tints the theme tokens (core/Theme.h: the
// background, surfaces, grays, borders, the accent on knob arcs and armed
// buttons, the connector wire), re-shapes the chain tiles' corners, redraws
// the header logo in its own face and colours, and paints its own
// background art behind the chain. Function colours never change: the scene
// letters A-D and footswitches 1-8 (ForkColors.h), the tuner, meters, the
// EQ curve and clip LEDs read the same in every skin.
//
//   Stock   the TONE3000 look (black, yellow arcs, white wire).
//   Zenith  deep indigo, gold linework, sacred geometry + a starfield,
//           square-cornered tiles, a wide Michroma logo.
//   Tidal   dark teal, a 7-against-4 polyrhythm diagram over layered
//           waveforms, soft round tiles, a heavy Unbounded logo.
//
// All art is original geometry drawn in code: no band names, logos or
// album art. The skin lives in UiPrefs (kPrefKey) and applies machine-wide.
#pragma once

#include <juce_graphics/juce_graphics.h>

namespace t3k::ui::skin {

enum class Id { stock, zenith, tidal };

inline constexpr const char* kPrefKey = "t3k.fork.skin";

Id fromString(const juce::String& name);  // unknown → stock
juce::String toString(Id id);
juce::String displayName(Id id);

Id current();
// Message thread, before the UI builds and on a change (callers repaint).
void apply(Id id);

// The skin's background art over `area` (the whole editor): a no-op for
// Stock. Painted under every view; opaque views (header, plate, panels)
// cover it, the chain area shows it.
void paintBackdrop(juce::Graphics& g, juce::Rectangle<float> area);

// Header logo.
struct Logo {
  juce::Colour front, mid, back;  // stacked print, front to back
  bool italic = true;
};
Logo logo();
// "3000" and the TOAN grid letters in the skin's logo face.
juce::Font logoNumberFont(float px);
juce::Font logoGridFont(float px);

// Whether paintBackdrop draws anything (the chain's edge fades stand aside
// over art: fading to a flat colour would cut a band out of it).
bool hasBackdrop();

// Zenith marks each tile's corners with fine registration ticks.
bool tileCornerTicks();

}  // namespace t3k::ui::skin
