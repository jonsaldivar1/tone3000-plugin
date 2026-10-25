#include "Presets.h"

#include "ParabolaProcessor.h"

namespace parabola {

// ---------------------------------------------------------------- library
juce::File Library::parabolaRoot() {
  return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Parabola Bass Z");
}

namespace {
juce::File settingsFile() {
  return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
      .getChildFile("Parabola Bass Z")
      .getChildFile("settings.json");
}

void writeAdded(const std::vector<juce::File>& folders) {
  juce::Array<juce::var> list;
  for (const auto& f : folders) list.add(f.getFullPathName());
  auto* obj = new juce::DynamicObject();
  obj->setProperty("myGearFolders", list);
  settingsFile().getParentDirectory().createDirectory();
  settingsFile().replaceWithText(juce::JSON::toString(juce::var(obj)));
}
}  // namespace

std::vector<juce::File> Library::addedRoots() {
  std::vector<juce::File> out;
  const auto json = juce::JSON::parse(settingsFile());
  if (auto* list = json["myGearFolders"].getArray())
    for (const auto& v : *list)
      if (juce::File::isAbsolutePath(v.toString())) out.emplace_back(v.toString());
  return out;
}

void Library::addRoot(const juce::File& folder) {
  auto folders = addedRoots();
  for (const auto& f : folders)
    if (f == folder) return;
  folders.push_back(folder);
  writeAdded(folders);
}

void Library::removeRoot(const juce::File& folder) {
  auto folders = addedRoots();
  folders.erase(std::remove(folders.begin(), folders.end(), folder), folders.end());
  writeAdded(folders);
}

std::vector<juce::File> Library::roots() {
  std::vector<juce::File> out;
  const auto docs = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
  std::vector<juce::File> candidates{parabolaRoot(), docs.getChildFile("T3KJ Tones"), docs.getChildFile("TK3J Tones")};
  for (const auto& f : addedRoots()) candidates.push_back(f);
  for (const auto& f : candidates) {
    if (!f.isDirectory()) continue;
    // Skip a folder already covered by another one (nested or repeated).
    bool covered = false;
    for (const auto& o : out)
      if (f == o || f.isAChildOf(o)) covered = true;
    if (!covered) out.push_back(f);
  }
  return out;
}

std::vector<Library::Entry> Library::scan() {
  std::vector<Entry> out;
  for (const auto& root : roots()) {
    for (const auto& f : root.findChildFiles(juce::File::findFiles, true, "*.nam;*.wav;*.aif;*.aiff;*.flac")) {
      if (f.isHidden() || out.size() > 5000) continue;
      Entry e;
      e.file = f;
      e.name = f.getFileNameWithoutExtension();
      e.pack = f.getParentDirectory() == root ? root.getFileName()
                                              : f.getParentDirectory().getRelativePathFrom(root).replaceCharacter('\\', '/');
      e.isNam = isNamFile(f);
      out.push_back(std::move(e));
    }
  }
  std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) {
    if (a.isNam != b.isNam) return a.isNam;
    if (const int c = a.pack.compareNatural(b.pack); c != 0) return c < 0;
    return a.name.compareNatural(b.name) < 0;
  });
  return out;
}

juce::File Library::find(const juce::String& fileName) {
  if (fileName.isEmpty()) return {};
  for (const auto& root : roots())
    for (const auto& f : root.findChildFiles(juce::File::findFiles, true, "*"))
      if (f.getFileName().equalsIgnoreCase(fileName)) return f;
  return {};
}

// ---------------------------------------------------------------- presets
namespace {
juce::File resolve(const juce::String& fileName, const juce::String& path) {
  if (path.isNotEmpty() && juce::File::isAbsolutePath(path) && juce::File(path).existsAsFile()) return juce::File(path);
  return Library::find(fileName);
}

Preset make(const char* name, std::array<const char*, 3> nams, const char* ir,
            std::initializer_list<std::pair<const char*, float>> params) {
  Preset p;
  p.name = name;
  p.factory = true;
  for (size_t b = 0; b < 3; ++b) p.nam[b] = nams[b] != nullptr ? nams[b] : "";
  p.ir = ir != nullptr ? ir : "";
  for (const auto& [id, v] : params) p.params.set(id, v);
  return p;
}
}  // namespace

