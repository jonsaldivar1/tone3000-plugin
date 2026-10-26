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
  setColour(juce::TextEditor::backgroundColourId, panel);
  setColour(juce::TextEditor::textColourId, look.ink);
  setColour(juce::TextEditor::outlineColourId, look.accent);
  setColour(juce::TextEditor::focusedOutlineColourId, look.accent);
  setColour(juce::TextEditor::highlightColourId, look.accent.withAlpha(0.4f));
  setColour(juce::CaretComponent::caretColourId, look.accent);
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
  getProperties().set(kParamIdProp, id);
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
    : LeftClickOnly<juce::Button>(name), name_(name), description_(std::move(description)) {
  getProperties().set(kParamIdProp, id);
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
  if (e.mods.isPopupMenu()) return;
  const int i = juce::jlimit(0, labels_.size() - 1, e.x * labels_.size() / juce::jmax(1, getWidth()));
  setIndex(i);
  if (onSelect) onSelect(i);
}

ParamSegmented::ParamSegmented(juce::AudioProcessorValueTreeState& state, const juce::String& id,
                               juce::StringArray labels, juce::String description, int width)
    : Segmented(std::move(labels), std::move(description), width) {
  setTitle(id);
  getProperties().set(kParamIdProp, id);
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

// ---------------------------------------------------------------- toggles
ParamToggle::ParamToggle(juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& label,
                         juce::String description, int width)
    : LeftClickOnly<juce::Button>(label), description_(std::move(description)) {
  setClickingTogglesState(true);
  setTitle(label);
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, id, *this);
  getProperties().set(kParamIdProp, id);
  setSize(width, 24);
}

void ParamToggle::paintButton(juce::Graphics& g, bool highlighted, bool) {
  const auto& L = DjontLookAndFeel::lookOf(*this);
  const auto box = getLocalBounds().toFloat().reduced(1.0f);
  const bool on = getToggleState();
  if (on) {
    g.setColour(L.accent);
    g.fillRect(box);
  }
  g.setColour(on ? juce::Colours::black : L.ink.withAlpha(highlighted ? 1.0f : 0.85f));
  g.setFont(DjontLookAndFeel::label(10.0f));
  g.drawText(getButtonText(), box, juce::Justification::centred, false);
  g.setColour(on ? L.accent : L.ink);
  g.drawRect(box, 2.0f);
}

Chip::Chip(const juce::String& label, juce::String description, int width)
    : LeftClickOnly<juce::Button>(label), description_(std::move(description)) {
  setTitle(label);
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  setSize(width, 24);
}

void Chip::paintButton(juce::Graphics& g, bool highlighted, bool down) {
  const auto& L = DjontLookAndFeel::lookOf(*this);
  const auto box = getLocalBounds().toFloat().reduced(1.0f);
  if (down || highlighted) {
    g.setColour(L.ink.withAlpha(down ? 0.3f : 0.14f));
    g.fillRect(box);
  }
  g.setColour(L.ink);
  g.drawRect(box, 1.5f);
  g.setFont(DjontLookAndFeel::label(10.0f));
  g.drawText(getButtonText(), box, juce::Justification::centred, false);
}

// ---------------------------------------------------------------- wham pedal
WhamPedal::WhamPedal(DjontProcessor& processor)
    : processor_(processor), pedal_(processor.params.getParameter("whamPedal")) {
  setTitle("Wham pedal");
  getProperties().set(kParamIdProp, "whamPedal");
  attachment_ = std::make_unique<juce::ParameterAttachment>(*pedal_, [this](float v) {
    value_ = v / 100.0f;
    repaint();
  });
  attachment_->sendInitialUpdate();
}

bool WhamPedal::pedalMode() const { return processor_.params.getRawParameterValue("whamMode")->load() < 0.5f; }

float WhamPedal::shown() const {
  if (pedalMode()) return value_;
  return juce::jmax(0.0f, processor_.whamPosition.load());
}

void WhamPedal::tick() {
  setMouseCursor(pedalMode() ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
  const float now = std::round(shown() * 100.0f) / 100.0f;
  if (!juce::exactlyEqual(now, lastShown_)) {
    lastShown_ = now;
    repaint();
  }
}

juce::String WhamPedal::hint() const {
  const auto pos = juce::String(juce::roundToInt(shown() * 100.0f)) + "%";
  if (pedalMode())
    return juce::String(juce::CharPointer_UTF8("Wham \xc2\xb7 Pedal: drag up toward the toe, down to the heel (scroll works too; double-click: heel). Automate "
           "it or map a MIDI CC to Wham Pedal in your DAW.  ")) + pos;
  return juce::String(juce::CharPointer_UTF8("Wham \xc2\xb7 Pedal: Auto is moving it for you. Switch to PEDAL to play it by hand.  ")) + pos;
}

void WhamPedal::paint(juce::Graphics& g) {
  const auto& L = DjontLookAndFeel::lookOf(*this);
  const float pos = shown();
  const auto area = getLocalBounds().toFloat();
  constexpr float kLabelH = 12.0f;
  g.setFont(DjontLookAndFeel::label(8.5f));
  g.setColour(pos > 0.5f ? L.accent : L.ink.withAlpha(0.6f));
  g.drawText("TOE", area.withHeight(kLabelH), juce::Justification::centred, false);
  g.setColour(pos <= 0.5f ? L.ink : L.ink.withAlpha(0.6f));
  g.drawText("HEEL", area.withTrimmedTop(area.getHeight() - kLabelH), juce::Justification::centred, false);

  const auto housing = area.withTrimmedTop(kLabelH + 2).withTrimmedBottom(kLabelH + 2);
  g.setColour(L.bg.withAlpha(0.85f));
  g.fillRoundedRectangle(housing, 6.0f);
  g.setColour(L.plateBorder.withAlpha(0.9f));
  g.drawRoundedRectangle(housing.reduced(0.5f), 6.0f, 1.0f);

  const auto bed = housing.reduced(5.0f);
  g.setGradientFill(juce::ColourGradient(L.faceHi, bed.getX(), bed.getY(), L.faceLo, bed.getRight(), bed.getBottom(), false));
  g.fillRoundedRectangle(bed, 3.0f);
  const float barY = bed.getBottom() - bed.getHeight() * pos;
  g.setColour(L.accent.withAlpha(0.22f));
  g.fillRoundedRectangle(bed.withTop(barY), 3.0f);
  g.setColour(juce::Colours::black.withAlpha(0.55f));
  for (int i = 1; i < 10; ++i) {
    const float y = bed.getY() + bed.getHeight() * static_cast<float>(i) / 10.0f;
    g.drawLine(bed.getX() + 4.0f, y, bed.getRight() - 4.0f, y, 1.0f);
  }
  g.setColour(L.accent);
  g.fillRoundedRectangle(bed.getX() - 2.0f, barY - 2.0f, bed.getWidth() + 4.0f, 4.0f, 2.0f);
}

void WhamPedal::mouseDown(const juce::MouseEvent& e) {
  if (!pedalMode() || e.mods.isPopupMenu()) return;
  dragStart_ = value_;
  attachment_->beginGesture();
}

void WhamPedal::mouseDrag(const juce::MouseEvent& e) {
  if (!pedalMode() || e.mods.isPopupMenu()) return;
  const float travel = juce::jmax(20.0f, static_cast<float>(getHeight()) - 40.0f);
  const float k = e.mods.isShiftDown() ? 0.25f : 1.0f;
  const float v = juce::jlimit(0.0f, 1.0f, dragStart_ - k * static_cast<float>(e.getDistanceFromDragStartY()) / travel);
  attachment_->setValueAsPartOfGesture(v * 100.0f);
}

void WhamPedal::mouseUp(const juce::MouseEvent& e) {
  if (pedalMode() && !e.mods.isPopupMenu()) attachment_->endGesture();
}

void WhamPedal::mouseDoubleClick(const juce::MouseEvent&) {
  if (pedalMode()) attachment_->setValueAsCompleteGesture(0.0f);
}

void WhamPedal::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
  if (!pedalMode()) {
    Component::mouseWheelMove(e, wheel);
    return;
  }
  attachment_->setValueAsCompleteGesture(juce::jlimit(0.0f, 1.0f, value_ + wheel.deltaY * 0.5f) * 100.0f);
}

