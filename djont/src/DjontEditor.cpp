#include "DjontEditor.h"

#include "DjontBinary.h"

// The standalone app's audio device and input-mute live in its holder. Only
// the plugin build has one; the test app (and a DAW at runtime) does not.
#if defined(JucePlugin_Build_Standalone) && JucePlugin_Build_Standalone && \
    !(defined(JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP) && JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP)
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#define DJONT_HAS_STANDALONE_HOLDER 1
#else
#define DJONT_HAS_STANDALONE_HOLDER 0
#endif

namespace djont {

namespace {
constexpr float kArcStart = juce::MathConstants<float>::pi * 1.25f;
constexpr float kArcEnd = juce::MathConstants<float>::pi * 2.75f;
constexpr int kPad = 24, kHeaderH = 110, kRule = 3, kHintH = 30;
constexpr int kBodyTop = kHeaderH + kRule;
const juce::String kDefaultHint = "Hover any control to see what it does. Double-click a knob to reset it.";
const juce::Identifier kLookProp{"look"};

juce::Colour rgba(int r, int g, int b, float a) { return juce::Colour::fromRGBA(static_cast<juce::uint8>(r), static_cast<juce::uint8>(g), static_cast<juce::uint8>(b), static_cast<juce::uint8>(juce::roundToInt(a * 255.0f))); }

const Look kChrome{"chrome", "Chrome",
                   juce::Colour(0xff0b0b0b), juce::Colour(0xffece6d6), juce::Colour(0xffff5a1f),
                   rgba(12, 12, 12, 0.74f), rgba(236, 230, 214, 0.14f), rgba(236, 230, 214, 0.28f),
                   juce::Colour(0xff2a2a2a), juce::Colour(0xff0d0d0d), rgba(236, 230, 214, 0.16f),
                   rgba(236, 230, 214, 0.16f), rgba(8, 8, 8, 0.88f), juce::Colour(0xffd4cebf)};
const Look kRedFoil{"redfoil", "Red Foil",
                    juce::Colour(0xff05060c), juce::Colour(0xfff2ecf0), juce::Colour(0xffff3b2f),
                    rgba(4, 6, 14, 0.74f), rgba(130, 160, 255, 0.20f), rgba(242, 236, 240, 0.26f),
                    juce::Colour(0xff1d2236), juce::Colour(0xff070912), rgba(130, 160, 255, 0.22f),
                    rgba(242, 236, 240, 0.14f), rgba(4, 5, 10, 0.90f), juce::Colour(0xffcfc9d8)};

juce::Image loadPhoto(const juce::String& lookId) {
  if (lookId == "redfoil")
    return juce::ImageFileFormat::loadFrom(DjontBinary::djontredfoil_jpg, DjontBinary::djontredfoil_jpgSize);
  return juce::ImageFileFormat::loadFrom(DjontBinary::djontchrome_jpg, DjontBinary::djontchrome_jpgSize);
}

#if DJONT_HAS_STANDALONE_HOLDER
juce::StandalonePluginHolder* holder(const juce::AudioProcessor& p) {
  if (p.wrapperType != juce::AudioProcessor::wrapperType_Standalone) return nullptr;
  return juce::StandalonePluginHolder::getInstance();
}
#endif
}  // namespace

const char* const kLookIds[2] = {"chrome", "redfoil"};

const Look& lookFor(const juce::String& id) { return id == "redfoil" ? kRedFoil : kChrome; }

// ---------------------------------------------------------------- look
DjontLookAndFeel::DjontLookAndFeel()
    : anton_(juce::Typeface::createSystemTypefaceFor(DjontBinary::AntonRegular_ttf,
                                                      DjontBinary::AntonRegular_ttfSize)),
      look_(&kChrome) {
  setLook(kChrome);
}

void DjontLookAndFeel::setLook(const Look& look) {
  look_ = &look;
  const auto panel = look.bg.brighter(0.08f);
  setColour(juce::ResizableWindow::backgroundColourId, look.bg);
  setColour(juce::Label::textColourId, look.ink);
  setColour(juce::ComboBox::backgroundColourId, panel);
  setColour(juce::ComboBox::textColourId, look.ink);
  setColour(juce::ComboBox::outlineColourId, look.ink.withAlpha(0.3f));
  setColour(juce::ComboBox::arrowColourId, look.accent);
  setColour(juce::ComboBox::focusedOutlineColourId, look.accent);
  setColour(juce::PopupMenu::backgroundColourId, panel);
  setColour(juce::PopupMenu::textColourId, look.ink);
  setColour(juce::PopupMenu::highlightedBackgroundColourId, look.accent);
  setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
  setColour(juce::ListBox::backgroundColourId, panel);
  setColour(juce::ListBox::outlineColourId, look.ink.withAlpha(0.2f));
  setColour(juce::ListBox::textColourId, look.ink);
  setColour(juce::TextButton::buttonColourId, panel);
  setColour(juce::TextButton::buttonOnColourId, look.accent);
  setColour(juce::TextButton::textColourOffId, look.ink);
  setColour(juce::TextButton::textColourOnId, juce::Colours::black);
  setColour(juce::ToggleButton::textColourId, look.ink);
  setColour(juce::ToggleButton::tickColourId, look.accent);
  setColour(juce::ToggleButton::tickDisabledColourId, look.ink.withAlpha(0.5f));
  setColour(juce::ScrollBar::thumbColourId, look.ink.withAlpha(0.35f));
  setColour(juce::Slider::thumbColourId, look.accent);
  setColour(juce::Slider::trackColourId, look.accent);
  setColour(juce::Slider::backgroundColourId, look.track);
  setColour(juce::Slider::textBoxTextColourId, look.ink);
  setColour(juce::Slider::textBoxOutlineColourId, look.ink.withAlpha(0.2f));
  setColour(juce::BubbleComponent::backgroundColourId, look.bg);
  setColour(juce::BubbleComponent::outlineColourId, look.ink);
}

juce::Font DjontLookAndFeel::display(const juce::Component& c, float px) {
  if (auto* lnf = dynamic_cast<DjontLookAndFeel*>(&c.getLookAndFeel()))
    return juce::Font(juce::FontOptions(lnf->anton_).withHeight(px));
  return juce::Font(juce::FontOptions(px, juce::Font::bold));
}

juce::Font DjontLookAndFeel::label(float px) {
  return juce::Font(juce::FontOptions(px, juce::Font::bold)).withExtraKerningFactor(0.12f);
}

const Look& DjontLookAndFeel::lookOf(const juce::Component& c) {
  if (auto* lnf = dynamic_cast<DjontLookAndFeel*>(&c.getLookAndFeel())) return lnf->look();
  return kChrome;
}

void DjontLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start,
                                        float end, juce::Slider& slider) {
  const auto& L = look();
  const auto box = juce::Rectangle<int>(x, y, w, h).toFloat();
  const float r = juce::jmin(box.getWidth(), box.getHeight()) / 2.0f;
  const auto c = box.getCentre();
  const float angle = start + pos * (end - start);
  const float stroke = juce::jmax(3.0f, r * 0.11f);
  const float arcR = r - stroke / 2.0f;

  juce::Path track;
  track.addCentredArc(c.x, c.y, arcR, arcR, 0.0f, start, end, true);
  g.setColour(L.track);
  g.strokePath(track, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

  juce::Path value;
  const float from = slider.getProperties()["bipolar"] ? (start + end) / 2.0f : start;
  value.addCentredArc(c.x, c.y, arcR, arcR, 0.0f, juce::jmin(from, angle), juce::jmax(from, angle), true);
  g.setColour(L.accent);
  g.strokePath(value, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

  // The face: a dark dome lit from the top left, with a drop shadow.
  const float bodyR = r - stroke - juce::jmax(2.0f, r * 0.06f);
  const auto face = juce::Rectangle<float>(2 * bodyR, 2 * bodyR).withCentre(c);
  g.setColour(juce::Colours::black.withAlpha(0.5f));
  g.fillEllipse(face.translated(0, 2.0f).expanded(1.0f));
  g.setGradientFill(juce::ColourGradient(L.faceHi, face.getX() + face.getWidth() * 0.4f,
                                         face.getY() + face.getHeight() * 0.3f, L.faceLo, face.getRight(),
                                         face.getBottom(), true));
  g.fillEllipse(face);
  g.setColour(L.knobRing);
  g.drawEllipse(face.reduced(0.5f), 1.0f);

  const juce::Point<float> dir(std::sin(angle), -std::cos(angle));
  g.setColour(slider.isMouseOverOrDragging() ? L.accent : L.ink);
  g.drawLine(juce::Line<float>(c + dir * (bodyR * 0.35f), c + dir * (bodyR * 0.86f)), juce::jmax(2.5f, r * 0.08f));
}

// ---------------------------------------------------------------- knob
Knob::Knob(juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& name, int size,
           juce::String description)
    : name_(name), description_(std::move(description)), size_(size) {
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
  param_ = state.getParameter(id);
  if (param_ != nullptr)
    slider_.setDoubleClickReturnValue(true, param_->convertFrom0to1(param_->getDefaultValue()));
  setSize(juce::jmax(size + 16, 70), size + kLabelH + 6);
}

juce::String Knob::valueText() const {
  if (param_ == nullptr) return {};
  auto text = param_->getCurrentValueAsText();
  if (param_->getLabel().isNotEmpty()) text << " " << param_->getLabel();
  return text;
}

juce::String Knob::hint() const { return description_ + "  " + valueText(); }

void Knob::resized() { slider_.setBounds((getWidth() - size_) / 2, 0, size_, size_); }

void Knob::paint(juce::Graphics& g) {
  const auto& L = DjontLookAndFeel::lookOf(*this);
  const bool live = slider_.isMouseButtonDown();
  g.setColour(live ? L.accent : L.ink);
  g.setFont(DjontLookAndFeel::label(10.0f));
  g.drawText(live ? valueText() : name_.toUpperCase(), 0, size_ + 6, getWidth(), Knob::kLabelH,
             juce::Justification::centredTop, false);
}

// ---------------------------------------------------------------- power LED
PowerLed::PowerLed(juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& name,
                   juce::String description)
    : juce::Button(name), name_(name), description_(std::move(description)) {
  setClickingTogglesState(true);
  setTitle(name + " power");
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  onStateChange = [this] {
    if (onPower) onPower(getToggleState());
  };
  attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, id, *this);
  setSize(26, 26);
}

juce::String PowerLed::hint() const {
  return name_ + ": " + (getToggleState() ? "on" : "off") + ". " + description_ +
         " Click to bypass; it fades over 20 ms so a stomp mid-riff won't click.";
}

void PowerLed::paintButton(juce::Graphics& g, bool highlighted, bool) {
  const auto& L = DjontLookAndFeel::lookOf(*this);
  const auto box = getLocalBounds().toFloat().reduced(3.0f);
  const bool on = getToggleState();
  if (on) {
    g.setColour(L.accent.withAlpha(0.2f));
    g.fillEllipse(box.expanded(3.0f));
  }
  g.setColour(on ? L.accent : L.ink.withAlpha(highlighted ? 0.25f : 0.12f));
  g.fillEllipse(box.reduced(1.0f));
  g.setColour(L.ink);
  g.drawEllipse(box.reduced(1.0f), 2.0f);
}

// ---------------------------------------------------------------- segmented
Segmented::Segmented(juce::StringArray labels, juce::String description, int width)
    : labels_(std::move(labels)), description_(std::move(description)) {
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  setSize(width, 24);
}

void Segmented::setIndex(int index) {
  index_ = juce::jlimit(0, labels_.size() - 1, index);
  repaint();
}

void Segmented::paint(juce::Graphics& g) {
  const auto& L = DjontLookAndFeel::lookOf(*this);
  const auto box = getLocalBounds().toFloat().reduced(1.0f);
  const float w = box.getWidth() / static_cast<float>(labels_.size());
  for (int i = 0; i < labels_.size(); ++i) {
    const auto seg = juce::Rectangle<float>(box.getX() + static_cast<float>(i) * w, box.getY(), w, box.getHeight());
    const bool on = index_ == i;
    if (on) {
      g.setColour(L.ink);
      g.fillRect(seg);
    }
    g.setColour(on ? L.bg : L.ink);
    g.setFont(DjontLookAndFeel::label(10.0f));
    g.drawText(labels_[i], seg, juce::Justification::centred, false);
  }
  g.setColour(L.ink);
  g.drawRect(box, 2.0f);
}

void Segmented::mouseDown(const juce::MouseEvent& e) {
  const int i = juce::jlimit(0, labels_.size() - 1, e.x * labels_.size() / juce::jmax(1, getWidth()));
  setIndex(i);
  if (onSelect) onSelect(i);
}

ParamSegmented::ParamSegmented(juce::AudioProcessorValueTreeState& state, const juce::String& id,
                               juce::StringArray labels, juce::String description, int width)
    : Segmented(std::move(labels), std::move(description), width) {
  setTitle(id);
  attachment_ = std::make_unique<juce::ParameterAttachment>(*state.getParameter(id),
                                                            [this](float v) { setIndex(juce::roundToInt(v)); });
  attachment_->sendInitialUpdate();
  onSelect = [this](int i) { attachment_->setValueAsCompleteGesture(static_cast<float>(i)); };
}

// ---------------------------------------------------------------- module
namespace {
// "TIGHT" -> "Tight", "LO-FI" -> "Lo-Fi".
juce::String displayName(const juce::String& title) {
  juce::String out;
  bool start = true;
  for (auto ch : title) {
    out += start ? juce::CharacterFunctions::toUpperCase(ch) : juce::CharacterFunctions::toLowerCase(ch);
    start = !juce::CharacterFunctions::isLetter(ch);
  }
  return out;
}
}  // namespace

Module::Module(juce::AudioProcessorValueTreeState& state, const juce::String& title, const juce::String& powerId,
               juce::String powerDescription)
    : title_(title), power_(state, powerId, displayName(title), std::move(powerDescription)) {
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
  const auto& L = DjontLookAndFeel::lookOf(*this);
  const auto box = getLocalBounds().toFloat().reduced(0.5f);
  g.setColour(L.plate);
  g.fillRoundedRectangle(box, 6.0f);
  g.setColour(L.plateBorder);
  g.drawRoundedRectangle(box, 6.0f, 1.0f);
  g.setColour(L.ink);
  g.setFont(DjontLookAndFeel::display(*this, 34.0f));
  g.drawText(title_, 14, 8, getWidth() - 60, 40, juce::Justification::centredLeft, false);
}

void Module::resized() {
  power_.setTopRightPosition(getWidth() - 12, 15);
  body_.setBounds(0, 56, getWidth(), getHeight() - 56);
  // The controls sit centred in the body.
  int contentH = extra_ != nullptr ? extra_->getHeight() + 12 : 0;
  for (size_t i = 0; i < knobs_.size(); i += 2) {
    int rowH = knobs_[i]->getHeight();
    if (i + 1 < knobs_.size()) rowH = juce::jmax(rowH, knobs_[i + 1]->getHeight());
    contentH += rowH + 6;
  }
  int y = juce::jmax(2, (body_.getHeight() - contentH) / 2 - 6);
  if (extra_ != nullptr) {
    extra_->setTopLeftPosition((getWidth() - extra_->getWidth()) / 2, y);
    y += extra_->getHeight() + 12;
  }
  // Two knobs side by side; a third goes centred underneath.
  const int count = static_cast<int>(knobs_.size());
  const int perRow = juce::jmin(2, count);
  for (int i = 0; i < count;) {
    const int inRow = juce::jmin(perRow, count - i);
    int rowW = 0, rowH = 0;
    for (int k = 0; k < inRow; ++k) {
      rowW += knobs_[static_cast<size_t>(i + k)]->getWidth();
      rowH = juce::jmax(rowH, knobs_[static_cast<size_t>(i + k)]->getHeight());
    }
    const int gap = inRow > 1 ? 6 : 0;
    int x = (getWidth() - rowW - gap * (inRow - 1)) / 2;
    for (int k = 0; k < inRow; ++k) {
      auto& knob = *knobs_[static_cast<size_t>(i + k)];
      knob.setTopLeftPosition(x, y);
      x += knob.getWidth() + gap;
    }
    y += rowH + 6;
    i += inRow;
  }
}

// ---------------------------------------------------------------- gear
GearButton::GearButton() : juce::Button("Settings") {
  setTitle("Settings");
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  setSize(30, 30);
}

juce::String GearButton::hint() const {
  return "Settings: the look, and in the standalone app your audio interface (ASIO driver, inputs, outputs, "
         "sample rate, buffer) and latency.";
}

void GearButton::paintButton(juce::Graphics& g, bool highlighted, bool down) {
  const auto& L = DjontLookAndFeel::lookOf(*this);
  const auto c = getLocalBounds().toFloat().getCentre();
  const float outer = 12.0f, inner = 9.0f;
  juce::Path gear;
  constexpr int kTeeth = 8;
  for (int i = 0; i < kTeeth * 2; ++i) {
    const float a0 = juce::MathConstants<float>::twoPi * static_cast<float>(i) / (kTeeth * 2);
    const float a1 = juce::MathConstants<float>::twoPi * static_cast<float>(i + 1) / (kTeeth * 2);
    const float r = (i % 2 == 0) ? outer : inner;
    const auto p0 = c + juce::Point<float>(std::sin(a0), -std::cos(a0)) * r;
    const auto p1 = c + juce::Point<float>(std::sin(a1), -std::cos(a1)) * r;
    if (i == 0) gear.startNewSubPath(p0);
    else gear.lineTo(p0);
    gear.lineTo(p1);
  }
  gear.closeSubPath();
  gear.addEllipse(c.x - 4.0f, c.y - 4.0f, 8.0f, 8.0f);
  gear.setUsingNonZeroWinding(false);
  g.setColour(getToggleState() || down ? L.accent : highlighted ? L.ink : L.ink.withAlpha(0.8f));
  g.fillPath(gear);
}

// ---------------------------------------------------------------- settings
SettingsPanel::SettingsPanel(DjontProcessor& processor, std::function<void(const juce::String&)> onLook)
    : processor_(processor),
      look_({"CHROME", "RED FOIL"}, "Look: the photo and accent color. Saved with the plugin's state.", 200) {
  addAndMakeVisible(look_);
  look_.onSelect = [onLook](int i) { onLook(kLookIds[i]); };
  addAndMakeVisible(done_);
  done_.onClick = [this] {
    if (onClose) onClose();
  };
  done_.setMouseCursor(juce::MouseCursor::PointingHandCursor);

#if DJONT_HAS_STANDALONE_HOLDER
  if (auto* h = holder(processor_)) {
    devices_ = &h->deviceManager;
    // Inputs as single channels (dJONt is mono: it plays the first enabled
    // input), outputs as up to a stereo pair (it feeds both).
    selector_ = std::make_unique<juce::AudioDeviceSelectorComponent>(*devices_, 1, 2, 1, 2, false, false, false, false);
    viewport_.setViewedComponent(selector_.get(), false);
    viewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport_);

    hear_.setTitle("Hear yourself");
    hear_.setHelpText("Hear yourself: play dJONt's output while your input is live. Off mutes the input "
                      "(protects against feedback with a mic and speakers).");
    hear_.setToggleState(!static_cast<bool>(h->getMuteInputValue().getValue()), juce::dontSendNotification);
    hear_.onClick = [this, h] { h->getMuteInputValue().setValue(!hear_.getToggleState()); };
    addAndMakeVisible(hear_);
    startTimerHz(2);
  }
#endif
  updateLatency();
}

