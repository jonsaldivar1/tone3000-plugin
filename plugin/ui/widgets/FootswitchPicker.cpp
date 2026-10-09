#include "FootswitchPicker.h"

#include "Clickable.h"
#include "core/Fonts.h"
#include "core/ForkColors.h"
#include "core/Paint.h"
#include "core/Theme.h"

namespace t3k::ui {

class FootswitchPicker::Choice : public Clickable {
public:
  Choice(juce::String letter, bool current) : Clickable(letter.isEmpty() ? "None" : letter), letter_(std::move(letter)), current_(current) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setTitle(letter_.isEmpty() ? juce::String("No footswitch") : "Footswitch " + letter_);
    setHelpText(letter_.isEmpty() ? juce::String("None: take this block off its footswitch.")
                                  : "Footswitch " + letter_ + ": this block turns on and off with " + letter_ +
                                        " (with any other block on it).");
  }
  const juce::String& letter() const { return letter_; }

  void paintButton(juce::Graphics& g, bool highlighted, bool) override {
    const auto box = getLocalBounds().toFloat().reduced(1.0f);
    if (letter_.isEmpty()) {
      if (highlighted || current_) paint::fill(g, box, 8.0f, theme::kHighlight);
      paint::text(g, "None", getLocalBounds(), Fonts::sans(13.0f), theme::kWhite, juce::Justification::centred);
      return;
    }
    const auto colour = fork_colors::footswitch(letter_);
    if (current_ || highlighted) paint::fill(g, box, 8.0f, current_ ? colour : colour.withAlpha(0.25f));
    paint::border(g, box, 8.0f, colour);
    paint::text(g, letter_, getLocalBounds(), Fonts::sans(15.0f, true), current_ ? juce::Colours::black : colour,
                juce::Justification::centred);
  }

private:
  juce::String letter_;
  bool current_;
};

FootswitchPicker::FootswitchPicker(const juce::String& current) {
  primaryOnly = true;
  for (const auto* letter : fork_colors::kFootswitches) choices_.push_back(std::make_unique<Choice>(letter, current == letter));
  choices_.push_back(std::make_unique<Choice>(juce::String(), current.isEmpty()));
  for (auto& c : choices_) {
    c->onClick = [this, letter = c->letter()] {
      if (onPick) onPick(letter);
      dismiss();
    };
    addAndMakeVisible(*c);
  }
  setSize(2 * kBorder + 2 * kPad + 4 * kButton + 4 * kGap + kNoneW, 2 * kBorder + 2 * kPad + kButton);
}

FootswitchPicker::~FootswitchPicker() = default;

void FootswitchPicker::paint(juce::Graphics& g) {
  const auto box = getLocalBounds().toFloat();
  paint::fill(g, box, theme::kPanelCorner, theme::kPanelBg);
  paint::border(g, box, theme::kPanelCorner, theme::kBorder);
}

void FootswitchPicker::resized() {
  auto area = contentBounds().reduced(kPad);
  for (size_t i = 0; i < choices_.size(); ++i) {
    const int w = i + 1 == choices_.size() ? kNoneW : kButton;
    choices_[i]->setBounds(area.removeFromLeft(w));
    area.removeFromLeft(kGap);
  }
}

}  // namespace t3k::ui
