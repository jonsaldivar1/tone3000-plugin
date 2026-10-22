#include "WhamPresets.h"

namespace djont {

namespace {

// mode 0 = Pedal, 1 = Auto; shape 0..3 = Sine, Triangle, Rise, Square.
struct Factory {
  const char* name;
  float mode, heel, toe, rate, shape, retrig;
};
constexpr Factory kFactory[] = {
    {"Octave Scoop", 1, 0, 12, 4.0f, 2, 1},   // pick, then scoop up an octave
    {"Dive Bomb", 1, 0, -12, 1.5f, 2, 1},     // pick, then drop an octave
    {"Siren", 1, 0, 12, 0.5f, 0, 0},          // slow up and down
    {"Octave Stabs", 1, 0, 12, 6.0f, 3, 0},   // jump to the octave and back
    {"Whole-Step Trill", 1, 0, 2, 8.0f, 3, 0},
    {"Seasick", 1, -1, 1, 0.8f, 0, 0},        // a slow detune wobble
    {"Pedal: Octave Up", 0, 0, 12, 1.0f, 0, 0},
    {"Pedal: Drop Tune", 0, 0, -5, 1.0f, 0, 0},
};

juce::NamedValueSet valuesOf(const Factory& f) {
  juce::NamedValueSet v;
  v.set("whamMode", f.mode);
  v.set("whamHeel", f.heel);
  v.set("whamToe", f.toe);
  v.set("whamRate", f.rate);
  v.set("whamShape", f.shape);
  v.set("whamRetrig", f.retrig);
  return v;
}

float realValue(juce::AudioProcessorValueTreeState& state, const juce::String& id) {
  return state.getRawParameterValue(id)->load();
}

}  // namespace

const juce::StringArray& WhamPresets::paramIds() {
  static const juce::StringArray ids{"whamMode", "whamHeel", "whamToe", "whamRate", "whamShape", "whamRetrig", "whamGlide"};
  return ids;
}

juce::File WhamPresets::defaultFile() {
  return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
      .getChildFile("dJONt")
      .getChildFile("wham-presets.json");
}

WhamPresets::WhamPresets(juce::File file) : file_(std::move(file)) { reload(); }

void WhamPresets::reload() {
  presets_.clear();
  for (const auto& f : kFactory) presets_.push_back({f.name, true, valuesOf(f)});
  const auto json = juce::JSON::parse(file_);
  if (const auto* list = json["presets"].getArray()) {
    for (const auto& item : *list) {
      Preset p;
      p.name = item["name"].toString().trim();
      if (p.name.isEmpty() || find(p.name) != nullptr) continue;
      for (const auto& id : paramIds())
        if (item.hasProperty(id)) p.values.set(id, static_cast<float>(static_cast<double>(item[juce::Identifier(id)])));
      presets_.push_back(std::move(p));
    }
  }
}

const WhamPresets::Preset* WhamPresets::find(const juce::String& name) const {
  for (const auto& p : presets_)
    if (p.name.equalsIgnoreCase(name)) return &p;
  return nullptr;
}

bool WhamPresets::save(const juce::String& rawName, juce::AudioProcessorValueTreeState& state) {
  const auto name = rawName.trim();
  if (name.isEmpty()) return false;
  if (const auto* existing = find(name); existing != nullptr && existing->factory) return false;
  Preset p{name, false, {}};
  for (const auto& id : paramIds()) p.values.set(id, realValue(state, id));
  bool replaced = false;
  for (auto& existing : presets_)
    if (existing.name.equalsIgnoreCase(name)) {
      existing = p;
      replaced = true;
    }
  if (!replaced) presets_.push_back(p);
  write();
  return true;
}

bool WhamPresets::remove(const juce::String& name) {
  const auto it = std::find_if(presets_.begin(), presets_.end(),
                               [&](const Preset& p) { return !p.factory && p.name.equalsIgnoreCase(name); });
  if (it == presets_.end()) return false;
  presets_.erase(it);
  write();
  return true;
}

void WhamPresets::apply(const Preset& preset, juce::AudioProcessorValueTreeState& state) {
  for (const auto& id : paramIds()) {
    if (!preset.values.contains(id)) continue;
    if (auto* param = state.getParameter(id)) {
      param->beginChangeGesture();
      param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(preset.values[id])));
      param->endChangeGesture();
    }
  }
}

juce::String WhamPresets::matching(juce::AudioProcessorValueTreeState& state) const {
  for (const auto& p : presets_) {
    bool same = true;
    for (const auto& id : paramIds())
      if (p.values.contains(id) && std::abs(static_cast<float>(p.values[id]) - realValue(state, id)) > 0.01f) {
        same = false;
        break;
      }
    if (same) return p.name;
  }
  return {};
}

void WhamPresets::write() const {
  juce::Array<juce::var> list;
  for (const auto& p : presets_) {
    if (p.factory) continue;
    auto* obj = new juce::DynamicObject();
    obj->setProperty("name", p.name);
    for (const auto& v : p.values) obj->setProperty(v.name, v.value);
    list.add(juce::var(obj));
  }
  auto* root = new juce::DynamicObject();
  root->setProperty("presets", list);
  file_.getParentDirectory().createDirectory();
  file_.replaceWithText(juce::JSON::toString(juce::var(root)));
}

}  // namespace djont