SettingsPanel::~SettingsPanel() { viewport_.setViewedComponent(nullptr, false); }

void SettingsPanel::syncLook(const juce::String& lookId) { look_.setIndex(lookId == "redfoil" ? 1 : 0); }

juce::String SettingsPanel::hint() const {
  if (devices_ == nullptr)
    return "Settings. In a DAW, the DAW owns the audio device, inputs, outputs and buffer; dJONt adds no latency.";
  return "Settings: pick the driver type (ASIO for the lowest latency), your interface, which input your guitar is on, "
         "the outputs, sample rate and buffer. Smaller buffer: less latency, more CPU.";
}

void SettingsPanel::timerCallback() {
  updateLatency();
#if DJONT_HAS_STANDALONE_HOLDER
  if (auto* h = holder(processor_))
    hear_.setToggleState(!static_cast<bool>(h->getMuteInputValue().getValue()), juce::dontSendNotification);
#endif
}

void SettingsPanel::updateLatency() {
  juce::String text;
  if (devices_ == nullptr) {
    text = "Running in a DAW: set the audio device, inputs, outputs and buffer in the DAW.\n\n"
           "dJONt adds 0 ms of latency. Sub's Clean octave trails your note by about 20 ms by design "
           "(the dry note doesn't); Synth mode has no delay.";
  } else if (auto* device = devices_->getCurrentAudioDevice()) {
    const double rate = device->getCurrentSampleRate();
    const auto ms = [rate](int samples) { return rate > 0 ? 1000.0 * samples / rate : 0.0; };
    const int buffer = device->getCurrentBufferSizeSamples();
    const int in = device->getInputLatencyInSamples(), out = device->getOutputLatencyInSamples();
    text << device->getTypeName() << ": " << device->getName() << "\n"
         << juce::String(rate / 1000.0, 1) << " kHz · buffer " << buffer << " (" << juce::String(ms(buffer), 1)
         << " ms)\n"
         << "In " << juce::String(ms(in), 1) << " + out " << juce::String(ms(out), 1) << " ms · round trip ~"
         << juce::String(ms(in + out + buffer), 1) << " ms\n"
         << "dJONt itself adds 0 ms.";
  } else {
    text = "No audio device is open. Pick a driver type and device on the right.";
  }
  if (text != latency_) {
    latency_ = text;
    repaint();
  }
}