// ---------------------------------------------------------------- wham module
WhamModule::WhamModule(DjontProcessor& processor)
    : processor_(processor),
      power_(processor.params, "whamOn", "Wham",
             "A virtual whammy pedal: pitch sweeps between Heel and Toe (or the Interval's). Adds about 16 ms of "
             "latency while on in Custom / Whammy settings."),
      adv_("ADV", "Advanced: flip Wham over for its voicing (Grain, Buffer, Tone, Mix). BACK flips it back.", 44),
      mode_(processor.params, "whamMode", juce::StringArray{"PEDAL", "AUTO"},
            juce::String(juce::CharPointer_UTF8("Wham \xc2\xb7 Mode: Pedal, you move the pedal (drag, automation, MIDI). Auto, it moves itself at Rate in Shape.")),
            120),
      retrig_(processor.params, "whamRetrig", "RETRIG",
              juce::String(juce::CharPointer_UTF8("Wham \xc2\xb7 Retrig: Auto's sweep restarts at the heel on every pick, so it follows your playing. "
              "Rise + Retrig = one scoop per pick.")),
              70),
      algo_(processor.params, "whamAlgo", juce::StringArray{"CLASSIC", "CHORDS"},
            juce::String(juce::CharPointer_UTF8("Wham \xc2\xb7 Engine: Classic is the old-school shifter, clean on single notes with the glitchy warble on "
            "chords. Chords is the clean, chord-friendly engine.")),
            150),
      heel_(processor.params, "whamHeel", "Heel", 44, juce::String(juce::CharPointer_UTF8("Wham \xc2\xb7 Heel: the pitch with the pedal all the way up, \xc2\xb1""24 semitones."))),
      toe_(processor.params, "whamToe", "Toe", 44, juce::String(juce::CharPointer_UTF8("Wham \xc2\xb7 Toe: the pitch with the pedal all the way down, \xc2\xb1""24 semitones."))),
      rate_(processor.params, "whamRate", "Rate", 44, juce::String(juce::CharPointer_UTF8("Wham \xc2\xb7 Rate: how fast Auto sweeps, 0.1-10 Hz."))),
      shape_(processor.params, "whamShape", "Shape", 44,
             juce::String(juce::CharPointer_UTF8("Wham \xc2\xb7 Shape: Auto's sweep. Sine (smooth), Triangle, Rise (heel to toe, then snaps back), Square (jumps)."))),
      glide_(processor.params, "whamGlide", "Glide", 44,
             juce::String(juce::CharPointer_UTF8("Wham \xc2\xb7 Glide: how long the pedal takes to sweep heel to toe. Turn it up for a footswitch: press = "
             "sweep up, release = sweep back. 0: follows instantly."))),
      mix_(processor.params, "whamMix", "Mix", 44,
           juce::String(juce::CharPointer_UTF8("Wham \xc2\xb7 Mix: Whammy settings, dry to fully shifted. Harmony / Detune, the level of the added voice."))),
      grain_(processor.params, "whamGrain", "Grain", 44,
             juce::String(juce::CharPointer_UTF8("Wham \xc2\xb7 Grain (Classic): the shifter's sweep length. Short = tighter, more metallic warble on chords; "
             "long = smoother, wobblier. Longer adds a little latency."))),
      buffer_(processor.params, "whamBuffer", "Buffer", 44,
              juce::String(juce::CharPointer_UTF8("Wham \xc2\xb7 Buffer (Chords): the clean engine's buffer. 20 ms tightest, 60 ms smoothest on low notes."))),
      tone_(processor.params, "whamTone", "Tone", 44, juce::String(juce::CharPointer_UTF8("Wham \xc2\xb7 Tone: darkens the shifted signal. All the way up = off."))),
      mixBack_(processor.params, "whamMix", "Mix", 44,
               juce::String(juce::CharPointer_UTF8("Wham \xc2\xb7 Mix: Whammy settings, dry to fully shifted. Harmony / Detune, the level of the added voice."))),
      pedal_(processor),
      save_("SAVE", "Save: name the current Heel / Toe / Rate / Shape / Retrig / Mode as a preset. Same name replaces it.", 46),
      del_("DEL", "Delete: remove the selected preset (yours only; click twice).", 40) {
  addAndMakeVisible(power_);
  addAndMakeVisible(body_);
  addAndMakeVisible(adv_);
  adv_.onClick = [this] { setBack(!back_); };
  for (juce::Component* c : std::initializer_list<juce::Component*>{
           &mode_, &retrig_, &algo_, &interval_, &heel_, &toe_, &rate_, &shape_, &glide_, &mix_, &grain_, &buffer_,
           &tone_, &mixBack_, &pedal_, &presets_, &save_, &del_})
    body_.addAndMakeVisible(*c);
  // INTERVAL: the list under Whammy / Harmony / Detune headings; items in
  // parameter order, so the attachment's indices line up.
  {
    int id = 1;
    juce::String section;
    for (const auto& i : kWhamIntervals) {
      const juce::String name(i.name);
      const auto head = name.upToFirstOccurrenceOf(": ", false, false);
      if (name.contains(": ") && head != section) {
        if (section.isNotEmpty() || id > 1) interval_.addSeparator();
        interval_.addSectionHeading(head);
        section = head;
      }
      interval_.addItem(name.fromFirstOccurrenceOf(": ", false, false).isNotEmpty()
                            ? name.fromFirstOccurrenceOf(": ", false, false)
                            : name,
                        id++);
    }
    interval_.setTitle("Wham interval");
    interval_.setHelpText(juce::String(juce::CharPointer_UTF8("Wham \xc2\xb7 Interval: Custom uses your Heel / Toe. Whammy: the classic pedal's settings (no "
                          "shift at the heel, the interval at the toe). Harmony: your note plus a voice moving "
                          "between two intervals. Detune: a thickening double.")));
    interval_.getProperties().set(kParamIdProp, "whamInterval");
    intervalAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.params, "whamInterval", interval_);
  }
  body_.addChildComponent(name_);
  heel_.setBipolar();
  toe_.setBipolar();
  power_.onPower = [this](bool on) { setPowered(on); };
  setPowered(power_.getToggleState());

  presets_.setTextWhenNothingSelected("Presets");
  presets_.setTitle("Wham presets");
  presets_.setHelpText(juce::String(juce::CharPointer_UTF8("Wham \xc2\xb7 Presets: pick one to load its Heel / Toe / Rate / Shape / Retrig / Mode.")));
  presets_.onChange = [this] {
    if (const auto* p = store_.find(presets_.getText())) {
      WhamPresets::apply(*p, processor_.params);
      shownPreset_ = p->name;
    }
  };
  save_.onClick = [this] {
    if (name_.isVisible()) finishNaming(true);
    else startNaming();
  };
  del_.onClick = [this] {
    const auto* p = store_.find(presets_.getText());
    if (p == nullptr || p->factory) return;
    if (delArmedTicks_ == 0) {
      delArmedTicks_ = 90;  // ~3 s to confirm
      del_.setButtonText("SURE?");
      return;
    }
    store_.remove(p->name);
    delArmedTicks_ = 0;
    del_.setButtonText("DEL");
    refreshPresets();
  };
  name_.setTitle("Preset name");
  name_.setHelpText("Name this preset, then Enter (or SAVE). Esc cancels.");
  name_.setFont(juce::Font(juce::FontOptions(13.0f)));
  name_.onReturnKey = [this] { finishNaming(true); };
  name_.onEscapeKey = [this] { finishNaming(false); };
  name_.onFocusLost = [this] { finishNaming(false); };
  refreshPresets();
  syncVisibility(true);
  setSize(kWidth, 300);
}

