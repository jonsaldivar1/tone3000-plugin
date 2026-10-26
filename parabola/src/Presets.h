// My Gear and presets.
//
// Library: every capture (.nam) and IR (.wav/.aif/.aiff/.flac) under the
// tones folders: Documents\Parabola Bass Z and Documents\T3KJ Tones (the
// folder T3KJ's own My Gear uses), so captures you keep for T3KJ show up
// without a second copy. Captures never ship with Parabola (their licenses).
//
// Presets: every parameter plus each band's capture and the IR, by file
// name and last known path. Loading looks for the path first, then for a
// file of the same name anywhere in the library, so a preset still works
// after you move your files around, and the starter presets work on any
// computer that has the same captures downloaded. User presets live in
// %APPDATA%\Parabola Bass Z\presets.json.
#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <vector>

namespace parabola {

class ParabolaProcessor;

class Library {
public:
  struct Entry {
    juce::File file;
    juce::String name;  // file name without extension
    juce::String pack;  // the folder it's in, relative to its root
    bool isNam = true;
  };

  static std::vector<juce::File> roots();  // the tones folders that exist (built-in + added)
  // Folders you add in My Gear (FOLDERS > Add a folder), kept per computer in
  // %APPDATA%\Parabola Bass Z\settings.json.
  static std::vector<juce::File> addedRoots();
  static void addRoot(const juce::File& folder);
  static void removeRoot(const juce::File& folder);
  // The folder you last loaded a capture (nam) or an IR from, by any route
  // (LOAD, drag and drop, My Gear); every band's LOAD opens there.
  // Kept per computer in settings.json, not in presets or the DAW project.
  static juce::File lastFolder(bool nam);  // invalid if none yet
  static void rememberFolder(const juce::File& loadedFile);
  static juce::File parabolaRoot();         // Documents\Parabola Bass Z (may not exist yet)
  static std::vector<Entry> scan();        // everything, sorted by pack then name
  // The first file named `fileName` (case-insensitive) in the library.
  static juce::File find(const juce::String& fileName);
  static bool isNamFile(const juce::File& f) { return f.hasFileExtension("nam"); }
  static bool isIrFile(const juce::File& f) { return f.hasFileExtension("wav;aif;aiff;flac"); }
};

struct Preset {
  juce::String name;
  bool factory = false;
  juce::NamedValueSet params;                    // only the ones that differ from default (factory) or all (user)
  std::array<juce::String, 3> nam, namPath;      // file names ("" = none) and last known paths
  juce::String ir, irPath;
};

class PresetStore {
public:
  PresetStore();
  const std::vector<Preset>& all() const { return presets_; }  // factory first, then yours
  const Preset* byName(const juce::String& name) const;
  int indexOf(const juce::String& name) const;

  // Captures the processor's current settings and files as a user preset
  // (replacing one of the same name) and saves the file.
  void save(const juce::String& name, const ParabolaProcessor& processor);
  bool remove(const juce::String& name);  // user presets only

  // Applies a preset. Returns the files it couldn't find ("" if none).
  static juce::String apply(const Preset& preset, ParabolaProcessor& processor);
  static std::vector<Preset> factory();
  static juce::File userFile();

private:
  void load();
  void write() const;
  std::vector<Preset> presets_;
};

}  // namespace parabola