void SettingsPanel::paint(juce::Graphics& g) {
  const auto& L = DjontLookAndFeel::lookOf(*this);
  const auto box = getLocalBounds().toFloat().reduced(0.5f);
  g.setColour(L.bg.withAlpha(0.95f));
  g.fillRoundedRectangle(box, 6.0f);
  g.setColour(L.plateBorder.withAlpha(0.5f));
  g.drawRoundedRectangle(box, 6.0f, 1.0f);

  g.setColour(L.ink);
  g.setFont(DjontLookAndFeel::display(*this, 30.0f));
  g.drawText("SETTINGS", 18, 10, 220, 36, juce::Justification::centredLeft, false);
  g.setFont(DjontLookAndFeel::label(10.0f));
  g.setColour(L.ink.withAlpha(0.7f));
  g.drawText("LOOK", 18, 52, 200, 14, juce::Justification::centredLeft, false);
  g.drawText(devices_ != nullptr ? "LATENCY" : "AUDIO", 18, 106, 220, 14, juce::Justification::centredLeft, false);

  g.setColour(L.hintText);
  g.setFont(juce::Font(juce::FontOptions(12.5f)));
  const int textBottom = devices_ != nullptr ? hear_.getY() - 4 : getHeight() - 10;
  g.drawFittedText(latency_, 18, 126, 240, juce::jmax(20, textBottom - 126), juce::Justification::topLeft, 8, 1.0f);

  if (devices_ == nullptr) {
    // DAW: a note where the device list would be.
    auto r = juce::Rectangle<int>(280, 18, getWidth() - 298, getHeight() - 36);
    g.setColour(L.plate);
    g.fillRoundedRectangle(r.toFloat(), 6.0f);
    g.setColour(L.ink.withAlpha(0.8f));
    g.setFont(juce::Font(juce::FontOptions(14.0f)));
    g.drawFittedText("Audio interface, ASIO driver, inputs, outputs, sample rate and buffer size are set by your "
                     "DAW when dJONt runs as a plugin.\n\nOpen dJONt.exe (the standalone app) to pick them here.",
                     r.reduced(20), juce::Justification::centredLeft, 6, 1.0f);
  } else {
    g.setColour(L.plateBorder);
    g.drawVerticalLine(266, 16.0f, static_cast<float>(getHeight() - 16));
  }
}