void WhamModule::setPowered(bool on) { body_.setAlpha(on ? 1.0f : 0.38f); }

void WhamModule::setBack(bool back) {
  back_ = back;
  adv_.setButtonText(back_ ? "BACK" : "ADV");
  syncVisibility(true);
}

// Which controls show: the front or the back (ADV), and on the front, Auto's
// LFO (Rate, Shape, Retrig) or Pedal's Glide, and Custom's Heel / Toe or Mix.
void WhamModule::syncVisibility(bool force) {
  const bool autoMode = processor_.params.getRawParameterValue("whamMode")->load() > 0.5f;
  const bool custom = juce::roundToInt(processor_.params.getRawParameterValue("whamInterval")->load()) == 0;
  const int state = (back_ ? 4 : 0) + (autoMode ? 2 : 0) + (custom ? 1 : 0);
  if (state == shownState_ && !force) return;
  shownState_ = state;
  const bool front = !back_;
  for (juce::Component* c : std::initializer_list<juce::Component*>{&mode_, &algo_, &interval_, &pedal_})
    c->setVisible(front);
  retrig_.setVisible(front && autoMode);
  rate_.setVisible(front && autoMode);
  shape_.setVisible(front && autoMode);
  glide_.setVisible(front && !autoMode);
  heel_.setVisible(front && custom);
  toe_.setVisible(front && custom);
  mix_.setVisible(front && !custom);
  for (juce::Component* c : std::initializer_list<juce::Component*>{&grain_, &buffer_, &tone_, &mixBack_})
    c->setVisible(back_);
  repaint();
}

void WhamModule::refreshPresets() {
  presets_.clear(juce::dontSendNotification);
  int id = 1;
  bool userHeader = false;
  presets_.addSectionHeading("Factory");
  for (const auto& p : store_.all()) {
    if (!p.factory && !userHeader) {
      presets_.addSeparator();
      presets_.addSectionHeading("Yours");
      userHeader = true;
    }
    presets_.addItem(p.name, id++);
  }
  shownPreset_ = {};
  ticks_ = 0;
}

void WhamModule::startNaming() {
  const auto current = presets_.getText();
  const auto* p = store_.find(current);
  name_.setText(p != nullptr && !p->factory ? current : juce::String(), juce::dontSendNotification);
  name_.setTextToShowWhenEmpty("Preset name", DjontLookAndFeel::lookOf(*this).ink.withAlpha(0.45f));
  presets_.setVisible(false);
  name_.setVisible(true);
  name_.grabKeyboardFocus();
  save_.setButtonText("OK");
}

void WhamModule::finishNaming(bool save) {
  if (!name_.isVisible()) return;
  const auto name = name_.getText().trim();
  name_.setVisible(false);
  presets_.setVisible(true);
  save_.setButtonText("SAVE");
  if (!save || name.isEmpty()) return;
  if (const auto* existing = store_.find(name); existing != nullptr && existing->factory) {
    save_.setDescription("That's a factory preset's name: pick another to save yours.");
    return;
  }
  store_.save(name, processor_.params);
  refreshPresets();
  presets_.setText(name, juce::dontSendNotification);
  shownPreset_ = name;
}

void WhamModule::tick() {
  pedal_.tick();
  const bool autoMode = processor_.params.getRawParameterValue("whamMode")->load() > 0.5f;
  juce::ignoreUnused(autoMode);
  syncVisibility();
  if (delArmedTicks_ > 0 && --delArmedTicks_ == 0) del_.setButtonText("DEL");
  // Every half second: show which preset (if any) the knobs match now.
  if (++ticks_ % 15 == 0 && !name_.isVisible() && !presets_.isPopupActive()) {
    const auto match = store_.matching(processor_.params);
    if (match != shownPreset_) {
      shownPreset_ = match;
      if (match.isEmpty()) presets_.setSelectedId(0, juce::dontSendNotification);
      else presets_.setText(match, juce::dontSendNotification);
    }
  }
}

void WhamModule::paint(juce::Graphics& g) {
  const auto& L = DjontLookAndFeel::lookOf(*this);
  const auto box = getLocalBounds().toFloat().reduced(0.5f);
  g.setColour(L.plate);
  g.fillRoundedRectangle(box, 6.0f);
  g.setColour(L.plateBorder);
  g.drawRoundedRectangle(box, 6.0f, 1.0f);
  g.setColour(L.ink);
  g.setFont(DjontLookAndFeel::display(*this, 34.0f));
  g.drawText("WHAM", 14, 8, getWidth() - 60, 40, juce::Justification::centredLeft, false);
  if (back_) {
    g.setFont(DjontLookAndFeel::label(10.0f));
    g.setColour(L.ink.withAlpha(0.7f));
    g.drawText("VOICING", 14, 52, 200, 16, juce::Justification::centredLeft, false);
  }
}

void WhamModule::resized() {
  power_.setTopRightPosition(getWidth() - 12, 15);
  adv_.setTopRightPosition(power_.getX() - 10, 16);
  body_.setBounds(0, 52, getWidth(), getHeight() - 52);
  const int w = body_.getWidth(), h = body_.getHeight();
  const int presetY = h - 34;
  const int pedalW = 48;
  pedal_.setBounds(w - 12 - pedalW, 0, pedalW, presetY - 8);
  const int left = 12, right = pedal_.getX() - 8;
  mode_.setTopLeftPosition(left, 0);
  retrig_.setTopLeftPosition(mode_.getRight() + 6, 0);
  algo_.setTopLeftPosition(left, 32);
  interval_.setBounds(left, 64, right - left, 24);
  // Two rows of knobs, centred left of the pedal.
  const int knobW = heel_.getWidth(), gap = 6;
  const int x0 = left + (right - left - (2 * knobW + gap)) / 2, x1 = x0 + knobW + gap;
  const int rowH = heel_.getHeight() + 4;
  const int y0 = 96 + juce::jmax(0, (presetY - 96 - 2 * rowH) / 2);
  heel_.setTopLeftPosition(x0, y0);
  toe_.setTopLeftPosition(x1, y0);
  mix_.setTopLeftPosition(x0 + (knobW + gap) / 2, y0);
  rate_.setTopLeftPosition(x0, y0 + rowH);
  shape_.setTopLeftPosition(x1, y0 + rowH);
  glide_.setTopLeftPosition(x0 + (knobW + gap) / 2, y0 + rowH);
  // Back: Grain / Buffer over Tone / Mix, centred across the module.
  const int bx0 = (w - (2 * knobW + 24)) / 2, bx1 = bx0 + knobW + 24;
  const int by0 = 30 + juce::jmax(0, (presetY - 30 - 2 * rowH) / 2);
  grain_.setTopLeftPosition(bx0, by0);
  buffer_.setTopLeftPosition(bx1, by0);
  tone_.setTopLeftPosition(bx0, by0 + rowH);
  mixBack_.setTopLeftPosition(bx1, by0 + rowH);
  del_.setTopRightPosition(w - 12, presetY);
  save_.setTopRightPosition(del_.getX() - 6, presetY);
  presets_.setBounds(12, presetY, save_.getX() - 18, 24);
  name_.setBounds(presets_.getBounds());
}

