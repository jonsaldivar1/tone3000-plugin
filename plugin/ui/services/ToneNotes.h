// Fork (TK3J): the player's own info about a tone: a gear / product label
// (shown under its tile in the chain), creator, link, description and
// personal notes (the block card's info view).
//
// Where it lives:
//   - a local tone loaded from disk: a small sidecar file next to its files,
//     so it follows them (and every preset that loads them) like the art:
//       folder tone  -> <folder>/tk3j.json
//       single file  -> <name>.tk3j.json beside it; creator / link /
//                       description fall back to the folder's tk3j.json
//                       (a pack's details), notes and labels don't
//   - a TONE3000 tone: per-machine prefs keyed by its tone id
//   - a local tone with no recorded source (loaded before TK3J kept paths):
//     prefs keyed by its title
// Only what the player typed is stored; empty fields fall back to the tone's
// own data (catalog gear, title, creator) at display time.
#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include "UiPrefs.h"
#include "model/ChainState.h"

namespace t3k::ui {

class ToneNotes : public juce::ChangeBroadcaster {
public:
  static constexpr const char* kPrefsKey = "t3k.fork.toneNotes";
  static constexpr const char* kSidecarName = "tk3j.json";

  struct Info {
    juce::String gear;     // "Amp", "Pedal", "Cab IR", ...
    juce::String product;  // "Zuta GBG120"
    juce::String creator;
    juce::String link;
    juce::String description;
    juce::String notes;

    bool operator==(const Info&) const = default;
  };

  explicit ToneNotes(UiPrefs& prefs);

  // What the player stored (empty fields = not set).
  Info stored(const ToneSummary& tone) const;
  // Stored values with the tone's own data filled in where empty.
  Info resolved(const ToneSummary& tone) const;
  void store(const ToneSummary& tone, const Info& info);

  // Display defaults from the tone itself.
  static juce::String defaultGear(const ToneSummary& tone);
  static juce::String defaultProduct(const ToneSummary& tone) { return tone.title; }

  // Creator picture for a local tone: creator.jpg / .png or avatar.jpg /
  // .png in its folder, as a file: URL (empty when there is none).
  static juce::String creatorImageUrl(const ToneSummary& tone);

  // The sidecar a local tone reads and writes (null File when it has no
  // usable source on disk).
  static juce::File sidecarFor(const ToneSummary& tone);

private:
  static Info fromVar(const juce::var& v);
  static juce::var toVar(const Info& info);
  juce::String prefsId(const ToneSummary& tone) const;

  UiPrefs& prefs_;
};

}  // namespace t3k::ui