std::vector<Preset> PresetStore::factory() {
  return {
      make("Init (empty)", {nullptr, nullptr, nullptr}, nullptr, {}),
      make("Djent Low End", {"SVT CLEAN.nam", "B7K Djent.nam", "B7K Heavy.nam"}, "Ampeg 8x10 D6 AH.wav",
           {{"gate", -60.0f}, {"lowComp", 55.0f}, {"midDrive", 3.0f}, {"highTone", 9000.0f}, {"highLevel", -7.0f},
            {"geq1", -3.0f}, {"geq4", 2.0f}}),
      make("Modern Growl", {"SVT CLEAN PUSHED.nam", "B7K Hard Rock.nam", "B7K Distortion.nam"}, "Eden 4x10 D6 AH.wav",
           {{"xoverLow", 160.0f}, {"xoverHigh", 1400.0f}, {"midDrive", 2.0f}, {"midMid", 2.0f}, {"highLevel", -8.0f},
            {"geq2", -2.0f}, {"geq3", 2.0f}}),
      make("Fuzz Wall", {"DG210A Comp.nam", "Darkglass Fuzz - OMEGA.nam", "B7K Heavy.nam"}, "Ampeg 8x10 57 AH.wav",
           {{"lowComp", 30.0f}, {"midLevel", -3.0f}, {"highTone", 8000.0f}, {"highLevel", -9.0f}}),
      make("Alpha Omega Split", {"SVT CLEAN.nam", "Darkglass Distortion - ALPHA.nam", "Darkglass Fuzz - OMEGA.nam"},
           nullptr, {{"xoverHigh", 2000.0f}, {"midLevel", -1.0f}, {"highLevel", -8.0f}}),
      make("Vintage Round", {"SVT CLEAN.nam", "SVT PUSHED.nam", nullptr}, "1970 Bassman Cabinet CTS - RE20 Lower - Cap.wav",
           {{"xoverLow", 220.0f}, {"xoverHigh", 1800.0f}, {"highOn", 0.0f}, {"midTone", 3500.0f}, {"lowComp", 30.0f},
            {"geq5", -4.0f}}),
      make("Clean DI + Grit", {nullptr, "B7K Clean.nam", "B7K Djent.nam"}, "Eden 4x10 D6 AH.wav",
           {{"lowComp", 60.0f}, {"midLevel", -4.0f}, {"highLevel", -10.0f}}),
      make("Microtubes Clean", {"DG210A Clean.nam", "DG210A Comp.nam", "B7K Hard Rock.nam"}, "Ampeg 8x10 D6 AH.wav",
           {{"highLevel", -10.0f}}),
  };
}

juce::File PresetStore::userFile() {
  return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
      .getChildFile("Parabola Bass Z")
      .getChildFile("presets.json");
}

PresetStore::PresetStore() { load(); }

void PresetStore::load() {
  presets_ = factory();
  const auto json = juce::JSON::parse(userFile());
  if (auto* list = json.getArray()) {
    for (const auto& item : *list) {
      Preset p;
      p.name = item["name"].toString();
      if (p.name.isEmpty()) continue;
      if (auto* obj = item["params"].getDynamicObject())
        for (const auto& [id, v] : obj->getProperties()) p.params.set(id, v);
      if (auto* nams = item["nam"].getArray())
        for (size_t b = 0; b < 3 && static_cast<int>(b) < nams->size(); ++b) {
          p.nam[b] = (*nams)[static_cast<int>(b)]["file"].toString();
          p.namPath[b] = (*nams)[static_cast<int>(b)]["path"].toString();
        }
      p.ir = item["ir"]["file"].toString();
      p.irPath = item["ir"]["path"].toString();
      presets_.push_back(std::move(p));
    }
  }
}