// ---------------------------------------------------------------- hold
HoldButton::HoldButton(juce::AudioProcessorValueTreeState& state) : state_(state) {
  setTitle("Hold");
  getProperties().set(kParamIdProp, "holdOn");
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  attachment_ = std::make_unique<juce::ParameterAttachment>(*state.getParameter("holdOn"), [this](float v) {
    on_ = v > 0.5f;
    repaint();
  });
  attachment_->sendInitialUpdate();
  setSize(110, 44);
}

bool HoldButton::latch() const { return state_.getRawParameterValue("holdLatch")->load() > 0.5f; }

juce::String HoldButton::hint() const {
  return juce::String("Hold: ") + (on_ ? "HOLDING. " : "") +
         (latch() ? "click to hold what's ringing, click again to let go."
                  : "hold the mouse down to hold what's ringing; let go to release.") +
         " Right-click to put it on a footswitch (MIDI learn).";
}

void HoldButton::paint(juce::Graphics& g) {
  const auto& L = DjontLookAndFeel::lookOf(*this);
  const auto box = getLocalBounds().toFloat().reduced(1.5f);
  if (on_) {
    g.setColour(L.accent.withAlpha(0.25f));
    g.fillRoundedRectangle(box.expanded(1.0f), 8.0f);
    g.setColour(L.accent);
    g.fillRoundedRectangle(box.reduced(3.0f), 6.0f);
  } else {
    g.setColour(L.bg.withAlpha(0.6f));
    g.fillRoundedRectangle(box, 8.0f);
  }
  g.setColour(on_ ? L.accent : L.ink);
  g.drawRoundedRectangle(box, 8.0f, 2.0f);
  g.setColour(on_ ? juce::Colours::black : L.ink);
  g.setFont(DjontLookAndFeel::display(*this, 24.0f));
  g.drawText("HOLD", box, juce::Justification::centred, false);
}

void HoldButton::mouseDown(const juce::MouseEvent& e) {
  if (e.mods.isPopupMenu()) return;
  attachment_->setValueAsCompleteGesture(latch() ? (on_ ? 0.0f : 1.0f) : 1.0f);
}

void HoldButton::mouseUp(const juce::MouseEvent& e) {
  if (e.mods.isPopupMenu() || latch()) return;
  attachment_->setValueAsCompleteGesture(0.0f);
}

// ---------------------------------------------------------------- sustain module
SustainModule::SustainModule(DjontProcessor& processor)
    : processor_(processor),
      power_(processor.params, "sustainOn", "Sustain",
             "Hold (a freeze, like a piano's sustain pedal) or Sustainer (like a sustainer pickup)."),
      adv_("ADV", "Advanced: flip Sustain over for its voicing. BACK flips it back.", 44),
      page_(processor.params, "sustainMode", juce::StringArray{"HOLD", "SUSTAINER"},
            juce::String(juce::CharPointer_UTF8("Sustain \xc2\xb7 Mode: Hold freezes what's ringing so you can play over it. Sustainer keeps each note from "
            "dying, like a Sustainiac.")),
            200),
      hold_(processor.params),
      latch_(processor.params, "holdLatch", "LATCH",
             juce::String(juce::CharPointer_UTF8("Hold \xc2\xb7 Latch: on, one press holds and the next lets go (use with a momentary footswitch). Off, it "
             "holds only while pressed (or use a latching footswitch).")),
             70),
      holdType_(processor.params, "holdType", juce::StringArray{"CHORD", "PILE-UP"},
                juce::String(juce::CharPointer_UTF8("Hold \xc2\xb7 Type: Chord holds what was ringing when you pressed (a piano's middle pedal). Pile-up keeps "
                "adding everything you play while held (the right pedal).")),
                190),
      fade_(processor.params, "holdFade", "Fade", 44,
            juce::String(juce::CharPointer_UTF8("Hold \xc2\xb7 Fade: how fast the held sound swells in and dies away when you let go."))),
      level_(processor.params, "holdLevel", "Level", 44, juce::String(juce::CharPointer_UTF8("Hold \xc2\xb7 Level: the held sound against your live playing."))),
      sustainerType_(processor.params, "sustainerType", juce::StringArray{"FUND", "HARM", "MIX"},
                     juce::String(juce::CharPointer_UTF8("Sustainer \xc2\xb7 Type: Fund keeps the note you played. Harm blooms into the Interval (ADV), like a "
                     "sustainer's harmonic mode. Mix blooms halfway.")),
                     190),
      amount_(processor.params, "sustainerAmount", "Sustain", 58,
              juce::String(juce::CharPointer_UTF8("Sustainer \xc2\xb7 Sustain: how much it holds the note up as the string dies (up to 40 dB of make-up)."))),
      bloom_(processor.params, "sustainerBloom", "Bloom", 58,
             juce::String(juce::CharPointer_UTF8("Sustainer \xc2\xb7 Bloom: how long Harm / Mix takes to bloom after each pick."))),
      shimmer_(processor.params, "holdShimmer", "Shimmer", 44,
               juce::String(juce::CharPointer_UTF8("Hold \xc2\xb7 Shimmer: adds movement to the held sound so it's less of a static pad. 0 = steady."))),
      lowCut_(processor.params, "holdLowCut", "Low Cut", 44,
              juce::String(juce::CharPointer_UTF8("Hold \xc2\xb7 Low Cut: thins the held sound's lows so it doesn't muddy what you play over it. Bottom = off."))),
      holdTone_(processor.params, "holdTone", "Tone", 44, juce::String(juce::CharPointer_UTF8("Hold \xc2\xb7 Tone: darkens the held sound. Top = off."))),
      sustainerInterval_(processor.params, "sustainerInterval", juce::StringArray{"OCT", "5TH", "12TH", "2 OCT"},
                         juce::String(juce::CharPointer_UTF8("Sustainer \xc2\xb7 Interval: what Harm / Mix bloom into. Octave is the classic; the 12th (an "
                         "octave and a 5th) is the other harmonic a sustainer pickup likes to jump to.")),
                         210),
      floor_(processor.params, "sustainerFloor", "Floor", 44,
             juce::String(juce::CharPointer_UTF8("Sustainer \xc2\xb7 Floor: how quiet a note gets before it lets go. Lower = longer tails, more noise."))),
      response_(processor.params, "sustainerResponse", "Response", 44,
                juce::String(juce::CharPointer_UTF8("Sustainer \xc2\xb7 Response: how fast it rides the level. Fast = tight, can pump; slow = smooth, swells."))),
      sustainerTone_(processor.params, "sustainerTone", "Tone", 44,
                     juce::String(juce::CharPointer_UTF8("Sustainer \xc2\xb7 Tone: darkens the sustained sound (tames the noise it brings up). Top = off."))) {
  addAndMakeVisible(power_);
  addAndMakeVisible(adv_);
  adv_.onClick = [this] { setBack(!back_); };
  addAndMakeVisible(body_);
  for (juce::Component* c : std::initializer_list<juce::Component*>{
           &page_, &hold_, &latch_, &holdType_, &fade_, &level_, &sustainerType_, &amount_, &bloom_, &shimmer_,
           &lowCut_, &holdTone_, &sustainerInterval_, &floor_, &response_, &sustainerTone_})
    body_.addAndMakeVisible(*c);
  power_.onPower = [this](bool on) { setPowered(on); };
  setPowered(power_.getToggleState());
  syncVisibility(true);
}

