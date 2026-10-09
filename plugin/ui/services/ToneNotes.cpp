#include "ToneNotes.h"

#include "core/Labels.h"

namespace t3k::ui {

namespace {
const char* const kFields[] = {"gear", "product", "creator", "link", "description", "notes"};

juce::String* field(ToneNotes::Info& info, const juce::String& name) {
  if (name == "gear") return &info.gear;
  if (name == "product") return &info.product;
  if (name == "creator") return &info.creator;
  if (name == "link") return &info.link;
  if (name == "description") return &info.description;
  if (name == "notes") return &info.notes;
  return nullptr;
}

juce::File sourceOf(const ToneSummary& tone) {
  if (!tone.local || tone.sourcePath.isEmpty() || !juce::File::isAbsolutePath(tone.sourcePath)) return {};
  const juce::File source(tone.sourcePath);
  return source.exists() ? source : juce::File();
}

juce::var readJson(const juce::File& file) {
  if (!file.existsAsFile()) return {};
  return juce::JSON::parse(file.loadFileAsString());
}
}  // namespace

ToneNotes::ToneNotes(UiPrefs& prefs) : prefs_(prefs) {}

ToneNotes::Info ToneNotes::fromVar(const juce::var& v) {
  Info info;
  if (!v.isObject()) return info;
  for (const auto* name : kFields)
    if (auto* f = field(info, name)) *f = v[name].toString().trim();
  return info;
}

juce::var ToneNotes::toVar(const Info& info) {
  auto* obj = new juce::DynamicObject();
  Info copy = info;
  for (const auto* name : kFields)
    if (auto* f = field(copy, name); f != nullptr && f->trim().isNotEmpty()) obj->setProperty(name, f->trim());
  return juce::var(obj);
}

juce::File ToneNotes::sidecarFor(const ToneSummary& tone) {
  const auto source = sourceOf(tone);
  if (source == juce::File()) return {};
  return source.isDirectory() ? source.getChildFile(kSidecarName)
                              : source.getSiblingFile(source.getFileNameWithoutExtension() + "." + kSidecarName);
}

juce::String ToneNotes::prefsId(const ToneSummary& tone) const {
  if (!tone.local && tone.id > 0) return "t3k:" + juce::String(tone.id);
  return "local:" + tone.title;
}

ToneNotes::Info ToneNotes::stored(const ToneSummary& tone) const {
  if (const auto sidecar = sidecarFor(tone); sidecar != juce::File()) {
    auto info = fromVar(readJson(sidecar));
    // A single file inherits its pack folder's details.
    if (!juce::File(tone.sourcePath).isDirectory()) {
      const auto pack = fromVar(readJson(juce::File(tone.sourcePath).getSiblingFile(kSidecarName)));
      if (info.creator.isEmpty()) info.creator = pack.creator;
      if (info.link.isEmpty()) info.link = pack.link;
      if (info.description.isEmpty()) info.description = pack.description;
    }
    return info;
  }
  return fromVar(prefs_.getJson(kPrefsKey)[juce::Identifier(prefsId(tone))]);
}

ToneNotes::Info ToneNotes::resolved(const ToneSummary& tone) const {
  auto info = stored(tone);
  if (info.gear.isEmpty()) info.gear = defaultGear(tone);
  if (info.product.isEmpty()) info.product = defaultProduct(tone);
  if (info.creator.isEmpty() && tone.user) info.creator = tone.user->username;
  return info;
}

void ToneNotes::store(const ToneSummary& tone, const Info& info) {
  if (const auto sidecar = sidecarFor(tone); sidecar != juce::File()) {
    // Keep keys other tools (or later TK3J versions) wrote.
    auto existing = readJson(sidecar);
    auto* obj = existing.isObject() ? existing.getDynamicObject() : new juce::DynamicObject();
    juce::var keep(obj);
    const auto next = toVar(info);
    for (const auto* name : kFields) {
      if (next.hasProperty(name)) obj->setProperty(name, next[name]);
      else obj->removeProperty(name);
    }
    if (obj->getProperties().isEmpty()) sidecar.deleteFile();
    else sidecar.replaceWithText(juce::JSON::toString(keep));
  } else {
    auto all = prefs_.getJson(kPrefsKey);
    auto* obj = all.isObject() ? all.getDynamicObject() : new juce::DynamicObject();
    juce::var keep(obj);
    const auto next = toVar(info);
    if (next.getDynamicObject()->getProperties().isEmpty()) obj->removeProperty(prefsId(tone));
    else obj->setProperty(prefsId(tone), next);
    prefs_.setJson(kPrefsKey, keep);
  }
  sendChangeMessage();
}

juce::String ToneNotes::defaultGear(const ToneSummary& tone) {
  if (tone.gear.isNotEmpty()) return labels::gear(tone.gear);
  return tone.format.equalsIgnoreCase("ir") ? juce::String("Cab IR") : juce::String();
}

juce::String ToneNotes::creatorImageUrl(const ToneSummary& tone) {
  const auto source = sourceOf(tone);
  if (source == juce::File()) return {};
  const auto folder = source.isDirectory() ? source : source.getParentDirectory();
  for (const auto* stem : {"creator", "avatar"})
    for (const auto* ext : {".jpg", ".jpeg", ".png"})
      if (const auto f = folder.getChildFile(juce::String(stem) + ext); f.existsAsFile())
        return juce::URL(f).toString(false) + "#" + juce::String(f.getLastModificationTime().toMilliseconds());
  return {};
}

}  // namespace t3k::ui