void PresetStore::write() const {
  juce::Array<juce::var> list;
  for (const auto& p : presets_) {
    if (p.factory) continue;
    auto* obj = new juce::DynamicObject();
    obj->setProperty("name", p.name);
    auto* params = new juce::DynamicObject();
    for (const auto& nv : p.params) params->setProperty(nv.name, nv.value);
    obj->setProperty("params", juce::var(params));
    juce::Array<juce::var> nams;
    for (size_t b = 0; b < 3; ++b) {
      auto* n = new juce::DynamicObject();
      n->setProperty("file", p.nam[b]);
      n->setProperty("path", p.namPath[b]);
      nams.add(juce::var(n));
    }
    obj->setProperty("nam", nams);
    auto* ir = new juce::DynamicObject();
    ir->setProperty("file", p.ir);
    ir->setProperty("path", p.irPath);
    obj->setProperty("ir", juce::var(ir));
    list.add(juce::var(obj));
  }
  const auto file = userFile();
  file.getParentDirectory().createDirectory();
  file.replaceWithText(juce::JSON::toString(juce::var(list)));
}

const Preset* PresetStore::byName(const juce::String& name) const {
  const int i = indexOf(name);
  return i >= 0 ? &presets_[static_cast<size_t>(i)] : nullptr;
}

int PresetStore::indexOf(const juce::String& name) const {
  for (size_t i = 0; i < presets_.size(); ++i)
    if (presets_[i].name == name) return static_cast<int>(i);
  return -1;
}

void PresetStore::save(const juce::String& name, const ParabolaProcessor& processor) {
  Preset p;
  p.name = name;
  for (auto* param : processor.getParameters())
    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
      p.params.set(ranged->getParameterID(), ranged->convertFrom0to1(ranged->getValue()));
  for (int b = 0; b < kNumBands; ++b) {
    const auto& slot = processor.slot(b);
    if (!slot.isLoaded()) continue;
    const juce::File f(slot.info().path);
    p.nam[static_cast<size_t>(b)] = f.getFileName();
    p.namPath[static_cast<size_t>(b)] = f.getFullPathName();
  }
  if (processor.irLoaded()) {
    const juce::File f(processor.irPath());
    p.ir = f.getFileName();
    p.irPath = f.getFullPathName();
  }
  const int existing = indexOf(name);
  if (existing >= 0 && !presets_[static_cast<size_t>(existing)].factory) presets_[static_cast<size_t>(existing)] = p;
  else presets_.push_back(p);
  write();
}

bool PresetStore::remove(const juce::String& name) {
  const int i = indexOf(name);
  if (i < 0 || presets_[static_cast<size_t>(i)].factory) return false;
  presets_.erase(presets_.begin() + i);
  write();
  return true;
}

juce::String PresetStore::apply(const Preset& preset, ParabolaProcessor& processor) {
  for (auto* param : processor.getParameters()) {
    auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param);
    if (ranged == nullptr) continue;
    const auto id = ranged->getParameterID();
    const float value = preset.params.contains(id) ? static_cast<float>(preset.params[id])
                                                   : ranged->convertFrom0to1(ranged->getDefaultValue());
    const float normalised = ranged->convertTo0to1(value);
    if (!juce::approximatelyEqual(ranged->getValue(), normalised)) ranged->setValueNotifyingHost(normalised);
  }
  juce::StringArray missing;
  for (int b = 0; b < kNumBands; ++b) {
    const auto& name = preset.nam[static_cast<size_t>(b)];
    if (name.isEmpty()) {
      processor.clearNam(b);
      continue;
    }
    const auto file = resolve(name, preset.namPath[static_cast<size_t>(b)]);
    if (file.existsAsFile()) {
      if (juce::File(processor.slot(b).info().path) != file) processor.loadNam(b, file);
    } else {
      processor.clearNam(b);
      missing.add(name);
    }
  }
  if (preset.ir.isEmpty()) {
    processor.clearIr();
  } else if (const auto file = resolve(preset.ir, preset.irPath); file.existsAsFile()) {
    if (juce::File(processor.irPath()) != file) processor.loadIr(file);
  } else {
    processor.clearIr();
    missing.add(preset.ir);
  }
  processor.params.state.setProperty("preset", preset.name, nullptr);
  return missing.joinIntoString(", ");
}

}  // namespace parabola