void SettingsPanel::resized() {
  look_.setTopLeftPosition(18, 70);
  done_.setBounds(166, 16, 84, 26);
  hear_.setBounds(14, getHeight() - 34, 240, 26);
  if (selector_ != nullptr) {
    viewport_.setBounds(276, 8, getWidth() - 284, getHeight() - 16);
    selector_->setSize(viewport_.getWidth() - viewport_.getScrollBarThickness() - 4, juce::jmax(selector_->getHeight(), 300));
  }
}

// ---------------------------------------------------------------- editor
DjontEditor::DjontEditor(DjontProcessor& processor)
    : juce::AudioProcessorEditor(processor),
      processor_(processor),
      input_(processor.params, "input", "Input", 44, "Input: level into dJONt."),
      output_(processor.params, "output", "Output", 44, "Output: level out of dJONt."),
      settings_(processor, [this](const juce::String& id) { setLook(id); }) {
  setLookAndFeel(&lnf_);
  input_.setBipolar();
  output_.setBipolar();
  addAndMakeVisible(input_);
  addAndMakeVisible(output_);
  addAndMakeVisible(gear_);
  gear_.onClick = [this] { showSettings(!settings_.isVisible()); };

  auto& s = processor.params;
  auto tight = std::make_unique<Module>(s, "TIGHT", "tightOn", "Pick-attack punch for palm mutes.");
  tight->addKnob(std::make_unique<Knob>(s, "tightAmount", "Amount", 58,
                                        "Tight · Amount: how hard each pick attack is pushed, and how much the "
                                        "ring-out is trimmed."));
  tight->addKnob(std::make_unique<Knob>(s, "tightFreq", "Freq", 58,
                                        "Tight · Freq: the band Tight listens to and punches up. Set it near your "
                                        "low string."));

  auto grit = std::make_unique<Module>(s, "GRIT", "gritOn", "Fuzz on the low end only.");
  grit->addKnob(std::make_unique<Knob>(s, "gritDrive", "Drive", 58, "Grit · Drive: how hard the low-end fuzz is pushed."));
  grit->addKnob(std::make_unique<Knob>(s, "gritBlend", "Blend", 58, "Grit · Blend: how much fuzz sits under your dry signal."));
  grit->addKnob(std::make_unique<Knob>(s, "gritFreq", "Freq", 44, "Grit · Freq: only the lows below this get fuzzed."));

  auto sub = std::make_unique<Module>(s, "SUB", "subOn", "An octave below, under your dry signal.");
  sub->setExtra(std::make_unique<ParamSegmented>(
      s, "subMode", juce::StringArray{"CLEAN", "SYNTH"},
      "Sub · Mode: Clean handles chords (about 20 ms behind your note); Synth is instant and buzzy, single notes only.",
      140));
  sub->addKnob(std::make_unique<Knob>(s, "subBlend", "Blend", 58, "Sub · Blend: how loud the octave below is."));
  sub->addKnob(std::make_unique<Knob>(s, "subTone", "Tone", 58, "Sub · Tone: low-pass on the octave; lower is rounder."));

  auto lofi = std::make_unique<Module>(s, "LO-FI", "lofiOn", "A radio / telephone band-pass.");
  lofi->addKnob(std::make_unique<Knob>(s, "lofiTone", "Tone", 58, "Lo-Fi · Tone: center of the radio band."));
  lofi->addKnob(std::make_unique<Knob>(s, "lofiBlend", "Blend", 58, "Lo-Fi · Blend: dry to fully filtered."));

  for (auto* m : {tight.get(), grit.get(), sub.get(), lofi.get()}) addAndMakeVisible(*m);
  modules_.push_back(std::move(tight));
  modules_.push_back(std::move(grit));
  modules_.push_back(std::move(sub));
  modules_.push_back(std::move(lofi));

  addChildComponent(settings_);
  settings_.onClose = [this] { showSettings(false); };

  hint_ = kDefaultHint;
  setLook(processor.params.state.getProperty(kLookProp, "chrome").toString());
  setSize(kWidth, kHeight);
  startTimerHz(30);
}