void SustainModule::setPowered(bool on) { body_.setAlpha(on ? 1.0f : 0.38f); }

void SustainModule::setBack(bool back) {
  back_ = back;
  adv_.setButtonText(back_ ? "BACK" : "ADV");
  syncVisibility(true);
}

void SustainModule::syncVisibility(bool force) {
  const int page = processor_.params.getRawParameterValue("sustainMode")->load() > 0.5f ? 1 : 0;
  const int state = page + (back_ ? 2 : 0);
  if (state == shownState_ && !force) return;
  shownState_ = state;
  const bool holdFront = page == 0 && !back_, susFront = page == 1 && !back_;
  const bool holdBack = page == 0 && back_, susBack = page == 1 && back_;
  for (juce::Component* c : std::initializer_list<juce::Component*>{&hold_, &latch_, &holdType_, &fade_, &level_})
    c->setVisible(holdFront);
  for (juce::Component* c : std::initializer_list<juce::Component*>{&sustainerType_, &amount_, &bloom_})
    c->setVisible(susFront);
  for (juce::Component* c : std::initializer_list<juce::Component*>{&shimmer_, &lowCut_, &holdTone_})
    c->setVisible(holdBack);
  for (juce::Component* c : std::initializer_list<juce::Component*>{&sustainerInterval_, &floor_, &response_,
                                                                     &sustainerTone_})
    c->setVisible(susBack);
  repaint();
}

void SustainModule::tick() { syncVisibility(); }

void SustainModule::paint(juce::Graphics& g) {
  const auto& L = DjontLookAndFeel::lookOf(*this);
  const auto box = getLocalBounds().toFloat().reduced(0.5f);
  g.setColour(L.plate);
  g.fillRoundedRectangle(box, 6.0f);
  g.setColour(L.plateBorder);
  g.drawRoundedRectangle(box, 6.0f, 1.0f);
  g.setColour(L.ink);
  g.setFont(DjontLookAndFeel::display(*this, 34.0f));
  g.drawText("SUSTAIN", 14, 8, getWidth() - 60, 40, juce::Justification::centredLeft, false);
  if (back_) {
    g.setFont(DjontLookAndFeel::label(10.0f));
    g.setColour(L.ink.withAlpha(0.7f));
    g.drawText(shownState_ % 2 == 0 ? "HOLD VOICING" : "SUSTAINER VOICING", 0, 52 + 30, getWidth(), 16,
               juce::Justification::centred, false);
  }
}

