#include "DjontEditor.h"

#include "DjontBinary.h"

namespace djont {

namespace {
constexpr float kArcStart = juce::MathConstants<float>::pi * 1.25f;
constexpr float kArcEnd = juce::MathConstants<float>::pi * 2.75f;
constexpr int kPad = 24, kHeaderH = 92, kRule = 3;
}  // namespace

// ---------------------------------------------------------------- look
DjontLookAndFeel::DjontLookAndFeel()
    : anton_(juce::Typeface::createSystemTypefaceFor(DjontBinary::AntonRegular_ttf,
                                                      DjontBinary::AntonRegular_ttfSize)) {
  setColour(juce::BubbleComponent::backgroundColourId, colours::ink);
  setColour(juce::BubbleComponent::outlineColourId, colours::ink);
  setColour(juce::TooltipWindow::backgroundColourId, colours::ink);
  setColour(juce::TooltipWindow::textColourId, colours::bone);
}

juce::Font DjontLookAndFeel::display(const juce::Component& c, float px) {
  if (auto* lnf = dynamic_cast<DjontLookAndFeel*>(&c.getLookAndFeel()))
    return juce::Font(juce::FontOptions(lnf->anton_).withHeight(px));
  return juce::Font(juce::FontOptions(px, juce::Font::bold));
}

juce::Font DjontLookAndFeel::label(float px) {
  return juce::Font(juce::FontOptions(px, juce::Font::bold)).withExtraKerningFactor(0.08f);
}

void DjontLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start,
                                        float end, juce::Slider& slider) {
  const auto box = juce::Rectangle<int>(x, y, w, h).toFloat();
  const float r = juce::jmin(box.getWidth(), box.getHeight()) / 2.0f;
  const auto c = box.getCentre();
  const float angle = start + pos * (end - start);
  const float stroke = juce::jmax(3.0f, r * 0.12f);
  const float arcR = r - stroke / 2.0f;

  juce::Path track;
  track.addCentredArc(c.x, c.y, arcR, arcR, 0.0f, start, end, true);
  g.setColour(colours::ink.withAlpha(0.14f));
  g.strokePath(track, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

  juce::Path value;
  const float from = slider.getProperties()["bipolar"] ? (start + end) / 2.0f : start;
  value.addCentredArc(c.x, c.y, arcR, arcR, 0.0f, juce::jmin(from, angle), juce::jmax(from, angle), true);
  g.setColour(colours::accent);
  g.strokePath(value, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

  const float bodyR = r - stroke - juce::jmax(3.0f, r * 0.1f);
  g.setColour(colours::ink.withAlpha(0.25f));
  g.fillEllipse(juce::Rectangle<float>(2 * bodyR, 2 * bodyR).withCentre(c.translated(0, 2.0f)));
  g.setColour(colours::ink);
  g.fillEllipse(juce::Rectangle<float>(2 * bodyR, 2 * bodyR).withCentre(c));
  g.setColour(colours::inkSoft.withAlpha(0.6f));
  g.drawEllipse(juce::Rectangle<float>(2 * bodyR - 6, 2 * bodyR - 6).withCentre(c), 1.0f);

  const juce::Point<float> dir(std::sin(angle), -std::cos(angle));
  g.setColour(slider.isMouseOverOrDragging() ? colours::accent : colours::bone);
  g.drawLine(juce::Line<float>(c + dir * (bodyR * 0.3f), c + dir * (bodyR * 0.82f)), juce::jmax(2.5f, r * 0.08f));
}

// ---------------------------------------------------------------- knob
Knob::Knob(juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& name, int size)
    : name_(name.toUpperCase()), size_(size) {
  slider_.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  slider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  slider_.setRotaryParameters(kArcStart, kArcEnd, true);
  slider_.setTitle(name);
  slider_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
  slider_.onValueChange = [this] { repaint(); };
  slider_.onDragStart = [this] { repaint(); };
  slider_.onDragEnd = [this] { repaint(); };
  addAndMakeVisible(slider_);
  attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, id, slider_);
  if (auto* p = state.getParameter(id))
    slider_.setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue()));
  setSize(juce::jmax(size + 16, 70), size + kLabelH + 6);
}

void Knob::resized() { slider_.setBounds((getWidth() - size_) / 2, 0, size_, size_); }

void Knob::paint(juce::Graphics& g) {
  const bool live = slider_.isMouseButtonDown();
  const auto text = live ? slider_.getTextFromValue(slider_.getValue()) + slider_.getTextValueSuffix() : name_;
  g.setColour(live ? colours::accent : colours::ink);
  g.setFont(DjontLookAndFeel::label(11.5f));
  g.drawText(text, 0, size_ + 6, getWidth(), Knob::kLabelH, juce::Justification::centredTop, false);
}