DjontEditor::~DjontEditor() { setLookAndFeel(nullptr); }

void DjontEditor::setLook(const juce::String& lookId) {
  const auto& look = lookFor(lookId);
  processor_.params.state.setProperty(kLookProp, look.id, nullptr);
  lnf_.setLook(look);
  settings_.syncLook(look.id);
  photo_ = loadPhoto(look.id);
  photoScaled_ = {};
  sendLookAndFeelChange();
  repaint();
}

void DjontEditor::showSettings(bool show) {
  settings_.setVisible(show);
  gear_.setToggleState(show, juce::dontSendNotification);
  for (auto& m : modules_) m->setVisible(!show);
  if (show) settings_.toFront(false);
  repaint();
}

void DjontEditor::timerCallback() {
  const float db = processor_.outputPeakDb.load();
  if (std::abs(db - meterDb_) > 0.2f) {
    meterDb_ = db;
    repaint(meterBox_);
  }
  updateHint();
}

void DjontEditor::updateHint() {
  juce::String text = kDefaultHint;
  auto* c = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
  if (c != nullptr && (c == this || isParentOf(c))) {
    for (; c != nullptr && c != this; c = c->getParentComponent()) {
      if (auto* src = dynamic_cast<HintSource*>(c)) {
        text = src->hint();
        break;
      }
      if (c->getHelpText().isNotEmpty()) {
        text = c->getHelpText();
        break;
      }
    }
  }
  if (text != hint_) {
    hint_ = text;
    repaint(hintBox_);
  }
}

