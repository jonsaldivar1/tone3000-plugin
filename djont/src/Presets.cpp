#include "Presets.h"

#include <algorithm>

#include "DjontProcessor.h"

namespace djont {

namespace {

using Values = std::vector<std::pair<const char*, float>>;

juce::ValueTree paramsTree(const std::function<float(const juce::String&)>& defaultOf, const Values& overrides) {
  juce::ValueTree params("Params");
  for (const auto& id : DjontProcessor::sceneParamIds()) {
    float v = defaultOf(id);
    for (const auto& [oid, ov] : overrides)
      if (id == oid) v = ov;
    juce::ValueTree p("P");
    p.setProperty("id", id, nullptr);
    p.setProperty("v", v, nullptr);
    params.appendChild(p, nullptr);
  }
  return params;
}

// A starter: base settings, plus optional named scenes (each its own
// changes on top of the base). No scenes = four that start as copies.
juce::ValueTree starter(const std::function<float(const juce::String&)>& defaultOf, const juce::String& name,
                        const Values& base, const std::vector<std::pair<juce::String, Values>>& scenes = {}) {
  juce::ValueTree preset("Preset");
  preset.setProperty("name", name, nullptr);
  preset.appendChild(paramsTree(defaultOf, base), nullptr);
  juce::ValueTree tree("Scenes");
  tree.setProperty("count", scenes.empty() ? 4 : static_cast<int>(scenes.size()), nullptr);
  tree.setProperty("active", 0, nullptr);
  for (size_t i = 0; i < scenes.size(); ++i) {
    Values merged = base;
    for (const auto& kv : scenes[i].second) merged.push_back(kv);
    juce::ValueTree scene("Scene");
    scene.setProperty("index", static_cast<int>(i), nullptr);
    scene.setProperty("name", scenes[i].first, nullptr);
    scene.setProperty("stored", true, nullptr);
    for (const auto& p : paramsTree(defaultOf, merged)) scene.appendChild(p.createCopy(), nullptr);
    tree.appendChild(scene, nullptr);
  }
  preset.appendChild(tree, nullptr);
  return preset;
}

}  // namespace

PresetStore::PresetStore(std::function<float(const juce::String&)> defaultOf, juce::File file)
    : defaultOf_(std::move(defaultOf)), file_(std::move(file)) {
  reload();
}

juce::File PresetStore::defaultFile() {
  return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
      .getChildFile("dJONt")
      .getChildFile("presets.xml");
}

std::vector<Preset> PresetStore::factory() const {
  // Values in real units: percent 0-100, Hz, semitones; choices by index;
  // switches 0 / 1. Tight Mode 1 = FOCUS, Sustain Mode 1 = Sustainer,
  // Wham Mode 0 = Pedal, Wham Interval 0 = Custom, 2 = Whammy Oct Up.
  const Values tight{{"tightOn", 1}, {"tightMode", 1}, {"tightAmount", 55}, {"tightFreq", 220}};
  std::vector<Preset> out;
  const auto add = [&](const juce::ValueTree& data) {
    out.push_back({data.getProperty("name").toString(), true, data});
  };
  add(starter(defaultOf_, "Starter: Song Sections", tight,
              {{"Rhythm", {}},
               {"Lead", {{"sustainOn", 1}, {"sustainMode", 1}, {"sustainerAmount", 50}}},
               {"Lo-Fi Intro", {{"tightOn", 0}, {"lofiOn", 1}, {"lofiTone", 900}}},
               {"Octave Down", {{"whamOn", 1}, {"whamMode", 0}, {"whamInterval", 0}, {"whamHeel", -12}, {"whamToe", 0}}}}));
  add(starter(defaultOf_, "Starter: Tight Boost", {{"tightOn", 1}, {"tightMode", 1}, {"tightAmount", 60}, {"tightFreq", 250}}));
  Values whammy = tight;
  for (const auto& kv : Values{{"whamOn", 1}, {"whamMode", 0}, {"whamInterval", 2}}) whammy.push_back(kv);
  add(starter(defaultOf_, "Starter: Whammy Lead", whammy));
  add(starter(defaultOf_, "Starter: Ambient Hold",
              {{"tightOn", 0}, {"sustainOn", 1}, {"sustainMode", 0}, {"holdFade", 1.5f}, {"holdShimmer", 30},
               {"holdLowCut", 120}}));
  return out;
}

void PresetStore::reload() {
  user_.clear();
  if (auto xml = juce::XmlDocument::parse(file_)) {
    const auto tree = juce::ValueTree::fromXml(*xml);
    for (const auto& p : tree) {
      const auto name = p.getProperty("name").toString().trim();
      if (p.hasType("Preset") && name.isNotEmpty() && !isFactory(name)) user_.push_back({name, false, p.createCopy()});
    }
  }
  rebuild();
}

void PresetStore::rebuild() {
  all_ = user_;
  for (auto& f : factory()) all_.push_back(std::move(f));
}

void PresetStore::write() const {
  juce::ValueTree tree("DjontPresets");
  for (const auto& p : user_) tree.appendChild(p.data.createCopy(), nullptr);
  file_.getParentDirectory().createDirectory();
  if (auto xml = tree.createXml()) xml->writeTo(file_);
}

const Preset* PresetStore::find(const juce::String& name) const {
  for (const auto& p : all_)
    if (p.name == name) return &p;
  return nullptr;
}

int PresetStore::indexOf(const juce::String& name) const {
  for (size_t i = 0; i < all_.size(); ++i)
    if (all_[i].name == name) return static_cast<int>(i);
  return -1;
}

bool PresetStore::isFactory(const juce::String& name) const {
  return name.startsWith("Starter: ");
}

bool PresetStore::put(const juce::String& rawName, const juce::ValueTree& data) {
  const auto name = rawName.trim().substring(0, 60);
  if (name.isEmpty() || isFactory(name)) return false;
  auto copy = data.createCopy();
  copy.setProperty("name", name, nullptr);
  bool replaced = false;
  for (auto& p : user_)
    if (p.name == name) {
      p.data = copy;
      replaced = true;
    }
  if (!replaced) user_.push_back({name, false, copy});
  write();
  rebuild();
  return true;
}

bool PresetStore::rename(const juce::String& from, const juce::String& rawTo) {
  const auto to = rawTo.trim().substring(0, 60);
  if (to.isEmpty() || isFactory(to) || isFactory(from) || from == to) return false;
  for (const auto& p : user_)
    if (p.name == to) return false;  // taken
  for (auto& p : user_)
    if (p.name == from) {
      p.name = to;
      p.data.setProperty("name", to, nullptr);
      write();
      rebuild();
      return true;
    }
  return false;
}

bool PresetStore::remove(const juce::String& name) {
  const auto before = user_.size();
  user_.erase(std::remove_if(user_.begin(), user_.end(), [&](const Preset& p) { return p.name == name; }), user_.end());
  if (user_.size() == before) return false;
  write();
  rebuild();
  return true;
}

bool PresetStore::move(const juce::String& name, int delta) {
  for (size_t i = 0; i < user_.size(); ++i)
    if (user_[i].name == name) {
      const int to = static_cast<int>(i) + delta;
      if (to < 0 || to >= static_cast<int>(user_.size())) return false;
      std::swap(user_[i], user_[static_cast<size_t>(to)]);
      write();
      rebuild();
      return true;
    }
  return false;
}

}  // namespace djont