// ---------------------------------------------------------------- power LED
PowerLed::PowerLed(juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& name)
    : juce::Button(name) {
  setClickingTogglesState(true);
  setTitle(name + " power");
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  onStateChange = [this] {
    if (onPower) onPower(getToggleState());
  };
  attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, id, *this);
  setSize(26, 26);
}

void PowerLed::paintButton(juce::Graphics& g, bool highlighted, bool) {
  const auto box = getLocalBounds().toFloat().reduced(2.0f);
  const bool on = getToggleState();
  if (on) {
    g.setColour(colours::accent.withAlpha(0.25f));
    g.fillEllipse(box.expanded(2.0f));
  }
  g.setColour(on ? colours::accent : (highlighted ? colours::ink.withAlpha(0.12f) : colours::bone));
  g.fillEllipse(box.reduced(1.0f));
  g.setColour(colours::ink);
  g.drawEllipse(box.reduced(1.0f), 2.0f);
}

// ---------------------------------------------------------------- mode switch
ModeSwitch::ModeSwitch(juce::AudioProcessorValueTreeState& state, const juce::String& id)
    : param_(state.getParameter(id)) {
  setTitle("Sub mode");
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  attachment_ = std::make_unique<juce::ParameterAttachment>(*param_, [this](float v) {
    index_ = juce::roundToInt(v);
    repaint();
  });
  attachment_->sendInitialUpdate();
  setSize(140, 26);
}

void ModeSwitch::paint(juce::Graphics& g) {
  const auto box = getLocalBounds().toFloat().reduced(1.0f);
  const char* labels[] = {"CLEAN", "SYNTH"};
  const float w = box.getWidth() / 2.0f;
  for (int i = 0; i < 2; ++i) {
    const auto seg = juce::Rectangle<float>(box.getX() + i * w, box.getY(), w, box.getHeight());
    const bool on = index_ == i;
    if (on) {
      g.setColour(colours::ink);
      g.fillRect(seg);
    }
    g.setColour(on ? colours::bone : colours::ink);
    g.setFont(DjontLookAndFeel::label(11.0f));
    g.drawText(labels[i], seg, juce::Justification::centred, false);
  }
  g.setColour(colours::ink);
  g.drawRect(box, 2.0f);
}

void ModeSwitch::mouseDown(const juce::MouseEvent& e) {
  const int i = e.x < getWidth() / 2 ? 0 : 1;
  attachment_->setValueAsCompleteGesture(static_cast<float>(i));
}

// ---------------------------------------------------------------- module
Module::Module(juce::AudioProcessorValueTreeState& state, const juce::String& title, const juce::String& powerId)
    : title_(title), power_(state, powerId, title) {
  addAndMakeVisible(power_);
  addAndMakeVisible(body_);
  body_.setInterceptsMouseClicks(false, true);
  power_.onPower = [this](bool on) { setPowered(on); };
  setPowered(power_.getToggleState());
}

void Module::setPowered(bool on) { body_.setAlpha(on ? 1.0f : 0.38f); }

void Module::addKnob(std::unique_ptr<Knob> knob) {
  body_.addAndMakeVisible(*knob);
  knobs_.push_back(std::move(knob));
}

void Module::setExtra(std::unique_ptr<juce::Component> extra) {
  extra_ = std::move(extra);
  body_.addAndMakeVisible(*extra_);
}

void Module::paint(juce::Graphics& g) {
  g.setColour(colours::ink);
  g.setFont(DjontLookAndFeel::display(*this, 40.0f));
  g.drawText(title_, 18, 10, getWidth() - 70, 48, juce::Justification::centredLeft, false);
}

void Module::resized() {
  power_.setTopRightPosition(getWidth() - 18, 22);
  body_.setBounds(0, 64, getWidth(), getHeight() - 64);
  int y = 6;
  if (extra_ != nullptr) {
    extra_->setTopLeftPosition((getWidth() - extra_->getWidth()) / 2, y);
    y += extra_->getHeight() + 14;
  }
  // Two knobs side by side; a third goes centred underneath.
  const int count = static_cast<int>(knobs_.size());
  const int perRow = juce::jmin(2, count);
  for (int row = 0, i = 0; i < count; ++row) {
    const int inRow = juce::jmin(perRow, count - i);
    int rowW = 0, rowH = 0;
    for (int k = 0; k < inRow; ++k) {
      rowW += knobs_[static_cast<size_t>(i + k)]->getWidth();
      rowH = juce::jmax(rowH, knobs_[static_cast<size_t>(i + k)]->getHeight());
    }
    const int gap = inRow > 1 ? 10 : 0;
    int x = (getWidth() - rowW - gap * (inRow - 1)) / 2;
    for (int k = 0; k < inRow; ++k) {
      auto& knob = *knobs_[static_cast<size_t>(i + k)];
      knob.setTopLeftPosition(x, y);
      x += knob.getWidth() + gap;
    }
    y += rowH + 8;
    i += inRow;
  }
}

