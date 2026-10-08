// Fork (TK3J): "My Gear", the player's own local tones, offered before the
// TONE3000 search when a slot is filled. Three lists, all on disk paths:
//   - Library: every folder under the chosen tones folder (up to kMaxDepth
//     levels down) that directly holds .nam or IR .wav files, plus loose
//     files at its top. A folder loads as one tone with all its models, the
//     same as the tile menu's Load Folder.
//   - Recent: what My Gear last loaded, newest first.
//   - Pinned: favourites, in the order they were pinned.
// Everything lives in UiPrefs (per machine, shared by every host), so the
// lists follow the player across the standalone and every DAW.
#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <memory>
#include <vector>

#include "UiPrefs.h"

namespace juce {
class FileChooser;
}

namespace t3k::ui {

class MyGear {
public:
  static constexpr int kMaxDepth = 3;
  static constexpr int kMaxRecent = 12;
  static constexpr int kMaxLibrary = 400;

  // Pref keys (fork namespace, so stock TONE3000 never reads them).
  static constexpr const char* kRoot = "t3k.fork.myGearRoot";
  static constexpr const char* kRecent = "t3k.fork.myGearRecent";
  static constexpr const char* kPinned = "t3k.fork.myGearPinned";
  static constexpr const char* kOpenFirst = "t3k.fork.myGearOpenFirst";

  struct Entry {
    juce::File file;
    juce::String name;
    // "Amps / Zuta GBG120": the folders between the tones folder and this
    // entry, so packs with the same name stay distinguishable.
    juce::String group;
    bool isFolder = false;
    int namCount = 0;
    int irCount = 0;
    // "8 NAM", "1 IR", "6 NAM · 2 IR", "NAM file".
    juce::String summary() const;
  };

  explicit MyGear(UiPrefs& prefs);
  ~MyGear();

  // Open My Gear (instead of going straight to the search) on + and swap.
  bool openFirst() const { return prefs_.getBool(kOpenFirst, true); }

  // The tones folder. Unset: Documents/TK3J Tones when that folder exists.
  juce::File root() const;
  void setRoot(const juce::File& folder);
  // Async folder picker; `done` runs on the message thread when a folder
  // was chosen.
  void chooseRoot(std::function<void()> done);

  std::vector<Entry> library() const;
  std::vector<Entry> recent() const;
  std::vector<Entry> pinned() const;

  bool isPinned(const juce::File& file) const;
  void setPinned(const juce::File& file, bool pinned);
  void noteUsed(const juce::File& file);

  // What a path is as a My Gear row; isLoadable() is false for anything
  // that is neither a tone file nor a folder holding one.
  static Entry describe(const juce::File& file, const juce::File& root = {});
  static bool isLoadable(const Entry& entry) { return entry.namCount + entry.irCount > 0; }
  static bool isToneFile(const juce::File& file);

private:
  juce::StringArray paths(const char* key) const;
  void setPaths(const char* key, const juce::StringArray& paths);
  std::vector<Entry> entriesFor(const juce::StringArray& paths) const;

  UiPrefs& prefs_;
  std::unique_ptr<juce::FileChooser> chooser_;
};

}  // namespace t3k::ui
