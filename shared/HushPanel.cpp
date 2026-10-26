#include "HushPanel.h"

namespace hushui {

namespace {
const juce::Identifier kFloorsProp{"hushFloors"};
const juce::Identifier kParamIdProp{"paramId"};
const char* const kBandLabels[Hush::kBands] = {"200", "350", "600", "1K", "1.8K", "3.2K", "5.6K", "9K+"};
}  // namespace

void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
  layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"hushOn", 1}, "Hush", false));
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"hushStrength", 1}, "Hush Strength", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 60.0f,
      juce::AudioParameterFloatAttributes().withLabel("%")));
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"hushSensitivity", 1}, "Hush Sensitivity", juce::NormalisableRange<float>(0.0f, 24.0f, 0.5f), 6.0f,
      juce::AudioParameterFloatAttributes().withLabel("dB")));
  juce::NormalisableRange<float> release(10.0f, 500.0f, 1.0f);
  release.setSkewForCentre(100.0f);
  layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"hushRelease", 1}, "Hush Release", release, 120.0f,
                                                         juce::AudioParameterFloatAttributes().withLabel("ms")));
}

void Params::attach(juce::AudioProcessorValueTreeState& state) {
  on = state.getRawParameterValue("hushOn");
  strength = state.getRawParameterValue("hushStrength");
  sensitivity = state.getRawParameterValue("hushSensitivity");
  release = state.getRawParameterValue("hushRelease");
}

void Params::apply(Hush& hush) const {
  hush.set(on->load() > 0.5f, strength->load() / 100.0f, sensitivity->load(), release->load());
}

void saveFloors(const Hush& hush, juce::ValueTree& state) {
  juce::StringArray values;
  for (const auto& f : hush.floorDb) values.add(juce::String(f.load(), 1));
  state.setProperty(kFloorsProp, values.joinIntoString(","), nullptr);
}

void loadFloors(Hush& hush, const juce::ValueTree& state) {
  const auto values = juce::StringArray::fromTokens(state.getProperty(kFloorsProp).toString(), ",", "");
  for (int b = 0; b < Hush::kBands; ++b)
    hush.floorDb[static_cast<size_t>(b)].store(b < values.size() ? values[b].getFloatValue() : Hush::kDefaultFloorDb);
}

// ---------------------------------------------------------------- panel
HushPanel::HushPanel(juce::AudioProcessorValueTreeState& state, Hush& hush,
                     std::function<juce::Font(const juce::Component&, float)> displayFont, bool showDone)
    : state_(state), hush_(hush), display_(std::move(displayFont)), showDone_(showDone) {
  power_.setClickingTogglesState(true);
  power_.getProperties().set(kParamIdProp, "hushOn");
  power_.setHelpText("Hush on / off. Off is a perfect bypass. Your presets and settings don't change when it's off.");
  powerAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, "hushOn", power_);
  power_.onStateChange = [this] { power_.setButtonText(power_.getToggleState() ? "ON" : "OFF"); };
  power_.setButtonText(power_.getToggleState() ? "ON" : "OFF");
  learn_.setHelpText("LEARN: take your hands off the strings (volume up, as you'd play) and click. Hush listens for two "
                     "seconds and sets each band's noise floor. No delay is added; it only sets the thresholds.");
  learn_.onClick = [this] {
    hush_.startLearn();
    status_ = "Listening... keep your hands off the strings.";
    repaint();
  };
  done_.onClick = [this] {
    if (onClose) onClose();
  };
  for (auto* b : {&power_, &learn_, &done_}) {
    b->setMouseCursor(juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible(b);
  }
  done_.setVisible(showDone_);
  setupKnob(strength_, "hushStrength", "Strength",
            "Hush Strength: how far a band with only noise in it is turned down (100% = 30 dB). Raise it until the "
            "hiss is gone; lower it if quiet notes lose their sparkle.");
  setupKnob(sensitivity_, "hushSensitivity", "Sensitivity",
            "Hush Sensitivity: how far over its noise floor a band has to be to count as your note. Higher cuts "
            "more noise but can dull quiet notes; lower is gentler.");
  setupKnob(release_, "hushRelease", "Release",
            "Hush Release: how quickly a band turns down once your note leaves it. Short is tight for fast "
            "playing; long is smoother on sustained notes.");
  learnedSeen_ = hush_.learnedCount.load();
  status_ = "Hands off the strings, then LEARN.";
  startTimerHz(20);
}

HushPanel::~HushPanel() = default;

void HushPanel::setupKnob(Knob& k, const juce::String& id, const juce::String& label, const juce::String& help) {
  k.label = label;
  k.slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  k.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  k.slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
  k.slider.setMouseCursor(juce::MouseCursor::PointingHandCursor);
  k.slider.setHelpText(help);
  k.slider.getProperties().set(kParamIdProp, id);
  k.slider.onValueChange = [this] { repaint(); };
  k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state_, id, k.slider);
  if (auto* p = state_.getParameter(id)) k.slider.setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue()));
  addAndMakeVisible(k.slider);
}

juce::String HushPanel::hint() const {
  return "Hush: a de-noiser for your DI. Turn it on, take your hands off the strings and click LEARN, then raise "
         "Strength until the hiss is gone. Zero latency.";
}

