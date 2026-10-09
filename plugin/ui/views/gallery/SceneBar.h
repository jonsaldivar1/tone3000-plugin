// Fork (TK3J): the scene bar under the chain. Left: scene chips A-D (click
// to switch, double-click to rename, hover to preview which blocks the scene
// changes). Right: one chip per footswitch that has blocks on it (click =
// the same as stepping on that switch). Everything lives in the chain state
// (ProcessorScenes.cpp), so the QC driving TK3J over MIDI lights the same
// chips.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

#include "services/Services.h"
#include "widgets/TextField.h"

namespace t3k::ui {

class SceneBar : public juce::Component, private ChainStore::Listener {
public:
  static constexpr int kHeight = 56;
  static constexpr int kChipH = 36;
  static constexpr int kGap = 8;
  static constexpr float kTextPx = 13.0f;

  explicit SceneBar(Services& services);
  ~SceneBar() override;

  // Hovering a scene that isn't active: its index; leaving: -1.
  std::function<void(int scene)> onPreview;

  void paint(juce::Graphics& g) override;
  void resized() override;

  static juce::String sceneName(const ChainState& state, int index);

private:
  class SceneChip;
  class SwitchChip;

  void chainChanged(const ChainState& state) override;
  void rebuildSwitches();
  void beginRename(int index);
  void endRename(bool save);

  Services& services_;
  std::vector<std::unique_ptr<SceneChip>> scenes_;
  std::vector<std::unique_ptr<SwitchChip>> switches_;
  juce::StringArray switchKey_;  // what the switch chips were built from
  TextField rename_;
  int renaming_ = -1;
};

}  // namespace t3k::ui