void DjontEditor::paint(juce::Graphics& g) {
  const auto& L = lnf_.look();
  g.fillAll(L.bg);

  // The photo, covering the editor; rendered once per size / look / scale.
  if (photo_.isValid()) {
    const float scale = juce::jmax(1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (!photoScaled_.isValid() || photoScaledAt_ != scale) {
      const int w = juce::roundToInt(static_cast<float>(getWidth()) * scale);
      const int h = juce::roundToInt(static_cast<float>(getHeight()) * scale);
      juce::Image out(juce::Image::RGB, juce::jmax(1, w), juce::jmax(1, h), true);
      juce::Graphics sg(out);
      const float sw = static_cast<float>(photo_.getWidth()), sh = static_cast<float>(photo_.getHeight());
      const float k = juce::jmax(static_cast<float>(w) / sw, static_cast<float>(h) / sh);
      sg.drawImageTransformed(photo_, juce::AffineTransform::scale(k).translated((static_cast<float>(w) - sw * k) / 2.0f,
                                                                                  (static_cast<float>(h) - sh * k) / 2.0f),
                              false);
      photoScaled_ = out;
      photoScaledAt_ = scale;
    }
    g.drawImage(photoScaled_, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
  }

  // Wordmark: d JON t, the name in the accent, with a soft shadow for the photo.
  auto font = DjontLookAndFeel::display(*this, 70.0f);
  g.setFont(font);
  const std::pair<const char*, juce::Colour> parts[] = {{"d", L.ink}, {"JON", L.accent}, {"t", L.ink}};
  for (int pass = 0; pass < 2; ++pass) {
    float x = static_cast<float>(kPad);
    for (const auto& [text, colour] : parts) {
      const float w = juce::GlyphArrangement::getStringWidth(font, text);
      g.setColour(pass == 0 ? juce::Colours::black.withAlpha(0.45f) : colour);
      const float off = pass == 0 ? 2.0f : 0.0f;
      g.drawText(text, juce::Rectangle<float>(x + off, 16.0f + off, w + 2, 80.0f), juce::Justification::topLeft, false);
      x += w;
    }
  }

  // Header plate behind Input / Output / meter / gear.
  g.setColour(L.plate);
  g.fillRoundedRectangle(ioPlate_.toFloat(), 6.0f);
  g.setColour(L.plateBorder);
  g.drawRoundedRectangle(ioPlate_.toFloat().reduced(0.5f), 6.0f, 1.0f);

  // Rule under the header.
  g.setColour(L.rule);
  g.fillRect(kPad, kHeaderH, getWidth() - 2 * kPad, kRule);

  // Output meter.
  g.setColour(L.track);
  g.fillRect(meterBox_);
  const float level = juce::jlimit(0.0f, 1.0f, (meterDb_ + 60.0f) / 60.0f);
  const auto filled = meterBox_.toFloat().withTop(static_cast<float>(meterBox_.getBottom()) - level * static_cast<float>(meterBox_.getHeight()));
  g.setColour(meterDb_ > -1.0f ? juce::Colour(0xffd7261e) : L.accent);
  g.fillRect(filled);

  // Hint bar.
  g.setColour(L.hintBg);
  g.fillRect(hintBox_);
  g.setColour(L.rule);
  g.fillRect(hintBox_.getX(), hintBox_.getY(), hintBox_.getWidth(), 1);
  g.setColour(L.hintText);
  g.setFont(juce::Font(juce::FontOptions(12.5f)));
  g.drawText(hint_, hintBox_.reduced(kPad, 0), juce::Justification::centredLeft, true);
}

void DjontEditor::resized() {
  hintBox_ = {0, getHeight() - kHintH, getWidth(), kHintH};
  const int right = getWidth() - kPad;
  // Header, right to left: gear, meter, Output, Input, on one plate.
  gear_.setTopRightPosition(right - 8, (kHeaderH - gear_.getHeight()) / 2);
  meterBox_ = {gear_.getX() - 18, (kHeaderH - 60) / 2, 9, 60};
  output_.setTopRightPosition(meterBox_.getX() - 10, (kHeaderH - output_.getHeight()) / 2 + 2);
  input_.setTopRightPosition(output_.getX() - 2, output_.getY());
  ioPlate_ = juce::Rectangle<int>(input_.getX() - 6, 14, right - input_.getX() + 6, kHeaderH - 28);

  const int top = kBodyTop + 10;
  const int bottom = hintBox_.getY() - 10;
  const int left = 18, gap = 10;
  const int n = static_cast<int>(modules_.size());
  const int w = (getWidth() - 2 * left - gap * (n - 1)) / n;
  for (int i = 0; i < n; ++i) modules_[static_cast<size_t>(i)]->setBounds(left + i * (w + gap), top, w, bottom - top);
  settings_.setBounds(left, top, getWidth() - 2 * left, bottom - top);
  photoScaled_ = {};
}

}  // namespace djont