void HushPanel::timerCallback() {
  bool dirty = false;
  for (int b = 0; b < Hush::kBands; ++b) {
    const float r = hush_.reduction[static_cast<size_t>(b)].load();
    const float shown = r > shown_[static_cast<size_t>(b)] ? r : juce::jmax(r, shown_[static_cast<size_t>(b)] - 1.5f);
    if (std::abs(shown - shown_[static_cast<size_t>(b)]) > 0.1f) {
      shown_[static_cast<size_t>(b)] = shown;
      dirty = true;
    }
  }
  if (hush_.learning()) {
    status_ = "Listening... keep your hands off the strings. " + juce::String(juce::roundToInt(hush_.learnProgress() * 100)) + "%";
    dirty = true;
  } else if (const int learned = hush_.learnedCount.load(); learned != learnedSeen_) {
    learnedSeen_ = learned;
    saveFloors(hush_, state_.state);
    status_ = "Learned. Now play, and raise Strength until the hiss is gone.";
    dirty = true;
  }
  if (dirty) repaint();
}

void HushPanel::paint(juce::Graphics& g) {
  const auto ink = findColour(juce::Label::textColourId);
  const auto accent = findColour(juce::Slider::thumbColourId);
  const auto bg = findColour(juce::ResizableWindow::backgroundColourId);
  const auto box = getLocalBounds().toFloat().reduced(0.5f);
  g.setColour(bg.withAlpha(0.95f));
  g.fillRoundedRectangle(box, 6.0f);
  g.setColour(ink.withAlpha(0.18f));
  g.drawRoundedRectangle(box, 6.0f, 1.0f);

  if (showDone_) {  // inside dJONt / Parabola; the Hush plugin has its own header
    g.setColour(ink);
    g.setFont(display_(*this, 30.0f));
    g.drawText("HUSH", 18, 10, 120, 36, juce::Justification::centredLeft, false);
    g.setColour(ink.withAlpha(0.6f));
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)).withExtraKerningFactor(0.12f));
    g.drawText("DE-NOISER  /  ZERO LATENCY", 110, 22, 220, 14, juce::Justification::centredLeft, false);
  }

  // Knob labels.
  for (auto* k : {&strength_, &sensitivity_, &release_}) {
    const bool live = k->slider.isMouseButtonDown();
    auto text = k->label.toUpperCase();
    if (live) {
      text = juce::String(k->slider.getValue(), k == &sensitivity_ ? 1 : 0) + " " +
             (k == &strength_ ? "%" : k == &sensitivity_ ? "dB" : "ms");
    }
    g.setColour(live ? accent : ink);
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)).withExtraKerningFactor(0.12f));
    g.drawText(text, k->slider.getX() - 20, k->slider.getBottom() + 4, k->slider.getWidth() + 40, 14,
               juce::Justification::centredTop, false);
  }

  // Status line and how-to.
  g.setColour(hush_.learning() ? accent : ink.withAlpha(0.85f));
  g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
  auto text = textBox_;
  g.drawFittedText(status_, text.removeFromTop(20), juce::Justification::centredLeft, 1, 0.9f);
  if (hush_.learning()) {
    auto bar = juce::Rectangle<float>(static_cast<float>(text.getX()), static_cast<float>(text.getY() + 2),
                                      static_cast<float>(text.getWidth()), 3.0f);
    g.setColour(ink.withAlpha(0.15f));
    g.fillRect(bar);
    g.setColour(accent);
    g.fillRect(bar.withWidth(bar.getWidth() * hush_.learnProgress()));
  }
  auto how = text;
  how.removeFromTop(10);
  g.setColour(ink.withAlpha(0.7f));
  g.setFont(juce::Font(juce::FontOptions(12.0f)));
  g.drawFittedText("1. Turn it ON.  2. Volume up, hands off the strings, click LEARN (2 seconds).  3. Play, and raise "
                   "Strength until the hiss is gone. If quiet notes lose sparkle, lower Sensitivity or Strength.",
                   how, juce::Justification::topLeft, 4, 1.0f);

  // Meters: how far each band is turned down right now.
  const auto m = meterBox_.toFloat();
  g.setColour(ink.withAlpha(0.6f));
  g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)).withExtraKerningFactor(0.1f));
  g.drawText("NOISE CUT, PER BAND", m.withHeight(14.0f), juce::Justification::centredLeft, false);
  const float w = m.getWidth() / Hush::kBands;
  const auto bars = m.withTrimmedTop(20.0f).withTrimmedBottom(16.0f);
  for (int b = 0; b < Hush::kBands; ++b) {
    const auto col = juce::Rectangle<float>(bars.getX() + b * w + w * 0.2f, bars.getY(), w * 0.6f, bars.getHeight());
    g.setColour(ink.withAlpha(0.1f));
    g.fillRoundedRectangle(col, 2.0f);
    const float frac = juce::jlimit(0.0f, 1.0f, shown_[static_cast<size_t>(b)] / Hush::kMaxCutDb);
    g.setColour(accent);
    g.fillRoundedRectangle(col.withHeight(col.getHeight() * frac), 2.0f);
    g.setColour(ink.withAlpha(0.7f));
    g.drawText(kBandLabels[b], juce::Rectangle<float>(col.getCentreX() - 22.0f, bars.getBottom() + 2.0f, 44.0f, 13.0f),
               juce::Justification::centred, false);
  }
}

void HushPanel::resized() {
  const int top = showDone_ ? 58 : 22;
  power_.setBounds(18, top, 70, 30);
  learn_.setBounds(96, top, 90, 30);
  done_.setBounds(getWidth() - 18 - 84, 16, 84, 26);
  int x = 18;
  for (auto* k : {&strength_, &sensitivity_, &release_}) {
    k->slider.setBounds(x + 8, top + 48, 64, 64);
    x += 92;
  }
  textBox_ = juce::Rectangle<int>(18, top + 140, juce::jmin(300, getWidth() / 2 - 30), juce::jmax(60, getHeight() - top - 150));
  const int mx = juce::jmax(300, getWidth() / 2);
  meterBox_ = juce::Rectangle<int>(mx, top, getWidth() - mx - 18, juce::jlimit(120, 380, getHeight() - top - 30));
}

}  // namespace hushui
