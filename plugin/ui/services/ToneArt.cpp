#include "ToneArt.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace t3k::ui {

namespace {
const char* const kExtensions[] = {".jpg", ".jpeg", ".png"};

// A folder's own art names, in lookup order.
const char* const kFolderStems[] = {"cover", "folder"};

juce::Array<juce::File> ownCandidates(const juce::File& source) {
  juce::Array<juce::File> out;
  if (source.isDirectory()) {
    for (const auto* stem : kFolderStems)
      for (const auto* ext : kExtensions) out.add(source.getChildFile(juce::String(stem) + ext));
  } else {
    for (const auto* ext : kExtensions) out.add(source.withFileExtension(ext));
  }
  return out;
}

juce::File firstExisting(const juce::Array<juce::File>& files) {
  for (const auto& f : files)
    if (f.existsAsFile()) return f;
  return {};
}
}  // namespace

ToneArt::ToneArt() = default;
ToneArt::~ToneArt() = default;

juce::File ToneArt::find(const juce::File& source) {
  if (source == juce::File() || !source.exists()) return {};
  if (auto own = firstExisting(ownCandidates(source)); own != juce::File()) return own;
  // A single file inherits its folder's cover (a pack folder's art).
  if (!source.isDirectory()) return firstExisting(ownCandidates(source.getParentDirectory()));
  return {};
}

juce::String ToneArt::urlFor(const juce::File& source) {
  const auto art = find(source);
  if (art == juce::File()) return {};
  return juce::URL(art).toString(false) + "#" + juce::String(art.getLastModificationTime().toMilliseconds());
}

juce::String ToneArt::urlForTone(bool local, const juce::String& sourcePath, const juce::String& image) {
  if (!local || sourcePath.isEmpty() || !juce::File::isAbsolutePath(sourcePath)) return image;
  const auto art = urlFor(juce::File(sourcePath));
  return art.isNotEmpty() ? art : image;
}

juce::File ToneArt::fileFromUrl(const juce::String& url) {
  if (!url.startsWithIgnoreCase("file:")) return {};
  return juce::URL(url.upToFirstOccurrenceOf("#", false, false)).getLocalFile();
}

juce::File ToneArt::ownArtStem(const juce::File& source) {
  return source.isDirectory() ? source.getChildFile("cover") : source.withFileExtension("");
}

bool ToneArt::hasOwnArt(const juce::File& source) {
  return source != juce::File() && firstExisting(ownCandidates(source)) != juce::File();
}

bool ToneArt::set(const juce::File& source, const juce::File& image) {
  if (source == juce::File() || !source.exists() || !image.existsAsFile()) return false;
  auto ext = image.getFileExtension().toLowerCase();
  if (ext == ".jpeg") ext = ".jpg";
  if (ext != ".jpg" && ext != ".png") return false;
  const auto target = ownArtStem(source).withFileExtension(ext);
  if (image == target) return true;

  // Write beside the target first, so a failed copy leaves the old art.
  const auto temp = target.getSiblingFile(".tk3j-art-" + juce::String(juce::Random::getSystemRandom().nextInt64()) + ext);
  if (!image.copyFileTo(temp)) return false;
  for (const auto& old : ownCandidates(source))
    if (old.existsAsFile() && old != target) old.moveToTrash();
  const bool ok = temp.moveFileTo(target);
  if (!ok) temp.deleteFile();
  else target.setLastModificationTime(juce::Time::getCurrentTime());  // new cache key
  sendChangeMessage();
  return ok;
}

bool ToneArt::clear(const juce::File& source) {
  bool any = false;
  for (const auto& old : ownCandidates(source))
    if (old.existsAsFile()) any = old.moveToTrash() || any;
  sendChangeMessage();
  return any;
}

void ToneArt::choose(const juce::File& source, std::function<void(bool ok)> done) {
  if (chooser_ != nullptr) return;
  const auto start = source.isDirectory() ? source : source.getParentDirectory();
  chooser_ = std::make_unique<juce::FileChooser>("Choose art for this tone", start, "*.jpg;*.jpeg;*.png");
  chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                        [self = juce::WeakReference<ToneArt>(this), source,
                         done = std::move(done)](const juce::FileChooser& chooser) {
                          if (self == nullptr) return;
                          const auto picked = chooser.getResult();
                          // Release the chooser once its callback unwinds (it is the caller).
                          juce::MessageManager::callAsync([self] {
                            if (self != nullptr) self->chooser_.reset();
                          });
                          if (picked == juce::File()) return;
                          const bool ok = self->set(source, picked);
                          if (done) done(ok);
                        });
}

}  // namespace t3k::ui
