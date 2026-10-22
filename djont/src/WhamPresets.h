// Named Wham settings: mode, heel, toe, rate, shape and retrig (not the
// pedal position, which is a performance, not a setting). A few factory
// ones, then the user's own, kept per machine in
//   <user app data>/dJONt/wham-presets.json
// (Windows: %APPDATA%\dJONt\wham-presets.json), shared by every dJONt
// instance, standalone and plugin. Saving under an existing name replaces
// it; factory presets can't be overwritten or deleted.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace djont {

class WhamPresets {
public:
  static const juce::StringArray& paramIds();  // what a preset holds

  struct Preset {
    juce::String name;
    bool factory = false;
    juce::NamedValueSet values;  // param id -> real value
  };

  // `file` defaults to the per-machine store.
  explicit WhamPresets(juce::File file = defaultFile());
  static juce::File defaultFile();

  const std::vector<Preset>& all() const { return presets_; }
  const Preset* find(const juce::String& name) const;

  void reload();
  // Saves the state's current Wham settings as `name`. False for a factory
  // name or an empty one.
  bool save(const juce::String& name, juce::AudioProcessorValueTreeState& state);
  bool remove(const juce::String& name);
  static void apply(const Preset& preset, juce::AudioProcessorValueTreeState& state);
  // The preset whose values match the state now, if any ("" otherwise).
  juce::String matching(juce::AudioProcessorValueTreeState& state) const;

private:
  void write() const;
  juce::File file_;
  std::vector<Preset> presets_;
};

}  // namespace djont
