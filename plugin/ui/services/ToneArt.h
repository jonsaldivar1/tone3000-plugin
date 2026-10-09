// Fork (TK3J): custom artwork for local tones, kept on disk next to the
// tone itself so it follows the files (and every preset that loads them):
//   - a folder tone: cover.jpg / cover.png (or folder.jpg / .png) inside it
//   - a single file: an image with the same name next to it
//     ("Zuta Crunch.nam" + "Zuta Crunch.jpg"), else its folder's cover
// The tile's right-click "Set Art..." copies a picked image into place as
// cover.<ext> (folder) or <name>.<ext> (file); "Clear Art" moves it to the
// trash. Art is addressed as a file: URL whose #fragment carries the
// image's modification time, so replacing the picture misses the image
// cache instead of showing the old one.
#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <functional>
#include <memory>

namespace juce {
class FileChooser;
}

namespace t3k::ui {

// Broadcasts after every set/clear (from the message thread) so every
// tile showing a tone from that source re-resolves its art.
class ToneArt : public juce::ChangeBroadcaster {
public:
  ToneArt();
  ~ToneArt();

  // The image that is this source's art, or a null File.
  static juce::File find(const juce::File& source);
  // "file:///.../cover.jpg#<mtime>", or empty when there is no art.
  static juce::String urlFor(const juce::File& source);
  // What a tone image shows: a local tone's custom art when it has a
  // source on disk, else the tone's own (catalog) image.
  static juce::String urlForTone(bool local, const juce::String& sourcePath, const juce::String& image);
  // The art file (minus its fragment) a urlFor() URL names, or a null File
  // for anything that isn't one.
  static juce::File fileFromUrl(const juce::String& url);

  // Where Set Art writes for this source (without the extension), and
  // whether the source's own art (not an inherited folder cover) exists.
  static juce::File ownArtStem(const juce::File& source);
  static bool hasOwnArt(const juce::File& source);

  // Copy `image` into place as the source's art (replacing any previous
  // own art). False when the copy failed.
  bool set(const juce::File& source, const juce::File& image);
  // Move the source's own art to the trash.
  bool clear(const juce::File& source);

  // Pick an image and set it; `done(ok)` runs on the message thread after
  // a pick (not when the dialog is dismissed).
  void choose(const juce::File& source, std::function<void(bool ok)> done);

private:
  std::unique_ptr<juce::FileChooser> chooser_;
  JUCE_DECLARE_WEAK_REFERENCEABLE(ToneArt)
};

}  // namespace t3k::ui