void SustainModule::resized() {
  power_.setTopRightPosition(getWidth() - 12, 15);
  adv_.setTopRightPosition(power_.getX() - 10, 16);
  body_.setBounds(0, 52, getWidth(), getHeight() - 52);
  const int w = body_.getWidth();
  const int cx = w / 2;
  page_.setTopLeftPosition(cx - page_.getWidth() / 2, 0);
  // Hold page.
  holdType_.setTopLeftPosition(cx - holdType_.getWidth() / 2, 40);
  const int rowW = hold_.getWidth() + 10 + latch_.getWidth();
  hold_.setTopLeftPosition(cx - rowW / 2, 82);
  latch_.setTopLeftPosition(hold_.getRight() + 10, 82 + (hold_.getHeight() - latch_.getHeight()) / 2);
  const int knobsW = fade_.getWidth() * 2 + 10;
  fade_.setTopLeftPosition(cx - knobsW / 2, 148);
  level_.setTopLeftPosition(fade_.getRight() + 10, 148);
  // Sustainer page.
  sustainerType_.setTopLeftPosition(cx - sustainerType_.getWidth() / 2, 40);
  const int bigW = amount_.getWidth() * 2 + 10;
  amount_.setTopLeftPosition(cx - bigW / 2, 100);
  bloom_.setTopLeftPosition(amount_.getRight() + 10, 100);
  // Hold voicing: three knobs in a row.
  const int k = shimmer_.getWidth();
  const int threeW = 3 * k + 2 * 4;
  shimmer_.setTopLeftPosition(cx - threeW / 2, 60);
  lowCut_.setTopLeftPosition(shimmer_.getRight() + 4, 60);
  holdTone_.setTopLeftPosition(lowCut_.getRight() + 4, 60);
  // Sustainer voicing: the interval, then Floor / Response / Tone.
  sustainerInterval_.setTopLeftPosition(cx - sustainerInterval_.getWidth() / 2, 56);
  floor_.setTopLeftPosition(cx - threeW / 2, 100);
  response_.setTopLeftPosition(floor_.getRight() + 4, 100);
  sustainerTone_.setTopLeftPosition(response_.getRight() + 4, 100);
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
namespace {
juce::String whamLatencyLine(const DjontProcessor& p) {
  const int samples = p.getLatencySamples();
  const double rate = p.getSampleRate() > 0 ? p.getSampleRate() : 48000.0;
  if (samples <= 0) return "dJONt itself adds 0 ms (Wham off).";
  return "Wham adds " + juce::String(1000.0 * samples / rate, 1) + " ms while it's on.";
}
}  // namespace

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
    selector_ = std::make_unique<juce::AudioDeviceSelectorComponent>(*devices_, 1, 2, 1, 2, true, false, false, false);
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
    text = "Running in a DAW: set the audio device, inputs, outputs and buffer in the DAW.\n\n" +
           whamLatencyLine(processor_) +
           "\nSub's Clean octave trails your note by about 20 ms by design (the dry note doesn't).";
  } else if (auto* device = devices_->getCurrentAudioDevice()) {
    const double rate = device->getCurrentSampleRate();
    const auto ms = [rate](int samples) { return rate > 0 ? 1000.0 * samples / rate : 0.0; };
    const int buffer = device->getCurrentBufferSizeSamples();
    const int in = device->getInputLatencyInSamples(), out = device->getOutputLatencyInSamples();
    text << device->getTypeName() << ": " << device->getName() << "\n"
         << juce::String(rate / 1000.0, 1) << juce::String(juce::CharPointer_UTF8(" kHz \xc2\xb7 buffer ")) << buffer << " (" << juce::String(ms(buffer), 1)
         << " ms)\n"
         << "In " << juce::String(ms(in), 1) << " + out " << juce::String(ms(out), 1) << juce::String(juce::CharPointer_UTF8(" ms \xc2\xb7 round trip ~"))
         << juce::String(ms(in + out + buffer), 1) << " ms\n"
         << whamLatencyLine(processor_);
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

// ---------------------------------------------------------------- help (?)
HelpButton::HelpButton() : juce::Button("Manual") {
  setTitle("Manual");
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  setSize(30, 30);
}

juce::String HelpButton::hint() const {
  return "Manual: what each module does, how it works, the sound it's for, and settings to try.";
}

void HelpButton::paintButton(juce::Graphics& g, bool highlighted, bool down) {
  const auto& L = DjontLookAndFeel::lookOf(*this);
  const auto colour = getToggleState() || down ? L.accent : highlighted ? L.ink : L.ink.withAlpha(0.8f);
  const auto r = getLocalBounds().toFloat().reduced(3.0f);
  g.setColour(colour);
  g.drawEllipse(r, 2.0f);
  g.setFont(juce::Font(juce::FontOptions(17.0f, juce::Font::bold)));
  g.drawText("?", getLocalBounds().translated(0, 1), juce::Justification::centred, false);
}

// ---------------------------------------------------------------- manual
std::vector<ManualPanel::Section> ManualPanel::load() {
  std::vector<Section> out;
  const auto json = juce::JSON::parse(
      juce::String::fromUTF8(DjontBinary::manual_json, DjontBinary::manual_jsonSize));
  if (auto* sections = json["sections"].getArray()) {
    for (const auto& v : *sections) {
      Section s;
      s.id = v["id"].toString();
      s.title = v["title"].toString();
      s.tagline = v["tagline"].toString();
      s.what = v["what"].toString();
      s.how = v["how"].toString();
      s.sound = v["sound"].toString();
      if (auto* tries = v["try"].getArray())
        for (const auto& t : *tries) s.tries.add(t.toString());
      if (auto* controls = v["controls"].getArray())
        for (const auto& c : *controls)
          if (auto* pair = c.getArray(); pair != nullptr && pair->size() == 2)
            s.controls.emplace_back((*pair)[0].toString(), (*pair)[1].toString());
      out.push_back(std::move(s));
    }
  }
  return out;
}

ManualPanel::ManualPanel() : sections_(load()) {
  for (int i = 0; i < sectionCount(); ++i) {
    auto* tab = tabs_.add(new juce::TextButton(sections_[static_cast<size_t>(i)].title));
    tab->setClickingTogglesState(false);
    tab->setRadioGroupId(4711);
    tab->setMouseCursor(juce::MouseCursor::PointingHandCursor);
    tab->setHelpText(juce::String(juce::CharPointer_UTF8("Manual \xc2\xb7 ")) + sections_[static_cast<size_t>(i)].title + ": " +
                     sections_[static_cast<size_t>(i)].tagline);
    tab->onClick = [this, i] { showSection(i); };
    addAndMakeVisible(tab);
  }
  addAndMakeVisible(done_);
  done_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
  done_.onClick = [this] {
    if (onClose) onClose();
  };
  viewport_.setViewedComponent(&page_, false);
  viewport_.setScrollBarsShown(true, false);
  addAndMakeVisible(viewport_);
  showSection(0);
}

ManualPanel::~ManualPanel() { viewport_.setViewedComponent(nullptr, false); }

juce::String ManualPanel::hint() const {
  return "Manual: pick a topic on the left; scroll for more. DONE (or ?) goes back to the controls.";
}

void ManualPanel::showSection(int index) {
  if (sections_.empty()) return;
  current_ = juce::jlimit(0, sectionCount() - 1, index);
  for (int i = 0; i < tabs_.size(); ++i) tabs_[i]->setToggleState(i == current_, juce::dontSendNotification);
  const int width = juce::jmax(200, viewport_.getWidth() - viewport_.getScrollBarThickness() - 6);
  page_.build(sections_[static_cast<size_t>(current_)], width);
  viewport_.setViewPosition(0, 0);
}

void ManualPanel::lookAndFeelChanged() { showSection(current_); }

void ManualPanel::paint(juce::Graphics& g) {
  const auto& L = DjontLookAndFeel::lookOf(*this);
  const auto box = getLocalBounds().toFloat().reduced(0.5f);
  g.setColour(L.bg.withAlpha(0.95f));
  g.fillRoundedRectangle(box, 6.0f);
  g.setColour(L.plateBorder.withAlpha(0.5f));
  g.drawRoundedRectangle(box, 6.0f, 1.0f);
  g.setColour(L.ink);
  g.setFont(DjontLookAndFeel::display(*this, 30.0f));
  g.drawText("MANUAL", 18, 10, 140, 36, juce::Justification::centredLeft, false);
  g.setColour(L.plateBorder);
  g.drawVerticalLine(206, 16.0f, static_cast<float>(getHeight() - 16));
}

void ManualPanel::resized() {
  done_.setBounds(122, 16, 72, 26);
  const int step = juce::jmax(20, (getHeight() - 58) / juce::jmax(1, tabs_.size()));
  for (int i = 0; i < tabs_.size(); ++i) tabs_[i]->setBounds(14, 54 + i * step, 180, step - 3);
  viewport_.setBounds(220, 10, getWidth() - 228, getHeight() - 20);
  showSection(current_);
}

void ManualPanel::Page::build(const Section& s, int width) {
  const auto& L = DjontLookAndFeel::lookOf(*this);
  const auto body = juce::Font(juce::FontOptions(14.0f));
  const auto bold = juce::Font(juce::FontOptions(14.0f, juce::Font::bold));
  const auto head = DjontLookAndFeel::label(10.5f);
  const auto bodyInk = L.ink.withAlpha(0.88f);

  juce::AttributedString a;
  a.setWordWrap(juce::AttributedString::byWord);
  a.setLineSpacing(2.0f);
  a.append(s.title.toUpperCase() + "\n", DjontLookAndFeel::display(*this, 30.0f), L.ink);
  a.append(s.tagline + "\n", juce::Font(juce::FontOptions(15.0f)).italicised(), L.accent);
  const auto section = [&](const juce::String& heading, const juce::String& text) {
    if (text.isEmpty()) return;
    a.append("\n" + heading + "\n", head, L.ink.withAlpha(0.6f));
    a.append(text + "\n", body, bodyInk);
  };
  section("WHAT IT DOES", s.what);
  section("HOW IT WORKS", s.how);
  section("THE SOUND", s.sound);
  if (!s.tries.isEmpty()) {
    a.append("\nTRY THIS\n", head, L.ink.withAlpha(0.6f));
    for (int i = 0; i < s.tries.size(); ++i) {
      a.append(juce::String(i + 1) + ".  ", bold, L.accent);
      a.append(s.tries[i] + "\n", body, bodyInk);
    }
  }
  if (!s.controls.empty()) {
    a.append("\nCONTROLS\n", head, L.ink.withAlpha(0.6f));
    for (const auto& [name, text] : s.controls) {
      a.append(name + "   ", bold, L.ink);
      a.append(text + "\n", body, bodyInk);
    }
  }
  layout_.createLayout(a, static_cast<float>(width - 8));
  setSize(width, static_cast<int>(std::ceil(layout_.getHeight())) + 24);
  repaint();
}

void ManualPanel::Page::paint(juce::Graphics& g) {
  layout_.draw(g, getLocalBounds().toFloat().reduced(4.0f, 8.0f).withHeight(layout_.getHeight()));
}

// ---------------------------------------------------------------- editor
DjontView::DjontView(DjontProcessor& processor)
    : processor_(processor),
      input_(processor.params, "input", "Input", 44, "Input: level into dJONt."),
      output_(processor.params, "output", "Output", 44, "Output: level out of dJONt."),
      blend_(processor.params, "blend", "Blend", 44,
             "Blend: the whole board against your dry DI (after Hush). 100% is all dJONt; lower keeps your amp's own tone under the effects."),
      hushChip_("HUSH", "Hush: the de-noiser on your DI, before everything (zero latency). Opens its panel: on / off, "
                        "LEARN, Strength, Sensitivity, Release.", 74),
      wham_(processor),
      sustain_(processor),
      settings_(processor, [this](const juce::String& id) { setLook(id); }),
      hushPanel_(processor.params, processor.hush,
                 [](const juce::Component& c, float px) { return DjontLookAndFeel::display(c, px); }, true) {
  setLookAndFeel(&lnf_);
  input_.setBipolar();
  output_.setBipolar();
  addAndMakeVisible(input_);
  addAndMakeVisible(output_);
  addAndMakeVisible(blend_);
  addAndMakeVisible(gear_);
  gear_.onClick = [this] { showSettings(!settings_.isVisible()); };
  addAndMakeVisible(help_);
  help_.onClick = [this] { showManual(!manual_.isVisible()); };
  addAndMakeVisible(hushChip_);
  hushChip_.onClick = [this] { showHush(!hushPanel_.isVisible()); };

  auto& s = processor.params;
  auto tight = std::make_unique<Module>(s, "TIGHT", "tightOn", "Pick-attack punch for palm mutes, or a tight boost into your amp.");
  tight->setExtra(std::make_unique<ParamSegmented>(
      s, "tightMode", juce::StringArray{"PUNCH", "FOCUS"},
      "Tight: PUNCH pushes each pick attack and trims the ring-out. FOCUS is a tight boost: low cut, mid hump and a "
      "little clipping to hit a high-gain amp harder.",
      140));
  tight->addKnob(std::make_unique<Knob>(s, "tightAmount", "Amount", 58,
                                        juce::String(juce::CharPointer_UTF8("Tight \xc2\xb7 Amount. PUNCH: how hard each pick attack is pushed and the "
                                        "ring-out trimmed. FOCUS: how hard the boost drives your amp."))));
  tight->addKnob(std::make_unique<Knob>(s, "tightFreq", "Freq", 58,
                                        juce::String(juce::CharPointer_UTF8("Tight \xc2\xb7 Freq. PUNCH: the band it listens to (near your low string). "
                                        "FOCUS: the low cut; higher is tighter, 150 to 300 Hz is the classic range."))));

  auto grit = std::make_unique<Module>(s, "GRIT", "gritOn", "Fuzz on the low end only.");
  grit->addKnob(std::make_unique<Knob>(s, "gritDrive", "Drive", 58, juce::String(juce::CharPointer_UTF8("Grit \xc2\xb7 Drive: how hard the low-end fuzz is pushed."))));
  grit->addKnob(std::make_unique<Knob>(s, "gritBlend", "Blend", 58, juce::String(juce::CharPointer_UTF8("Grit \xc2\xb7 Blend: how much fuzz sits under your dry signal."))));
  grit->addKnob(std::make_unique<Knob>(s, "gritFreq", "Freq", 44, juce::String(juce::CharPointer_UTF8("Grit \xc2\xb7 Freq: only the lows below this get fuzzed."))));

  auto sub = std::make_unique<Module>(s, "SUB", "subOn", "An octave below, under your dry signal.");
  sub->setExtra(std::make_unique<ParamSegmented>(
      s, "subMode", juce::StringArray{"CLEAN", "SYNTH"},
      juce::String(juce::CharPointer_UTF8("Sub \xc2\xb7 Mode: Clean handles chords (about 20 ms behind your note); Synth is instant and buzzy, single notes only.")),
      140));
  sub->addKnob(std::make_unique<Knob>(s, "subBlend", "Blend", 58, juce::String(juce::CharPointer_UTF8("Sub \xc2\xb7 Blend: how loud the octave below is."))));
  sub->addKnob(std::make_unique<Knob>(s, "subTone", "Tone", 58, juce::String(juce::CharPointer_UTF8("Sub \xc2\xb7 Tone: low-pass on the octave; lower is rounder."))));

  auto lofi = std::make_unique<Module>(s, "LO-FI", "lofiOn", "A radio / telephone band-pass.");
  lofi->addKnob(std::make_unique<Knob>(s, "lofiTone", "Tone", 58, juce::String(juce::CharPointer_UTF8("Lo-Fi \xc2\xb7 Tone: center of the radio band."))));
  lofi->addKnob(std::make_unique<Knob>(s, "lofiBlend", "Blend", 58, juce::String(juce::CharPointer_UTF8("Lo-Fi \xc2\xb7 Blend: dry to fully filtered."))));

  addAndMakeVisible(wham_);
  addAndMakeVisible(sustain_);
  // Right-clicks anywhere inside come here too (MIDI learn).
  addMouseListener(this, true);
  for (auto* m : {tight.get(), grit.get(), sub.get(), lofi.get()}) addAndMakeVisible(*m);
  modules_.push_back(std::move(tight));
  modules_.push_back(std::move(grit));
  modules_.push_back(std::move(sub));
  modules_.push_back(std::move(lofi));

  addChildComponent(settings_);
  settings_.onClose = [this] { showSettings(false); };
  addChildComponent(manual_);
  addChildComponent(hushPanel_);
  hushPanel_.onClose = [this] { showHush(false); };
  manual_.onClose = [this] { showManual(false); };

  hint_ = kDefaultHint;
  setLook(processor.params.state.getProperty(kLookProp, "chrome").toString());
  setSize(kWidth, kHeight);
  startTimerHz(30);
}

DjontView::~DjontView() { setLookAndFeel(nullptr); }

void DjontView::setLook(const juce::String& lookId) {
  const auto& look = lookFor(lookId);
  processor_.params.state.setProperty(kLookProp, look.id, nullptr);
  lnf_.setLook(look);
  settings_.syncLook(look.id);
  photo_ = loadPhoto(look.id);
  photoScaled_ = {};
  sendLookAndFeelChange();
  repaint();
}

void DjontView::showAdvanced(bool show) {
  wham_.setBack(show);
  sustain_.setBack(show);
}

void DjontView::showSettings(bool show) { showOverlay(show ? &settings_ : nullptr); }

void DjontView::showManual(bool show, int section) {
  if (show && section >= 0) manual_.showSection(section);
  showOverlay(show ? &manual_ : nullptr);
}

void DjontView::showOverlay(juce::Component* overlay) {
  settings_.setVisible(overlay == &settings_);
  manual_.setVisible(overlay == &manual_);
  hushPanel_.setVisible(overlay == &hushPanel_);
  hushChip_.setToggleState(overlay == &hushPanel_, juce::dontSendNotification);
  gear_.setToggleState(overlay == &settings_, juce::dontSendNotification);
  help_.setToggleState(overlay == &manual_, juce::dontSendNotification);
  const bool controls = overlay == nullptr;
  for (auto& m : modules_) m->setVisible(controls);
  wham_.setVisible(controls);
  sustain_.setVisible(controls);
  if (overlay != nullptr) overlay->toFront(false);
  repaint();
}

void DjontView::timerCallback() {
  const float db = processor_.outputPeakDb.load();
  if (std::abs(db - meterDb_) > 0.2f) {
    meterDb_ = db;
    repaint(meterBox_);
  }
  wham_.tick();
  sustain_.tick();
  hushChip_.setButtonText(processor_.params.getRawParameterValue("hushOn")->load() > 0.5f ? "HUSH ON" : "HUSH");
  // MIDI learn: a CC arrived for the control waiting for one.
  if (const int learned = processor_.learnedCount.load(); learned != learnedSeen_) {
    learnedSeen_ = learned;
    if (learnTarget_ != nullptr) {
      const auto id = learnTarget_->getProperties()[kParamIdProp].toString();
      confirm_ = learnName_ + " is on MIDI CC " + juce::String(processor_.ccFor(id)) + ". Right-click it to forget.";
      confirmTicks_ = 120;
    }
    learnTarget_ = nullptr;
    repaint();
  }
  if (learnTarget_ != nullptr && processor_.learning().isEmpty()) {  // cancelled
    learnTarget_ = nullptr;
    repaint();
  }
  if (confirmTicks_ > 0 && --confirmTicks_ == 0) confirm_ = {};
  updateHint();
}

void DjontView::updateHint() {
  juce::String text = kDefaultHint;
  auto* c = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
  if (c != nullptr && (c == this || isParentOf(c))) {
    juce::String paramId;
    for (; c != nullptr && c != this; c = c->getParentComponent()) {
      if (paramId.isEmpty()) paramId = c->getProperties()[kParamIdProp].toString();
      if (auto* src = dynamic_cast<HintSource*>(c)) {
        text = src->hint();
        break;
      }
      if (c->getHelpText().isNotEmpty()) {
        text = c->getHelpText();
        break;
      }
      if (auto* panel = dynamic_cast<hushui::HushPanel*>(c)) {
        text = panel->hint();
        break;
      }
    }
    if (paramId.isNotEmpty())
      if (const int cc = processor_.ccFor(paramId); cc >= 0) text << juce::String(juce::CharPointer_UTF8("  \xc2\xb7 MIDI CC ")) << cc;
  }
  if (learnTarget_ != nullptr)
    text = "MIDI learn: press a footswitch or move a pedal on your controller to control " + learnName_ +
           ". Right-click to cancel.";
  else if (confirm_.isNotEmpty())
    text = confirm_;
  if (text != hint_) {
    hint_ = text;
    repaint(hintBox_);
  }
}

void DjontView::mouseDown(const juce::MouseEvent& e) {
  if (!e.mods.isPopupMenu()) return;
  // The control under the click, if it's bound to a parameter.
  juce::Component* target = nullptr;
  for (auto* c = e.eventComponent; c != nullptr && c != this; c = c->getParentComponent())
    if (c->getProperties().contains(kParamIdProp)) {
      target = c;
      break;
    }
  juce::PopupMenu menu;
  if (learnTarget_ != nullptr) {
    menu.addItem(3, "Cancel MIDI learn");
  } else if (target != nullptr) {
    const auto id = target->getProperties()[kParamIdProp].toString();
    auto* param = processor_.params.getParameter(id);
    const auto name = param != nullptr ? param->getName(64) : id;
    menu.addSectionHeader(name);
    menu.addItem(1, "MIDI Learn");
    if (const int cc = processor_.ccFor(id); cc >= 0) menu.addItem(2, "Forget MIDI CC " + juce::String(cc));
  } else {
    return;
  }
  juce::Component::SafePointer<juce::Component> safeTarget(target);
  menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({e.getScreenX(), e.getScreenY(), 1, 1}),
                     [this, safeTarget](int result) {
                       if (result == 3) {
                         processor_.cancelLearn();
                         learnTarget_ = nullptr;
                       } else if (safeTarget != nullptr && (result == 1 || result == 2)) {
                         const auto id = safeTarget->getProperties()[kParamIdProp].toString();
                         if (result == 1) {
                           auto* param = processor_.params.getParameter(id);
                           learnName_ = param != nullptr ? param->getName(64) : id;
                           learnTarget_ = safeTarget.getComponent();
                           processor_.startLearn(id);
                         } else {
                           processor_.forget(id);
                         }
                       }
                       repaint();
                     });
}

