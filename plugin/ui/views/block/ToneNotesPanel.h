// Fork (TK3J): the player's own info about a tone, in the block card's info
// view (ToneNotes holds it). Local tones get the full card: gear, product,
// creator, link, description and notes; TONE3000 tones already carry their
// creator and description, so just gear, product and notes. Edits save on
// their own a moment after typing stops (and when the panel goes away).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "model/ChainState.h"
#include "services/ToneNotes.h"

namespace t3k::ui {

class ToneNotesPanel : public juce::Component, private juce::Timer {
public:
  explicit ToneNotesPanel(ToneNotes& notes);
  ~ToneNotesPanel() override;

  void setTone(const ToneSummary& tone);
  int heightFor(int width) const;

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  class Field;
  static constexpr int kLabelH = 18, kLabelGap = 4, kRowGap = 14, kColGap = 16;
  static constexpr int kLineH = 18, kPadY = 8;
  static constexpr float kTextPx = 13.0f, kLabelPx = 11.0f;

  void timerCallback() override;
  void save();
  void rebuild();
  int fieldHeight(const Field& f) const;

  ToneNotes& notes_;
  ToneSummary tone_;
  bool full_ = false;
  bool dirty_ = false;
  std::unique_ptr<Field> gear_, product_, creator_, link_, description_, notes_field_;
  std::vector<Field*> visible_;
};

}  // namespace t3k::ui
