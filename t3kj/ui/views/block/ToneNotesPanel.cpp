#include "ToneNotesPanel.h"

#include "core/Fonts.h"
#include "core/Paint.h"
#include "core/Theme.h"

namespace t3k::ui {

namespace {
constexpr int kSaveDelayMs = 600;
}

// Label over a rounded, hairline-bordered editor (the presets bar's input
// look); `lines` > 1 makes it a multi-line box.
class ToneNotesPanel::Field : public juce::Component {
public:
  Field(const juce::String& label, const juce::String& placeholder, int lines) : label_(label), lines_(lines) {
    editor_.setMultiLine(lines > 1, true);
    editor_.setReturnKeyStartsNewLine(lines > 1);
    editor_.setScrollbarsShown(lines > 1);
    editor_.setPopupMenuEnabled(true);
    editor_.setBorder({});
    editor_.setIndents(0, 0);
    editor_.setFont(Fonts::sans(kTextPx));
    editor_.setTextToShowWhenEmpty(placeholder, theme::kGray);
    editor_.setTitle(label);
    editor_.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    editor_.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    editor_.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    editor_.setColour(juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);
    editor_.setColour(juce::TextEditor::textColourId, theme::kWhite);
    editor_.setColour(juce::TextEditor::highlightColourId, theme::kHighlight);
    editor_.setColour(juce::TextEditor::highlightedTextColourId, theme::kWhite);
    editor_.setColour(juce::CaretComponent::caretColourId, theme::kWhite);
    editor_.onTextChange = [this] {
      if (onChange) onChange();
    };
    editor_.onFocusLost = [this] {
      if (onBlur) onBlur();
    };
    setViewportIgnoreDragFlag(true);
    editor_.setViewportIgnoreDragFlag(true);
    addAndMakeVisible(editor_);
  }

  std::function<void()> onChange, onBlur;
  int lines() const { return lines_; }
  juce::String text() const { return editor_.getText(); }
  void setText(const juce::String& t) {
    if (t != editor_.getText()) editor_.setText(t, false);
  }

  void paint(juce::Graphics& g) override {
    paint::text(g, label_.toUpperCase(), {0, 0, getWidth(), kLabelH}, Fonts::sans(kLabelPx, true), theme::kSubtle);
    const auto box = boxBounds().toFloat();
    paint::fill(g, box, 8.0f, theme::kSurfaceRaised);
    paint::border(g, box, 8.0f, editor_.hasKeyboardFocus(true) ? theme::kGray : theme::kBorder);
  }
  void resized() override { editor_.setBounds(boxBounds().reduced(12, kPadY)); }
  void focusOfChildComponentChanged(FocusChangeType) override { repaint(); }

private:
  juce::Rectangle<int> boxBounds() const { return getLocalBounds().withTrimmedTop(kLabelH + kLabelGap); }

  juce::String label_;
  int lines_;
  juce::TextEditor editor_;
};

ToneNotesPanel::ToneNotesPanel(ToneNotes& notes) : notes_(notes) {
  gear_ = std::make_unique<Field>("Gear", "Amp, Pedal, Cab IR...", 1);
  product_ = std::make_unique<Field>("Product", "Zuta GBG120", 1);
  creator_ = std::make_unique<Field>("Creator", "Who captured it", 1);
  link_ = std::make_unique<Field>("Link", "Where it's from (pack page, Patreon...)", 1);
  description_ = std::make_unique<Field>("Description", "What the pack is: amp settings, mics, quirks", 3);
  notes_field_ = std::make_unique<Field>("My Notes", "Your own notes: what it's good for, knob settings...", 5);
  for (auto* f : {gear_.get(), product_.get(), creator_.get(), link_.get(), description_.get(), notes_field_.get()}) {
    f->onChange = [this] {
      dirty_ = true;
      startTimer(kSaveDelayMs);
    };
    f->onBlur = [this] {
      if (dirty_) save();
    };
    addChildComponent(*f);
  }
}

ToneNotesPanel::~ToneNotesPanel() {
  if (dirty_) save();
}

void ToneNotesPanel::setTone(const ToneSummary& tone) {
  const bool same = tone.id == tone_.id && tone.title == tone_.title && tone.sourcePath == tone_.sourcePath &&
                    tone.local == tone_.local;
  if (same && !visible_.empty()) return;
  if (dirty_) save();
  tone_ = tone;
  full_ = tone_.local;
  const auto stored = notes_.stored(tone_);
  gear_->setText(stored.gear.isNotEmpty() ? stored.gear : ToneNotes::defaultGear(tone_));
  product_->setText(stored.product.isNotEmpty() ? stored.product : ToneNotes::defaultProduct(tone_));
  creator_->setText(stored.creator);
  link_->setText(stored.link);
  description_->setText(stored.description);
  notes_field_->setText(stored.notes);
  dirty_ = false;
  rebuild();
}

void ToneNotesPanel::rebuild() {
  visible_ = {gear_.get(), product_.get()};
  if (full_) {
    visible_.push_back(creator_.get());
    visible_.push_back(link_.get());
    visible_.push_back(description_.get());
  }
  visible_.push_back(notes_field_.get());
  for (auto* f : {gear_.get(), product_.get(), creator_.get(), link_.get(), description_.get(), notes_field_.get()})
    f->setVisible(std::find(visible_.begin(), visible_.end(), f) != visible_.end());
  resized();
}

int ToneNotesPanel::fieldHeight(const Field& f) const {
  return kLabelH + kLabelGap + 2 * kPadY + f.lines() * kLineH;
}

// Pairs side by side (gear | product, creator | link), the long ones full width.
int ToneNotesPanel::heightFor(int) const {
  int h = fieldHeight(*gear_);
  if (full_) h += kRowGap + fieldHeight(*creator_) + kRowGap + fieldHeight(*description_);
  return h + kRowGap + fieldHeight(*notes_field_);
}

void ToneNotesPanel::resized() {
  const int w = getWidth();
  const int half = (w - kColGap) / 2;
  int y = 0;
  auto pair = [&](Field& a, Field& b) {
    const int h = fieldHeight(a);
    a.setBounds(0, y, half, h);
    b.setBounds(w - half, y, half, h);
    y += h + kRowGap;
  };
  auto full = [&](Field& f) {
    const int h = fieldHeight(f);
    f.setBounds(0, y, w, h);
    y += h + kRowGap;
  };
  pair(*gear_, *product_);
  if (full_) {
    pair(*creator_, *link_);
    full(*description_);
  }
  full(*notes_field_);
}

void ToneNotesPanel::paint(juce::Graphics&) {}

void ToneNotesPanel::timerCallback() {
  stopTimer();
  if (dirty_) save();
}

void ToneNotesPanel::save() {
  stopTimer();
  dirty_ = false;
  ToneNotes::Info info;
  // The defaults typed back (or cleared) store nothing, so the labels keep
  // following the tone's own data.
  const auto gear = gear_->text().trim();
  const auto product = product_->text().trim();
  info.gear = gear == ToneNotes::defaultGear(tone_) ? juce::String() : gear;
  info.product = product == ToneNotes::defaultProduct(tone_) ? juce::String() : product;
  if (full_) {
    info.creator = creator_->text().trim();
    info.link = link_->text().trim();
    info.description = description_->text().trim();
  } else {
    // Catalog tones don't show these; keep whatever is stored.
    const auto stored = notes_.stored(tone_);
    info.creator = stored.creator;
    info.link = stored.link;
    info.description = stored.description;
  }
  info.notes = notes_field_->text().trim();
  if (info == notes_.stored(tone_)) return;
  notes_.store(tone_, info);
}

}  // namespace t3k::ui