void DjontView::paintOverChildren(juce::Graphics& g) {
  if (learnTarget_ == nullptr) return;
  const auto r = getLocalArea(learnTarget_, learnTarget_->getLocalBounds()).toFloat().expanded(4.0f);
  g.setColour(lnf_.look().accent);
  g.drawRoundedRectangle(r, 6.0f, 2.5f);
}

void DjontView::paint(juce::Graphics& g) {
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

void DjontView::resized() {
  hintBox_ = {0, getHeight() - kHintH, getWidth(), kHintH};
  const int right = getWidth() - kPad;
  // Header, right to left: gear, meter, Output, Input, on one plate.
  gear_.setTopRightPosition(right - 8, (kHeaderH - gear_.getHeight()) / 2);
  help_.setTopRightPosition(gear_.getX() - 6, gear_.getY());
  meterBox_ = {help_.getX() - 18, (kHeaderH - 60) / 2, 9, 60};
  output_.setTopRightPosition(meterBox_.getX() - 10, (kHeaderH - output_.getHeight()) / 2 + 2);
  blend_.setTopRightPosition(output_.getX() - 2, output_.getY());
  input_.setTopRightPosition(blend_.getX() - 2, output_.getY());
  hushChip_.setTopRightPosition(input_.getX() - 8, (kHeaderH - hushChip_.getHeight()) / 2);
  ioPlate_ = juce::Rectangle<int>(hushChip_.getX() - 10, 14, right - hushChip_.getX() + 10, kHeaderH - 28);

  const int top = kBodyTop + 10;
  const int bottom = hintBox_.getY() - 10;
  const int left = 18, gap = 10;
  wham_.setBounds(left, top, WhamModule::kWidth, bottom - top);
  sustain_.setBounds(wham_.getRight() + gap, top, SustainModule::kWidth, bottom - top);
  const int x0 = sustain_.getRight() + gap;
  const int n = static_cast<int>(modules_.size());
  const int w = (getWidth() - left - x0 - gap * (n - 1)) / n;
  for (int i = 0; i < n; ++i) modules_[static_cast<size_t>(i)]->setBounds(x0 + i * (w + gap), top, w, bottom - top);
  settings_.setBounds(left, top, getWidth() - 2 * left, bottom - top);
  manual_.setBounds(settings_.getBounds());
  hushPanel_.setBounds(settings_.getBounds());
  photoScaled_ = {};
}

// ---------------------------------------------------------------- window
namespace {
const juce::Identifier kUiWidthProp{"uiWidth"};
}

DjontEditor::DjontEditor(DjontProcessor& processor)
    : juce::AudioProcessorEditor(processor), processor_(processor), view_(processor) {
  addAndMakeVisible(view_);
  constexpr double kAspect = static_cast<double>(DjontView::kWidth) / DjontView::kHeight;
  setResizable(true, true);
  setResizeLimits(DjontView::kWidth * 6 / 10, DjontView::kHeight * 6 / 10, DjontView::kWidth * 2,
                  DjontView::kHeight * 2);
  if (auto* c = getConstrainer()) c->setFixedAspectRatio(kAspect);
  const int width = juce::jlimit(DjontView::kWidth * 6 / 10, DjontView::kWidth * 2,
                                 static_cast<int>(processor.params.state.getProperty(kUiWidthProp, DjontView::kWidth)));
  setSize(width, juce::roundToInt(width / kAspect));
}

void DjontEditor::resized() {
  const float scale = static_cast<float>(getWidth()) / static_cast<float>(DjontView::kWidth);
  view_.setBounds(0, 0, DjontView::kWidth, DjontView::kHeight);
  view_.setTransform(juce::AffineTransform::scale(scale));
  processor_.params.state.setProperty(kUiWidthProp, getWidth(), nullptr);
}

}  // namespace djont
