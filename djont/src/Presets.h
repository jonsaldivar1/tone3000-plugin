// dJONt presets: every setting and every scene, under a name you pick.
//
// Yours come first, in the order you put them in, then the starters. The
// list order is the MIDI Program Change map: PC 0 loads the first preset,
// PC 1 the second, and so on, so keeping the same order in dJONt and T3KJ
// lets one Quad Cortex preset change switch both. Kept per machine in
//   <user app data>/dJONt/presets.xml
// (Windows: %APPDATA%\dJONt\presets.xml), shared by every dJONt instance,
// standalone and plugin. Starters can't be overwritten, renamed or deleted
// (save under a new name instead).
//
// A preset is a <Preset name=".."> tree with <Params> (<P id v/> per
// parameter, real units) and the <Scenes> tree (DjontProcessor::sceneTree).
// Parameters a preset doesn't list load at their default, so a starter only
// names what it changes.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <vector>

namespace djont {

struct Preset {
  juce::String name;
  bool factory = false;
  juce::ValueTree data;  // <Preset>
};

class PresetStore {
public:
  // `defaultOf` gives a parameter's default (real units); starters are built
  // on top of it.
  explicit PresetStore(std::function<float(const juce::String&)> defaultOf, juce::File file = defaultFile());
  static juce::File defaultFile();

  void reload();  // picks up presets saved by another instance
  const std::vector<Preset>& all() const { return all_; }  // yours, then the starters
  const Preset* find(const juce::String& name) const;
  int indexOf(const juce::String& name) const;  // -1 if missing
  int userCount() const { return static_cast<int>(user_.size()); }
  bool isFactory(const juce::String& name) const;

  // Adds a preset of yours, or replaces the one with that name in place.
  // False for an empty name or a starter's name.
  bool put(const juce::String& name, const juce::ValueTree& data);
  bool rename(const juce::String& from, const juce::String& to);
  bool remove(const juce::String& name);
  bool move(const juce::String& name, int delta);  // within your presets

private:
  void rebuild();
  void write() const;
  std::vector<Preset> factory() const;

  std::function<float(const juce::String&)> defaultOf_;
  juce::File file_;
  std::vector<Preset> user_, all_;
};

}  // namespace djont
