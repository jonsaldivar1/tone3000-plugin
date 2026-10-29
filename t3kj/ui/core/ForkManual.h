// Fork (T3KJ): the account menu's Manual. Opens the PDF the installer puts in
// <Program Files>\T3KJ\docs (or next to the standalone app), in whatever the
// computer opens PDFs with; without a local copy, the one on the T3KJ
// download page.
#pragma once

#include <juce_core/juce_core.h>

namespace t3k::ui::fork_manual {

inline constexpr const char* kFileName = "T3KJ-Manual.pdf";
inline constexpr const char* kUrl =
    "https://github.com/jonsaldivar1/tone3000-plugin/releases/download/t3kj-latest/T3KJ-Manual.pdf";

inline juce::File localCopy() {
  const juce::File candidates[] = {
      juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory).getChildFile("T3KJ").getChildFile("docs").getChildFile(kFileName),
      juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("docs").getChildFile(kFileName),
  };
  for (const auto& f : candidates)
    if (f.existsAsFile()) return f;
  return {};
}

inline void open() {
  if (const auto file = localCopy(); file.existsAsFile() && file.startAsProcess())
    return;
  juce::URL(kUrl).launchInDefaultBrowser();
}

}  // namespace t3k::ui::fork_manual
