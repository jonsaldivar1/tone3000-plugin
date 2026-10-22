#include "PitchDeckPanel.h"

#include "core/Fonts.h"
#include "core/Paint.h"
#include "core/Theme.h"

namespace t3k::ui {

namespace {

// Defaults: STEP on, tonality Off (top, normalised), the 30 ms buffer (the
// second of four detents).
constexpr bool kStepDefault = true;
constexpr float kTonalityDefault = 1.0f;
constexpr float kWindowDefault = 1.0f / 3.0f;
// Wham: off, pedal at the heel, heel 0 st (centre), 1 Hz (centre of the
// log map), Sine, no retrigger.
constexpr float kWhamHeelDefault = 0.5f;
constexpr float kWhamRateDefault = 0.5f;

// The gate deck's geometry (see GateDeckPanel.cpp).
constexpr int kPadTop = 14, kPadSide = 16, kPadBottom = 8;
constexpr int kSectionWidth = 64;
constexpr int kSectionGap = 18;
// The wham half: a hairline, the mode row, then the knob row.
constexpr int kDividerY = 88;
constexpr int kModeRowY = 98, kModeRowH = 20;
constexpr int kWhamKnobY = 128;

Knob::Options deckKnob(const char* label, const KnobScale& scale, float def, help::Key help,
                       std::optional<int> steps = {}) {
  Knob::Options o;
  o.label = label;
  o.size = theme::kKnobSizeSecondary;
  o.thumb = Knob::Thumb::secondary;
  o.scale = &scale;
  o.defaultValue = def;
  o.help = help;
  o.steps = steps;
  return o;
}

}  // namespace

PitchDeckPanel::PitchDeckPanel(Services& services)
    : step_(services.backend, "pitchStep", "STEP", help::Key::pitchStep),
      tonality_(services.backend, "pitchTonality",
                deckKnob("Tonality", scales::tonalityHz(), kTonalityDefault, help::Key::pitchTonality)),
      window_(services.backend, "pitchWindow",
              deckKnob("Buffer", scales::bufferMs(), kWindowDefault, help::Key::pitchWindow, 4)),
      whamMode_(services.backend, "whamMode"),
      modeSwitch_({{"OFF", help::Key::whamMode}, {"PEDAL", help::Key::whamMode}, {"AUTO", help::Key::whamMode}},
                  [] {
                    auto st = SegmentedText::armed();
                    st.accentOn = true;
                    st.minCellWidth = 52;
                    return st;
                  }()),
      heel_(services.backend, "whamHeel",
            deckKnob("Heel", scales::semitones(), kWhamHeelDefault, help::Key::whamHeel)),
      rate_(services.backend, "whamRate",
            deckKnob("Rate", scales::whamRateHz(), kWhamRateDefault, help::Key::whamRate)),
      shape_(services.backend, "whamShape", deckKnob("Shape", scales::whamShape(), 0.0f, help::Key::whamShape, 4)),
      retrig_(services.backend, "whamRetrig", "RETRIG", help::Key::whamRetrig),
      pedal_(services) {
  addAndMakeVisible(step_);
  for (auto* k : {&tonality_, &window_}) addAndMakeVisible(*k);

  addAndMakeVisible(modeSwitch_);
  modeSwitch_.onCellClick = [this](int index) { whamMode_.set(static_cast<float>(index) / 2.0f); };
  heelDim_.addAndMakeVisible(heel_);
  for (juce::Component* c : {static_cast<juce::Component*>(&rate_), static_cast<juce::Component*>(&shape_),
                             static_cast<juce::Component*>(&retrig_)})
    autoDim_.addAndMakeVisible(*c);
  addAndMakeVisible(heelDim_);
  addAndMakeVisible(autoDim_);
  addAndMakeVisible(pedal_);
  whamMode_.onChange = [this] { syncWham(); };
  syncWham();

  primaryOnly = true;          // right-click toggles the panel; don't dismiss on it
  dismissOnAnchorPress = true; // the anchor is the Pitch knob, a control
  setSize(kWidth, kHeight);
}

void PitchDeckPanel::resetDeck(Backend& backend) {
  ParamBinding(backend, "pitchStep").set(kStepDefault);
  ParamBinding(backend, "pitchTonality").set(kTonalityDefault);
  ParamBinding(backend, "pitchWindow").set(kWindowDefault);
  ParamBinding(backend, "whamMode").set(0.0f);
  ParamBinding(backend, "whamPedal").set(0.0f);
  ParamBinding(backend, "whamHeel").set(kWhamHeelDefault);
  ParamBinding(backend, "whamRate").set(kWhamRateDefault);
  ParamBinding(backend, "whamShape").set(0.0f);
  ParamBinding(backend, "whamRetrig").set(false);
}

void PitchDeckPanel::syncWham() {
  const int mode = juce::jlimit(0, 2, juce::roundToInt(whamMode_.normalised() * 2.0f));
  modeSwitch_.select(mode);
  heelDim_.setOff(mode == 0);
  autoDim_.setOff(mode != 2);
  pedal_.setMode(mode == 1 ? WhamPedal::Mode::pedal : mode == 2 ? WhamPedal::Mode::autoSweep : WhamPedal::Mode::off);
  pedal_.setAlpha(mode == 0 ? theme::kDisabledOpacity : 1.0f);
}

namespace {
juce::Rectangle<int> column(juce::Rectangle<int> content, int i) {
  return {content.getX() + i * (kSectionWidth + kSectionGap), content.getY(), kSectionWidth,
          content.getHeight()};
}
}  // namespace

void PitchDeckPanel::paint(juce::Graphics& g) {
  const auto box = getLocalBounds().toFloat();
  paint::fill(g, box, theme::kPanelCorner, theme::kPanelBg);
  paint::border(g, box, theme::kPanelCorner, theme::kBorder);
  // Fork (TK3J): the wham half.
  const float right = static_cast<float>(pedal_.getX() - kSectionGap / 2);
  paint::hairlineH(g, static_cast<float>(kPadSide), right, static_cast<float>(kDividerY), theme::kBorder);
  g.setFont(Fonts::mono(11.0f, true));
  g.setColour(theme::kGray);
  g.drawText("WHAM", kPadSide, kModeRowY, 48, kModeRowH, juce::Justification::centredLeft, false);
}

void PitchDeckPanel::resized() {
  auto content = contentBounds();
  content.removeFromTop(kPadTop);
  content.removeFromBottom(kPadBottom);
  content.reduce(kPadSide, 0);
  const int height = Knob::heightFor(theme::kKnobSizeSecondary);

  // Fork (TK3J): the pedal runs down the right edge.
  pedal_.setBounds(content.getRight() - WhamPedal::kWidth, content.getY(), WhamPedal::kWidth, content.getHeight());

  // STEP takes a knob column, its toggle centred on the knob row; the text
  // is its own label (as the EQ card's PRE has none).
  const int row1Top = content.getY();
  const auto step = column(content, 0);
  step_.setTopLeftPosition(step.getCentreX() - step_.getWidth() / 2,
                           row1Top + theme::kKnobSizeSecondary / 2 - step_.getHeight() / 2);
  int i = 1;
  for (auto* k : {&tonality_, &window_}) {
    const auto c = column(content, i++);
    k->setBounds(c.getX(), row1Top, kSectionWidth, height);
  }

  // Wham: the mode switch after its label, then Heel | Rate | Shape | RETRIG.
  modeSwitch_.setBounds(kPadSide + 52, kModeRowY, modeSwitch_.getWidth() > 0 ? modeSwitch_.getWidth() : 170, kModeRowH);
  const int x0 = column(content, 0).getX();
  heelDim_.setBounds(x0, kWhamKnobY, kSectionWidth, height);
  heel_.setBounds(0, 0, kSectionWidth, height);
  const int autoX = column(content, 1).getX();
  autoDim_.setBounds(autoX, kWhamKnobY, column(content, 3).getRight() - autoX, height);
  rate_.setBounds(0, 0, kSectionWidth, height);
  shape_.setBounds(kSectionWidth + kSectionGap, 0, kSectionWidth, height);
  retrig_.setTopLeftPosition(2 * (kSectionWidth + kSectionGap) + kSectionWidth / 2 - retrig_.getWidth() / 2,
                             theme::kKnobSizeSecondary / 2 - retrig_.getHeight() / 2);
}

}  // namespace t3k::ui
