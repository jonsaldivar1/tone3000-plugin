// Fork (TK3J): the small label under a chain tile:
//   Gear: Amp Head
//   Product: Zuta GBG120
// Click it to edit both in place (Enter / click away saves, Esc cancels).
// Values come from ToneNotes (the player's own, else the tone's catalog
// gear and title) and are saved back there.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "model/ChainState.h"
#include "services/ToneNotes.h"
#include "widgets/TextField.h"

namespace t3k::ui {

class TileCaption : public juce::Component, public juce::SettableTooltipClient {
public:
  static constexpr int kTopGap = 10;
  static constexpr int kLineH = 16;
  static constexpr int kFieldH = 22;
  static constexpr int kFieldGap = 4;
  // Room reserved under a lane: the taller of the two modes.
  static constexpr int kHeight = 2 * kFieldH + kFieldGap;
  static constexpr float kTextPx = 11.5f;

  explicit TileCaption(ToneNotes& notes);
  ~TileCaption() override;

  void setTone(const ToneSummary& tone);
  const ToneSummary& tone() const { return tone_; }
  void refresh();  // re-read the stored labels
  bool editing() const { return editing_; }

  void paint(juce::Graphics& g) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent& e) override;
  void mouseEnter(const juce::MouseEvent&) override { repaint(); }
  void mouseExit(const juce::MouseEvent&) override { repaint(); }

private:
  void beginEdit();
  void endEdit(bool save);
  void blurred();

  ToneNotes& notes_;
  ToneSummary tone_;
  ToneNotes::Info info_;
  bool editing_ = false;
  TextField gear_, product_;
};

}  // namespace t3k::ui
