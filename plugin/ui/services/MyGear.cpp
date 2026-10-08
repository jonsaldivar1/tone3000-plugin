#include "MyGear.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>

namespace t3k::ui {

namespace {
bool isNam(const juce::File& f) { return f.hasFileExtension("nam"); }
bool isIr(const juce::File& f) { return f.hasFileExtension("wav"); }

// Direct children only: a folder is a tone when the files are right in it.
void countTones(const juce::File& folder, int& nam, int& ir) {
  for (const auto& f : folder.findChildFiles(juce::File::findFiles, false, "*")) {
    if (isNam(f)) ++nam;
    else if (isIr(f)) ++ir;
  }
}

juce::String groupFor(const juce::File& file, const juce::File& root) {
  if (root == juce::File() || !file.isAChildOf(root)) return {};
  const auto parent = file.getParentDirectory();
  if (parent == root) return {};
  return parent.getRelativePathFrom(root).replaceCharacter('\\', '/').replace("/", " / ");
}
}  // namespace

juce::String MyGear::Entry::summary() const {
  if (!isFolder) return namCount > 0 ? "NAM file" : "IR file";
  juce::StringArray parts;
  if (namCount > 0) parts.add(juce::String(namCount) + " NAM");
  if (irCount > 0) parts.add(juce::String(irCount) + " IR");
  return parts.joinIntoString(juce::String::fromUTF8(" \xc2\xb7 "));
}

MyGear::MyGear(UiPrefs& prefs) : prefs_(prefs) {}
MyGear::~MyGear() = default;

bool MyGear::isToneFile(const juce::File& file) { return file.existsAsFile() && (isNam(file) || isIr(file)); }

juce::File MyGear::root() const {
  const auto saved = prefs_.get(kRoot);
  if (saved.isNotEmpty()) return juce::File(saved);
  const auto fallback =
      juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("TK3J Tones");
  return fallback.isDirectory() ? fallback : juce::File();
}

void MyGear::setRoot(const juce::File& folder) { prefs_.set(kRoot, folder.getFullPathName()); }

void MyGear::chooseRoot(std::function<void()> done) {
  const auto start = root() != juce::File() ? root()
                                            : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
  chooser_ = std::make_unique<juce::FileChooser>("Choose your tones folder", start);
  chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                        [this, done = std::move(done)](const juce::FileChooser& chooser) {
                          const auto picked = chooser.getResult();
                          if (picked.isDirectory()) {
                            setRoot(picked);
                            if (done) done();
                          }
                        });
}

MyGear::Entry MyGear::describe(const juce::File& file, const juce::File& root) {
  Entry e;
  e.file = file;
  e.name = file.isDirectory() ? file.getFileName() : file.getFileNameWithoutExtension();
  e.group = groupFor(file, root);
  e.isFolder = file.isDirectory();
  if (e.isFolder) countTones(file, e.namCount, e.irCount);
  else if (isNam(file) && file.existsAsFile()) e.namCount = 1;
  else if (isIr(file) && file.existsAsFile()) e.irCount = 1;
  return e;
}

std::vector<MyGear::Entry> MyGear::library() const {
  std::vector<Entry> out;
  const auto top = root();
  if (!top.isDirectory()) return out;

  // Breadth-first so shallow packs come before deep ones; each level sorted
  // by name. Hidden folders are skipped.
  std::vector<std::pair<juce::File, int>> queue{{top, 0}};
  for (size_t i = 0; i < queue.size() && static_cast<int>(out.size()) < kMaxLibrary; ++i) {
    const auto [folder, depth] = queue[i];
    auto children = folder.findChildFiles(juce::File::findFilesAndDirectories, false, "*");
    std::sort(children.begin(), children.end(), [](const juce::File& a, const juce::File& b) {
      return a.getFileName().compareNatural(b.getFileName()) < 0;
    });
    for (const auto& child : children) {
      if (child.isHidden() || child.getFileName().startsWithChar('.')) continue;
      if (child.isDirectory()) {
        auto e = describe(child, top);
        if (isLoadable(e)) out.push_back(std::move(e));
        if (depth + 1 < kMaxDepth) queue.push_back({child, depth + 1});
      } else if (depth == 0 && isToneFile(child)) {
        out.push_back(describe(child, top));
      }
      if (static_cast<int>(out.size()) >= kMaxLibrary) break;
    }
  }
  return out;
}

juce::StringArray MyGear::paths(const char* key) const {
  juce::StringArray result;
  const auto json = prefs_.getJson(key);
  if (const auto* list = json.getArray())
    for (const auto& v : *list) result.add(v.toString());
  return result;
}

void MyGear::setPaths(const char* key, const juce::StringArray& list) {
  juce::Array<juce::var> arr;
  for (const auto& p : list) arr.add(p);
  prefs_.setJson(key, juce::var(arr));
}

std::vector<MyGear::Entry> MyGear::entriesFor(const juce::StringArray& list) const {
  std::vector<Entry> out;
  const auto top = root();
  for (const auto& p : list) {
    const juce::File f(p);
    if (!f.exists()) continue;  // moved or deleted since: drop it from view
    auto e = describe(f, top);
    if (isLoadable(e)) out.push_back(std::move(e));
  }
  return out;
}

std::vector<MyGear::Entry> MyGear::recent() const { return entriesFor(paths(kRecent)); }
std::vector<MyGear::Entry> MyGear::pinned() const { return entriesFor(paths(kPinned)); }

bool MyGear::isPinned(const juce::File& file) const { return paths(kPinned).contains(file.getFullPathName()); }

void MyGear::setPinned(const juce::File& file, bool pin) {
  auto list = paths(kPinned);
  list.removeString(file.getFullPathName());
  if (pin) list.add(file.getFullPathName());
  setPaths(kPinned, list);
}

void MyGear::noteUsed(const juce::File& file) {
  auto list = paths(kRecent);
  list.removeString(file.getFullPathName());
  list.insert(0, file.getFullPathName());
  while (list.size() > kMaxRecent) list.remove(list.size() - 1);
  setPaths(kRecent, list);
}

}  // namespace t3k::ui