// ---------------------------------------------------------------- editor
DjontEditor::DjontEditor(DjontProcessor& processor)
    : juce::AudioProcessorEditor(processor),
      processor_(processor),
      input_(processor.params, "input", "Input", 46),
      output_(processor.params, "output", "Output", 46) {
  setLookAndFeel(&lnf_);
  input_.setBipolar();
  output_.setBipolar();
  addAndMakeVisible(input_);
  addAndMakeVisible(output_);

  auto& s = processor.params;
  auto tight = std::make_unique<Module>(s, "TIGHT", "tightOn");
  tight->addKnob(std::make_unique<Knob>(s, "tightAmount", "Amount", 62));
  tight->addKnob(std::make_unique<Knob>(s, "tightFreq", "Freq", 62));

  auto grit = std::make_unique<Module>(s, "GRIT", "gritOn");
  grit->addKnob(std::make_unique<Knob>(s, "gritDrive", "Drive", 62));
  grit->addKnob(std::make_unique<Knob>(s, "gritBlend", "Blend", 62));
  grit->addKnob(std::make_unique<Knob>(s, "gritFreq", "Freq", 50));

  auto sub = std::make_unique<Module>(s, "SUB", "subOn");
  sub->setExtra(std::make_unique<ModeSwitch>(s, "subMode"));
  sub->addKnob(std::make_unique<Knob>(s, "subBlend", "Blend", 62));
  sub->addKnob(std::make_unique<Knob>(s, "subTone", "Tone", 62));

  auto lofi = std::make_unique<Module>(s, "LO-FI", "lofiOn");
  lofi->addKnob(std::make_unique<Knob>(s, "lofiTone", "Tone", 62));
  lofi->addKnob(std::make_unique<Knob>(s, "lofiBlend", "Blend", 62));

  for (auto* m : {tight.get(), grit.get(), sub.get(), lofi.get()}) addAndMakeVisible(*m);
  modules_.push_back(std::move(tight));
  modules_.push_back(std::move(grit));
  modules_.push_back(std::move(sub));
  modules_.push_back(std::move(lofi));

  setSize(kWidth, kHeight);
  startTimerHz(30);
}

DjontEditor::~DjontEditor() { setLookAndFeel(nullptr); }

void DjontEditor::timerCallback() {
  const float db = processor_.outputPeakDb.load();
  if (std::abs(db - meterDb_) > 0.2f) {
    meterDb_ = db;
    repaint(meterBox_);
  }
}

void DjontEditor::paint(juce::Graphics& g) {
  g.fillAll(colours::bone);

  // Wordmark: d JON t, the name in the accent.
  auto font = DjontLookAndFeel::display(*this, 72.0f);
  g.setFont(font);
  float x = static_cast<float>(kPad);
  const float baselineY = static_cast<float>(kPad - 6);
  const std::pair<const char*, juce::Colour> parts[] = {
      {"d", colours::ink}, {"JON", colours::accent}, {"t", colours::ink}};
  for (const auto& [text, colour] : parts) {
    const float w = juce::GlyphArrangement::getStringWidth(font, text);
    g.setColour(colour);
    g.drawText(text, juce::Rectangle<float>(x, baselineY, w + 2, 80.0f), juce::Justification::topLeft, false);
    x += w;
  }

  // Rules: under the header and between the modules.
  g.setColour(colours::ink);
  g.fillRect(kPad, kHeaderH + kPad - 6, getWidth() - 2 * kPad, kRule);
  for (size_t i = 1; i < modules_.size(); ++i) {
    const auto& m = *modules_[i];
    g.fillRect(m.getX() - 1, m.getY() + 12, 2, m.getHeight() - 24);
  }

  // Output meter.
  g.setColour(colours::ink.withAlpha(0.12f));
  g.fillRect(meterBox_);
  const float level = juce::jlimit(0.0f, 1.0f, (meterDb_ + 60.0f) / 60.0f);
  const auto filled = meterBox_.toFloat().withTop(meterBox_.getBottom() - level * meterBox_.getHeight());
  g.setColour(meterDb_ > -1.0f ? juce::Colour(0xffd7261e) : colours::accent);
  g.fillRect(filled);
}

void DjontEditor::resized() {
  const int right = getWidth() - kPad;
  meterBox_ = {right - 10, kPad, 10, 62};
  output_.setTopRightPosition(meterBox_.getX() - 14, kPad - 2);
  input_.setTopRightPosition(output_.getX() - 6, kPad - 2);

  const int top = kHeaderH + kPad - 6 + kRule;
  const int w = (getWidth() - 2 * kPad) / static_cast<int>(modules_.size());
  for (size_t i = 0; i < modules_.size(); ++i)
    modules_[i]->setBounds(kPad + static_cast<int>(i) * w, top, w, getHeight() - top - 8);
}

}  // namespace djont
