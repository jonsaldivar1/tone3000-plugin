#include "ParabolaEditor.h"

#include "ParabolaBinary.h"

#if defined(JucePlugin_Build_Standalone) && JucePlugin_Build_Standalone && \
    !(defined(JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP) && JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP)
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#define PARABOLA_HAS_STANDALONE_HOLDER 1
#else
#define PARABOLA_HAS_STANDALONE_HOLDER 0
#endif

namespace parabola {

namespace {
constexpr float kArcStart = juce::MathConstants<float>::pi * 1.25f;
constexpr float kArcEnd = juce::MathConstants<float>::pi * 2.75f;
constexpr int kPad = 20, kHeaderH = 104, kRule = 3, kHintH = 30;
constexpr int kXoverY = kHeaderH + kRule + 12, kXoverH = 236;
constexpr int kBandsY = kXoverY + kXoverH + 12, kBandsH = 230;
constexpr int kBottomY = kBandsY + kBandsH + 12, kBottomH = 132;
const juce::String kDefaultHint = "Hover any control to see what it does. Double-click a knob to reset it. "
                                  "Drop a .nam on a band, or an IR on the cab.";
const juce::Identifier kLookProp{"look"};
const juce::Identifier kUiWidthProp{"uiWidth"};

juce::Colour hex(juce::uint32 argb) { return juce::Colour(argb); }
juce::Colour rgba(int r, int g, int b, float a) {
  return juce::Colour::fromRGBA(static_cast<juce::uint8>(r), static_cast<juce::uint8>(g), static_cast<juce::uint8>(b),
                                static_cast<juce::uint8>(juce::roundToInt(a * 255.0f)));
}

Look makeFoil() {
  Look L;
  L.id = "foil"; L.name = "Foil";
  L.bg = hex(0xff04070c); L.ink = hex(0xffe9eef3); L.muted = hex(0xffa7b6c4); L.accent = hex(0xff4cc3ff);
  L.plate = rgba(5, 9, 16, 0.82f); L.border = rgba(233, 238, 243, 0.16f); L.rule = rgba(233, 238, 243, 0.28f);
  L.grid = rgba(233, 238, 243, 0.08f); L.faceHi = hex(0xff26303c); L.faceLo = hex(0xff0a0e14);
  L.track = rgba(233, 238, 243, 0.16f); L.slot = rgba(233, 238, 243, 0.06f); L.hintBg = rgba(3, 5, 9, 0.92f);
  L.scrim = rgba(3, 6, 12, 0.35f);
  L.band = {hex(0xff3d8bff), hex(0xffff8a3d), hex(0xffffd84a)};
  return L;
}

const std::array<Look, 5>& looks() {
  static const std::array<Look, 5> all = [] {
    const Look base = makeFoil();
    Look inferno = base;
    inferno.id = "inferno"; inferno.name = "Inferno";
    inferno.bg = hex(0xff0a0203); inferno.scrim = rgba(10, 2, 3, 0.45f); inferno.ink = hex(0xfffbeee9);
    inferno.muted = hex(0xffd2aea9); inferno.accent = hex(0xffff3b2f); inferno.plate = rgba(16, 4, 5, 0.82f);
    inferno.border = rgba(251, 238, 233, 0.16f); inferno.rule = rgba(255, 59, 47, 0.5f);
    inferno.grid = rgba(251, 238, 233, 0.08f); inferno.faceHi = hex(0xff3a1214); inferno.faceLo = hex(0xff120405);
    inferno.hintBg = rgba(10, 2, 3, 0.92f); inferno.track = rgba(251, 238, 233, 0.16f);
    inferno.slot = rgba(251, 238, 233, 0.06f);
    inferno.band = {hex(0xff9fb4d0), hex(0xffff3b2f), hex(0xffffb35c)};

    Look hologram = base;
    hologram.id = "hologram"; hologram.name = "Hologram";
    hologram.bg = hex(0xff06060a); hologram.scrim = rgba(6, 6, 10, 0.5f); hologram.ink = hex(0xfff5f3ff);
    hologram.muted = hex(0xffbdb9d2); hologram.accent = hex(0xffff9a4a); hologram.plate = rgba(10, 10, 16, 0.82f);
    hologram.border = rgba(95, 211, 232, 0.24f); hologram.rule = rgba(95, 211, 232, 0.5f);
    hologram.faceHi = hex(0xff2a2a3a); hologram.faceLo = hex(0xff0c0c12);
    hologram.band = {hex(0xff5fd3e8), hex(0xffff9a4a), hex(0xffc7a6ff)};

    Look ram = base;
    ram.id = "ram"; ram.name = "Ram";
    ram.bg = hex(0xff0c0b09); ram.scrim = rgba(10, 9, 8, 0.6f); ram.ink = hex(0xfff4efe6); ram.muted = hex(0xffc4b9a6);
    ram.accent = hex(0xffd9c9a8); ram.plate = rgba(14, 12, 10, 0.84f); ram.border = rgba(217, 201, 168, 0.22f);
    ram.rule = rgba(217, 201, 168, 0.5f); ram.grid = rgba(217, 201, 168, 0.08f); ram.faceHi = hex(0xff3a342b);
    ram.faceLo = hex(0xff110f0c); ram.hintBg = rgba(10, 9, 8, 0.92f); ram.track = rgba(244, 239, 230, 0.16f);
    ram.slot = rgba(244, 239, 230, 0.06f);
    ram.band = {hex(0xff7d8b99), hex(0xffc98a4b), hex(0xffefe3c8)};

    Look ember = base;
    ember.id = "ember"; ember.name = "Ember";
    ember.bg = hex(0xff04060c); ember.scrim = rgba(4, 6, 12, 0.5f); ember.ink = hex(0xfff2f5fb);
    ember.muted = hex(0xffb4bccd); ember.accent = hex(0xffff4433); ember.plate = rgba(6, 8, 16, 0.84f);
    ember.border = rgba(47, 140, 255, 0.24f); ember.rule = rgba(255, 68, 51, 0.5f); ember.faceHi = hex(0xff22293c);
    ember.faceLo = hex(0xff080b14); ember.hintBg = rgba(4, 6, 12, 0.92f);
    ember.band = {hex(0xff2f8cff), hex(0xffff4433), hex(0xffffd0a0)};
    return std::array<Look, 5>{base, inferno, hologram, ram, ember};
  }();
  return all;
}

juce::Image loadPhoto(const juce::String& id) {
  if (id == "inferno") return juce::ImageFileFormat::loadFrom(ParabolaBinary::skininferno_jpg, ParabolaBinary::skininferno_jpgSize);
  if (id == "hologram") return juce::ImageFileFormat::loadFrom(ParabolaBinary::skinhologram_jpg, ParabolaBinary::skinhologram_jpgSize);
  if (id == "ram") return juce::ImageFileFormat::loadFrom(ParabolaBinary::skinram_jpg, ParabolaBinary::skinram_jpgSize);
  if (id == "ember") return juce::ImageFileFormat::loadFrom(ParabolaBinary::skinember_jpg, ParabolaBinary::skinember_jpgSize);
  return juce::ImageFileFormat::loadFrom(ParabolaBinary::skinfoil_jpg, ParabolaBinary::skinfoil_jpgSize);
}

#if PARABOLA_HAS_STANDALONE_HOLDER
juce::StandalonePluginHolder* holder(const juce::AudioProcessor& p) {
  if (p.wrapperType != juce::AudioProcessor::wrapperType_Standalone) return nullptr;
  return juce::StandalonePluginHolder::getInstance();
}
#endif

juce::Colour bandColour(const juce::Component& c, int band) {
  const auto& L = PbzLookAndFeel::lookOf(c);
  return band >= 0 && band < kNumBands ? L.band[static_cast<size_t>(band)] : L.accent;
}

// Where file choosers start: the folder you last loaded a capture (or IR)
// from, on any band and by any route, else Documents\Parabola Bass Z\<sub>
// when it exists, else the first tones folder, else Documents.
juce::File startDir(bool nam, const juce::String& sub) {
  const auto last = Library::lastFolder(nam);
  if (last.isDirectory()) return last;
  const auto docs = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
  const auto ours = docs.getChildFile("Parabola Bass Z").getChildFile(sub);
  if (ours.isDirectory()) return ours;
  const auto roots = Library::roots();
  return roots.empty() ? docs : roots.front();
}

void drawPlate(juce::Graphics& g, const juce::Component& c, juce::Rectangle<float> box) {
  const auto& L = PbzLookAndFeel::lookOf(c);
  g.setColour(L.plate);
  g.fillRoundedRectangle(box, 6.0f);
  g.setColour(L.border);
  g.drawRoundedRectangle(box.reduced(0.5f), 6.0f, 1.0f);
}

void drawSlot(juce::Graphics& g, const juce::Component& c, juce::Rectangle<float> box, const juce::String& tag,
              juce::Colour tagColour, const juce::String& name, const juce::String& note, bool highlight) {
  const auto& L = PbzLookAndFeel::lookOf(c);
  g.setColour(highlight ? tagColour.withAlpha(0.18f) : L.slot);
  g.fillRoundedRectangle(box, 4.0f);
  juce::Path outline;
  outline.addRoundedRectangle(box.reduced(0.5f), 4.0f);
  juce::Path dashed;
  const float dashes[] = {4.0f, 3.0f};
  juce::PathStrokeType(1.0f).createDashedStroke(dashed, outline, dashes, 2);
  g.setColour(highlight ? tagColour : L.border.withMultipliedAlpha(1.6f));
  g.fillPath(dashed);

  auto r = box.reduced(8.0f, 0.0f);
  g.setFont(PbzLookAndFeel::label(9.0f));
  const float tagW = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), tag) + 12.0f;
  const auto tagBox = r.removeFromLeft(tagW).withSizeKeepingCentre(tagW, 17.0f);
  g.setColour(tagColour);
  g.fillRoundedRectangle(tagBox, 2.0f);
  g.setColour(juce::Colour(0xff05080d));
  g.drawText(tag, tagBox, juce::Justification::centred, false);
  r.removeFromLeft(8.0f);
  g.setColour(L.ink);
  if (note.isEmpty()) {
    g.setFont(juce::Font(juce::FontOptions(12.5f, juce::Font::bold)));
    g.drawFittedText(name, r.toNearestInt(), juce::Justification::centredLeft, 1, 0.9f);
  } else {
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.drawFittedText(name, r.withTrimmedBottom(r.getHeight() * 0.45f).toNearestInt(), juce::Justification::bottomLeft, 1, 0.9f);
    g.setColour(L.muted);
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    g.drawFittedText(note, r.withTrimmedTop(r.getHeight() * 0.55f).toNearestInt(), juce::Justification::topLeft, 1, 0.9f);
  }
}
}  // namespace

const char* const kLookIds[5] = {"foil", "inferno", "hologram", "ram", "ember"};

const Look& lookFor(const juce::String& id) {
  for (const auto& l : looks())
    if (l.id == id) return l;
  return looks()[0];
}

// ---------------------------------------------------------------- look
PbzLookAndFeel::PbzLookAndFeel()
    : anton_(juce::Typeface::createSystemTypefaceFor(ParabolaBinary::AntonRegular_ttf, ParabolaBinary::AntonRegular_ttfSize)),
      look_(&looks()[0]) {
  setLook(looks()[0]);
}

void PbzLookAndFeel::setLook(const Look& look) {
  look_ = &look;
  const auto panel = look.bg.brighter(0.1f);
  setColour(juce::ResizableWindow::backgroundColourId, look.bg);
  setColour(juce::Label::textColourId, look.ink);
  setColour(juce::ComboBox::backgroundColourId, panel);
  setColour(juce::ComboBox::textColourId, look.ink);
  setColour(juce::ComboBox::outlineColourId, look.ink.withAlpha(0.3f));
  setColour(juce::ComboBox::arrowColourId, look.accent);
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
  setColour(juce::TextEditor::backgroundColourId, panel);
  setColour(juce::TextEditor::textColourId, look.ink);
  setColour(juce::TextEditor::outlineColourId, look.accent);
  setColour(juce::CaretComponent::caretColourId, look.accent);
}

juce::Font PbzLookAndFeel::display(const juce::Component& c, float px) {
  if (auto* lnf = dynamic_cast<PbzLookAndFeel*>(&c.getLookAndFeel()))
    return juce::Font(juce::FontOptions(lnf->anton_).withHeight(px));
  return juce::Font(juce::FontOptions(px, juce::Font::bold));
}

juce::Font PbzLookAndFeel::label(float px) {
  return juce::Font(juce::FontOptions(px, juce::Font::bold)).withExtraKerningFactor(0.12f);
}

const Look& PbzLookAndFeel::lookOf(const juce::Component& c) {
  if (auto* lnf = dynamic_cast<PbzLookAndFeel*>(&c.getLookAndFeel())) return lnf->look();
  return looks()[0];
}

void PbzLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end,
                                      juce::Slider& slider) {
  const auto& L = look();
  const auto box = juce::Rectangle<int>(x, y, w, h).toFloat();
  const float r = juce::jmin(box.getWidth(), box.getHeight()) / 2.0f;
  const auto c = box.getCentre();
  const float angle = start + pos * (end - start);
  const float stroke = juce::jmax(3.0f, r * 0.12f);
  const float arcR = r - stroke / 2.0f;
  const int band = slider.getProperties().getWithDefault("band", -1);
  const auto colour = band >= 0 && band < kNumBands ? L.band[static_cast<size_t>(band)] : L.accent;

  juce::Path track;
  track.addCentredArc(c.x, c.y, arcR, arcR, 0.0f, start, end, true);
  g.setColour(L.track);
  g.strokePath(track, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
  juce::Path value;
  const float from = slider.getProperties()["bipolar"] ? (start + end) / 2.0f : start;
  value.addCentredArc(c.x, c.y, arcR, arcR, 0.0f, juce::jmin(from, angle), juce::jmax(from, angle), true);
  g.setColour(colour);
  g.strokePath(value, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

  const float bodyR = r - stroke - juce::jmax(2.0f, r * 0.06f);
  const auto face = juce::Rectangle<float>(2 * bodyR, 2 * bodyR).withCentre(c);
  g.setColour(juce::Colours::black.withAlpha(0.5f));
  g.fillEllipse(face.translated(0, 2.0f).expanded(1.0f));
  g.setGradientFill(juce::ColourGradient(L.faceHi, face.getX() + face.getWidth() * 0.4f, face.getY() + face.getHeight() * 0.3f,
                                         L.faceLo, face.getRight(), face.getBottom(), true));
  g.fillEllipse(face);
  g.setColour(L.border);
  g.drawEllipse(face.reduced(0.5f), 1.0f);
  const juce::Point<float> dir(std::sin(angle), -std::cos(angle));
  g.setColour(slider.isMouseOverOrDragging() ? colour : L.ink);
  g.drawLine(juce::Line<float>(c + dir * (bodyR * 0.3f), c + dir * (bodyR * 0.86f)), juce::jmax(2.5f, r * 0.08f));
}

void PbzLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                      juce::Slider::SliderStyle, juce::Slider& slider) {
  const auto& L = look();
  const float cx = static_cast<float>(x) + static_cast<float>(w) / 2.0f;
  const auto track = juce::Rectangle<float>(cx - 3.0f, static_cast<float>(y), 6.0f, static_cast<float>(h));
  g.setColour(L.track);
  g.fillRoundedRectangle(track, 3.0f);
  const float mid = static_cast<float>(y) + static_cast<float>(h) / 2.0f;
  g.setColour(L.accent.withAlpha(0.85f));
  g.fillRect(juce::Rectangle<float>(cx - 3.0f, juce::jmin(mid, pos), 6.0f, std::abs(pos - mid)));
  g.setColour(L.ink.withAlpha(0.35f));
  g.fillRect(cx - 7.0f, mid - 0.5f, 14.0f, 1.0f);
  const auto thumb = juce::Rectangle<float>(20.0f, 9.0f).withCentre({cx, pos});
  g.setColour(slider.isMouseOverOrDragging() ? L.accent : L.ink);
  g.fillRoundedRectangle(thumb, 2.0f);
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
  if (param_ != nullptr) slider_.setDoubleClickReturnValue(true, param_->convertFrom0to1(param_->getDefaultValue()));
  setSize(juce::jmax(size + 16, 64), size + kLabelH + 6);
}

juce::String Knob::valueText() const {
  if (param_ == nullptr) return {};
  auto text = param_->getCurrentValueAsText();
  if (param_->getLabel().isNotEmpty() && text != "Off") text << " " << param_->getLabel();
  return text;
}

juce::String Knob::hint() const { return description_ + "  " + valueText(); }

void Knob::resized() { slider_.setBounds((getWidth() - size_) / 2, 0, size_, size_); }

void Knob::paint(juce::Graphics& g) {
  const auto& L = PbzLookAndFeel::lookOf(*this);
  const bool live = slider_.isMouseButtonDown();
  g.setColour(live ? bandColour(*this, band_) : L.ink);
  g.setFont(PbzLookAndFeel::label(size_ >= 50 ? 10.0f : 9.0f));
  g.drawText(live ? valueText() : name_.toUpperCase(), 0, size_ + 5, getWidth(), kLabelH,
             juce::Justification::centredTop, false);
}

// ---------------------------------------------------------------- chips
Chip::Chip(const juce::String& text, juce::String description) : juce::Button(text), description_(std::move(description)) {
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  setTitle(text);
}

void Chip::paintButton(juce::Graphics& g, bool highlighted, bool down) {
  const auto& L = PbzLookAndFeel::lookOf(*this);
  const auto box = getLocalBounds().toFloat().reduced(1.0f);
  if (down || getToggleState()) {
    g.setColour(L.ink);
    g.fillRect(box);
  }
  g.setColour(down || getToggleState() ? L.bg : highlighted ? L.accent : L.ink);
  g.drawRect(box, 1.5f);
  g.setFont(PbzLookAndFeel::label(9.5f));
  g.drawText(getButtonText(), box, juce::Justification::centred, false);
}

ParamChip::ParamChip(juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& text,
                     juce::String description)
    : LeftClickOnly<juce::Button>(text), description_(std::move(description)) {
  setClickingTogglesState(true);
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  setTitle(text);
  getProperties().set(kParamIdProp, id);
  attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, id, *this);
}

juce::String ParamChip::hint() const { return description_ + (getToggleState() ? "  On" : "  Off"); }

void ParamChip::paintButton(juce::Graphics& g, bool highlighted, bool) {
  const auto& L = PbzLookAndFeel::lookOf(*this);
  const auto box = getLocalBounds().toFloat().reduced(1.0f);
  const bool on = getToggleState();
  if (on) {
    g.setColour(L.ink);
    g.fillRect(box);
  }
  g.setColour(on ? L.bg : highlighted ? L.accent : L.ink);
  g.drawRect(box, 1.5f);
  g.setFont(PbzLookAndFeel::label(10.0f));
  g.drawText(getButtonText(), box, juce::Justification::centred, false);
}

PowerLed::PowerLed(juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& name, int band)
    : LeftClickOnly<juce::Button>(name), name_(name), band_(band) {
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
  return name_ + " band: " + (getToggleState() ? "on" : "off (muted)") + ". Click to mute or unmute the band.";
}

void PowerLed::paintButton(juce::Graphics& g, bool highlighted, bool) {
  const auto& L = PbzLookAndFeel::lookOf(*this);
  const auto colour = bandColour(*this, band_);
  const auto box = getLocalBounds().toFloat().reduced(3.0f);
  const bool on = getToggleState();
  if (on) {
    g.setColour(colour.withAlpha(0.22f));
    g.fillEllipse(box.expanded(3.0f));
  }
  g.setColour(on ? colour : L.ink.withAlpha(highlighted ? 0.25f : 0.12f));
  g.fillEllipse(box.reduced(1.0f));
  g.setColour(L.ink);
  g.drawEllipse(box.reduced(1.0f), 2.0f);
}

IconButton::IconButton(Kind kind, juce::String description)
    : juce::Button(kind == Kind::gear ? "Settings" : "Manual"), kind_(kind), description_(std::move(description)) {
  setTitle(getName());
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  setSize(30, 30);
}

void IconButton::paintButton(juce::Graphics& g, bool highlighted, bool down) {
  const auto& L = PbzLookAndFeel::lookOf(*this);
  const auto colour = getToggleState() || down ? L.accent : highlighted ? L.ink : L.ink.withAlpha(0.8f);
  g.setColour(colour);
  if (kind_ == Kind::help) {
    g.drawEllipse(getLocalBounds().toFloat().reduced(3.0f), 2.0f);
    g.setFont(juce::Font(juce::FontOptions(17.0f, juce::Font::bold)));
    g.drawText("?", getLocalBounds().translated(0, 1), juce::Justification::centred, false);
    return;
  }
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
  g.fillPath(gear);
}

// ---------------------------------------------------------------- split switch
SplitSwitch::SplitSwitch(juce::AudioProcessorValueTreeState& state, const juce::String& id, int band) : band_(band) {
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  setTitle(id);
  getProperties().set(kParamIdProp, id);
  attachment_ = std::make_unique<juce::ParameterAttachment>(*state.getParameter(id), [this](float v) {
    index_ = juce::roundToInt(v);
    repaint();
  });
  attachment_->sendInitialUpdate();
  setSize(96, 20);
}

juce::String SplitSwitch::hint() const {
  const juce::String band = kBandNames[band_];
  return band + juce::String(juce::CharPointer_UTF8(" \xc2\xb7 PRE / POST: ")) +
         "PRE splits first, so the capture only hears this band's slice (thin, fizzy). POST drives first: the capture "
         "hears everything above the low split and the slice is cut out after it, so it grinds on your real notes. "
         "Now: " + (index_ == 1 ? "POST" : "PRE");
}

void SplitSwitch::mouseDown(const juce::MouseEvent& e) {
  if (e.mods.isPopupMenu()) return;
  const int i = e.x < getWidth() / 2 ? 0 : 1;
  attachment_->setValueAsCompleteGesture(static_cast<float>(i));
}

void SplitSwitch::paint(juce::Graphics& g) {
  const auto& L = PbzLookAndFeel::lookOf(*this);
  const auto box = getLocalBounds().toFloat().reduced(1.0f);
  const float w = box.getWidth() / 2.0f;
  const char* labels[2] = {"PRE", "POST"};
  for (int i = 0; i < 2; ++i) {
    const auto seg = juce::Rectangle<float>(box.getX() + static_cast<float>(i) * w, box.getY(), w, box.getHeight());
    if (index_ == i) {
      g.setColour(L.band[static_cast<size_t>(band_)]);
      g.fillRect(seg);
    }
    g.setColour(index_ == i ? juce::Colour(0xff05080d) : L.ink);
    g.setFont(PbzLookAndFeel::label(9.0f));
    g.drawText(labels[i], seg, juce::Justification::centred, false);
  }
  g.setColour(L.ink);
  g.drawRect(box, 1.5f);
}

// ---------------------------------------------------------------- crossover
namespace {
constexpr double kMinHz = 30.0, kMaxHz = 16000.0, kTopDb = 12.0, kBottomDb = -30.0;
}

CrossoverView::CrossoverView(ParabolaProcessor& processor) : processor_(processor) {
  const char* ids[2] = {"xoverLow", "xoverHigh"};
  for (int i = 0; i < 2; ++i) {
    attachments_[static_cast<size_t>(i)] = std::make_unique<juce::ParameterAttachment>(
        *processor.params.getParameter(ids[i]), [this, i](float v) {
          hz_[static_cast<size_t>(i)] = v;
          repaint();
        });
    attachments_[static_cast<size_t>(i)]->sendInitialUpdate();
  }
  setMouseCursor(juce::MouseCursor::NormalCursor);
}

CrossoverView::~CrossoverView() = default;

juce::Rectangle<float> CrossoverView::plot() const {
  return getLocalBounds().toFloat().withTrimmedTop(46.0f).withTrimmedBottom(24.0f).reduced(16.0f, 0.0f);
}

float CrossoverView::xForHz(double hz) const {
  const auto p = plot();
  const double t = std::log(hz / kMinHz) / std::log(kMaxHz / kMinHz);
  return p.getX() + static_cast<float>(t) * p.getWidth();
}

double CrossoverView::hzForX(float x) const {
  const auto p = plot();
  const double t = juce::jlimit(0.0, 1.0, static_cast<double>((x - p.getX()) / p.getWidth()));
  return kMinHz * std::pow(kMaxHz / kMinHz, t);
}

float CrossoverView::yForDb(double db) const {
  const auto p = plot();
  const double t = (kTopDb - juce::jlimit(kBottomDb, kTopDb, db)) / (kTopDb - kBottomDb);
  return p.getY() + static_cast<float>(t) * p.getHeight();
}

double CrossoverView::bandDb(int band, double hz) const {
  auto& s = processor_.params;
  const auto v = [&](const juce::String& id) { return static_cast<double>(s.getRawParameterValue(id)->load()); };
  const juce::String b = kBandIds[band];
  const double f1 = hz_[0], f2 = std::max(static_cast<double>(hz_[1]), hz_[0] * 1.5);
  double mag = band == kLow ? LR4Split::lowMag(hz, f1)
               : band == kMid ? LR4Split::highMag(hz, f1) * LR4Split::lowMag(hz, f2)
                              : LR4Split::highMag(hz, f1) * LR4Split::highMag(hz, f2);
  constexpr double fs = 48000.0;
  Biquad eq;
  eq.set(Biquad::Type::lowShelf, fs, BandEq::kBassHz, 0.7071, v(b + "Bass"));
  mag *= eq.magnitude(fs, hz);
  eq.set(Biquad::Type::peak, fs, BandEq::kMidHz, 0.8, v(b + "Mid"));
  mag *= eq.magnitude(fs, hz);
  eq.set(Biquad::Type::highShelf, fs, BandEq::kTrebleHz, 0.7071, v(b + "Treble"));
  mag *= eq.magnitude(fs, hz);
  if (band != kLow) {
    const double tone = v(b + "Tone");
    if (tone < 19900.0) {
      eq.set(Biquad::Type::lowPass, fs, tone, 0.7071);
      mag *= eq.magnitude(fs, hz);
    }
  }
  return 20.0 * std::log10(std::max(mag, 1e-6)) + v(b + "Level");
}

void CrossoverView::tick() {
  auto& s = processor_.params;
  juce::String sig;
  for (int b = 0; b < kNumBands; ++b)
    for (const char* suffix : {"On", "Level", "Bass", "Mid", "Treble", "Tone"})
      if (auto* p = s.getRawParameterValue(juce::String(kBandIds[b]) + suffix)) sig << p->load() << ",";
  if (sig != signature_) {
    signature_ = sig;
    repaint();
  }
}

void CrossoverView::paint(juce::Graphics& g) {
  const auto& L = PbzLookAndFeel::lookOf(*this);
  drawPlate(g, *this, getLocalBounds().toFloat());
  g.setColour(L.ink);
  g.setFont(PbzLookAndFeel::display(*this, 22.0f));
  g.drawText("CROSSOVER", 14, 8, 200, 30, juce::Justification::centredLeft, false);

  const auto p = plot();
  // Grid.
  g.setColour(L.grid);
  for (double hz : {50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0}) {
    const float x = xForHz(hz);
    g.drawVerticalLine(juce::roundToInt(x), p.getY(), p.getBottom());
  }
  for (double db : {0.0, -12.0, -24.0}) {
    const float y = yForDb(db);
    if (db == 0.0) g.drawHorizontalLine(juce::roundToInt(y), p.getX(), p.getRight());
    else {
      const float dash[] = {3.0f, 5.0f};
      g.drawDashedLine(juce::Line<float>(p.getX(), y, p.getRight(), y), dash, 2);
    }
  }
  g.setColour(L.muted);
  g.setFont(juce::Font(juce::FontOptions(10.0f)));
  const std::pair<double, const char*> labels[] = {{50, "50"}, {100, "100"}, {200, "200"}, {500, "500"},
                                                   {1000, "1k"}, {2000, "2k"}, {5000, "5k"}, {10000, "10k"}};
  for (const auto& [hz, text] : labels)
    g.drawText(text, juce::Rectangle<float>(xForHz(hz) - 20.0f, p.getBottom() + 4.0f, 40.0f, 14.0f),
               juce::Justification::centred, false);

  // Bands.
  auto& s = processor_.params;
  for (int b = 0; b < kNumBands; ++b) {
    const bool on = s.getRawParameterValue(juce::String(kBandIds[b]) + "On")->load() > 0.5f;
    const auto colour = L.band[static_cast<size_t>(b)].withMultipliedAlpha(on ? 1.0f : 0.3f);
    juce::Path curve, fill;
    const int steps = 160;
    for (int i = 0; i <= steps; ++i) {
      const float x = p.getX() + p.getWidth() * static_cast<float>(i) / steps;
      const float y = yForDb(bandDb(b, hzForX(x)));
      if (i == 0) {
        curve.startNewSubPath(x, y);
        fill.startNewSubPath(x, p.getBottom());
      }
      curve.lineTo(x, y);
      fill.lineTo(x, y);
    }
    fill.lineTo(p.getRight(), p.getBottom());
    fill.closeSubPath();
    g.setColour(colour.withAlpha(on ? 0.2f : 0.06f));
    g.fillPath(fill);
    g.setColour(colour);
    g.strokePath(curve, juce::PathStrokeType(2.5f));
  }

  // Split handles.
  for (int i = 0; i < 2; ++i) {
    const double hz = hz_[static_cast<size_t>(i)];
    const float x = xForHz(hz);
    const float y = yForDb(0.5 * (bandDb(i, hz) + bandDb(i + 1, hz)));
    const bool hot = drag_ == i || hover_ == i;
    g.setColour(hot ? L.accent : L.ink);
    g.fillEllipse(juce::Rectangle<float>(hot ? 20.0f : 18.0f, hot ? 20.0f : 18.0f).withCentre({x, y}));
    g.setColour(L.bg);
    g.drawEllipse(juce::Rectangle<float>(18.0f, 18.0f).withCentre({x, y}), 1.5f);
    g.setColour(L.ink);
    g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
    const auto text = hz >= 1000.0 ? juce::String(hz / 1000.0, 2) + " kHz" : juce::String(juce::roundToInt(hz)) + " Hz";
    g.drawText(text, juce::Rectangle<float>(x - 50.0f, y - 30.0f, 100.0f, 16.0f), juce::Justification::centred, false);
  }

  // Legend: band levels.
  auto legend = getLocalBounds().toFloat().removeFromTop(40.0f).withTrimmedRight(16.0f);
  g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
  for (int b = kNumBands - 1; b >= 0; --b) {
    const float level = s.getRawParameterValue(juce::String(kBandIds[b]) + "Level")->load();
    const bool on = s.getRawParameterValue(juce::String(kBandIds[b]) + "On")->load() > 0.5f;
    const auto text = juce::String(kBandNames[b]).toUpperCase() + " " +
                      (on ? (level > 0.05f ? "+" : "") + juce::String(level, 1) + " dB" : juce::String("OFF"));
    const float w = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), text) + 18.0f;
    auto r = legend.removeFromRight(w);
    legend.removeFromRight(10.0f);
    g.setColour(L.band[static_cast<size_t>(b)]);
    g.fillRect(juce::Rectangle<float>(9.0f, 9.0f).withCentre({r.getX() + 5.0f, r.getCentreY() + 2.0f}));
    g.setColour(L.ink);
    g.drawText(text, r.withTrimmedLeft(14.0f).translated(0, 2.0f), juce::Justification::centredLeft, false);
  }
}

int CrossoverView::handleAt(juce::Point<float> pt) const {
  int best = -1;
  float bestD = 22.0f;
  for (int i = 0; i < 2; ++i) {
    const float d = std::abs(pt.x - xForHz(hz_[static_cast<size_t>(i)]));
    if (d < bestD && plot().expanded(0, 20).contains(pt)) {
      bestD = d;
      best = i;
    }
  }
  return best;
}

void CrossoverView::mouseMove(const juce::MouseEvent& e) {
  const int h = handleAt(e.position);
  if (h != hover_) {
    hover_ = h;
    setMouseCursor(h >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    repaint();
  }
}

void CrossoverView::mouseDown(const juce::MouseEvent& e) {
  drag_ = handleAt(e.position);
  if (drag_ >= 0) attachments_[static_cast<size_t>(drag_)]->beginGesture();
}

void CrossoverView::mouseDrag(const juce::MouseEvent& e) {
  if (drag_ < 0) return;
  double hz = hzForX(e.position.x);
  if (drag_ == 0) hz = juce::jlimit(60.0, std::min(400.0, hz_[1] / 1.5), hz);
  else hz = juce::jlimit(std::max(600.0, hz_[0] * 1.5), 5000.0, hz);
  attachments_[static_cast<size_t>(drag_)]->setValueAsPartOfGesture(static_cast<float>(hz));
}

void CrossoverView::mouseUp(const juce::MouseEvent&) {
  if (drag_ >= 0) attachments_[static_cast<size_t>(drag_)]->endGesture();
  drag_ = -1;
  repaint();
}

void CrossoverView::mouseDoubleClick(const juce::MouseEvent& e) {
  const int h = handleAt(e.position);
  if (h < 0) return;
  attachments_[static_cast<size_t>(h)]->setValueAsCompleteGesture(h == 0 ? 180.0f : 1600.0f);
}

juce::String CrossoverView::hint() const {
  if (hover_ == 0 || drag_ == 0)
    return "Low / Mid split: " + juce::String(juce::roundToInt(hz_[0])) +
           " Hz. Below it is the low band (your clean foundation). Drag to move it, double-click to reset (180 Hz).";
  if (hover_ == 1 || drag_ == 1)
    return "Mid / High split: " + juce::String(hz_[1] / 1000.0f, 2) +
           " kHz. Between the splits is the mid band (growl), above it the high band (grit, fizz). Drag to move, "
           "double-click to reset (1.6 kHz).";
  return "Crossover: your bass split into three bands, each with its own amp capture. The curves show each band's "
         "range, level and EQ. Drag the two handles to move the splits.";
}

// ---------------------------------------------------------------- band strip
namespace {
const char* const kRoles[kNumBands] = {"CLEAN \xc2\xb7 COMPRESSED", "GROWL", "GRIT \xc2\xb7 FIZZ"};  // UTF-8
}

BandStrip::BandStrip(ParabolaProcessor& processor, int band)
    : processor_(processor),
      band_(band),
      power_(processor.params, juce::String(kBandIds[band]) + "On", kBandNames[band], band),
      adv_("ADV", "ADV: flip this band to its Bass / Mid / Treble and its CAB switch."),
      load_("LOAD", "Load a NAM capture (.nam) into this band. You can also drag a .nam file onto the band."),
      clear_(juce::CharPointer_UTF8("\xc3\x97"), "Remove this band's capture (the band plays clean)."),
      cab_(processor.params, juce::String(kBandIds[band]) + "Cab", "CAB",
           juce::String(kBandNames[band]) + juce::String(juce::CharPointer_UTF8(" \xc2\xb7 Cab: send this band through the cab IR. Mid and high are on by "
                                             "default, low is off (Parallax-style: a clean, cab-free low end). Full-rig "
                                             "captures skip the cab anyway."))) {
  auto& s = processor.params;
  const juce::String id = kBandIds[band];
  const juce::String name = kBandNames[band];
  addAndMakeVisible(power_);
  addAndMakeVisible(adv_);
  addAndMakeVisible(load_);
  addAndMakeVisible(clear_);
  addChildComponent(cab_);
  adv_.onClick = [this] { setBack(!back_); };
  load_.onClick = [this] { chooseFile(); };
  clear_.onClick = [this] {
    processor_.clearNam(band_);
    refreshSlot();
  };
  power_.onPower = [this](bool on) { setPowered(on); };

  auto knob = [&](const char* suffix, const juce::String& label, int size, const juce::String& desc, bool bipolar) {
    auto k = std::make_unique<Knob>(s, id + suffix, label, size, name + juce::String(juce::CharPointer_UTF8(" \xc2\xb7 ")) + desc);
    k->setColourIndex(band);
    if (bipolar) k->setBipolar();
    return k;
  };
  front_.push_back(knob("Drive", "Drive", 56,
                        band == kLow ? "Drive: level into the low band's capture (only matters when one is loaded)."
                                     : "Drive: how hard this band's capture is pushed. More = more grind.",
                        false));
  if (band == kLow)
    front_.push_back(knob("Comp", "Comp", 56,
                          "Comp: squashes the low band so the bottom stays even and solid. More = lower threshold, "
                          "higher ratio.",
                          false));
  else
    front_.push_back(knob("Tone", "Tone", 56,
                          "Tone: low-pass after the capture. Turn down to tame fizz; fully up is off.", false));
  front_.push_back(knob("Level", "Level", 56, "Level: this band's volume in the mix.", true));
  backKnobs_.push_back(knob("Bass", "Bass", 50, juce::String(juce::CharPointer_UTF8("Bass: shelf at 100 Hz, \xc2\xb1""12 dB.")), true));
  backKnobs_.push_back(knob("Mid", "Mid", 50, juce::String(juce::CharPointer_UTF8("Mid: bell at 650 Hz, \xc2\xb1""12 dB.")), true));
  backKnobs_.push_back(knob("Treble", "Treble", 50, juce::String(juce::CharPointer_UTF8("Treble: shelf at 3 kHz, \xc2\xb1""12 dB.")), true));
  for (auto& k : front_) addAndMakeVisible(*k);
  for (auto& k : backKnobs_) addChildComponent(*k);
  if (band != kLow) {
    split_ = std::make_unique<SplitSwitch>(s, id + "Split", band);
    addAndMakeVisible(*split_);
  }
  refreshSlot();
  setPowered(power_.getToggleState());
}

void BandStrip::setPowered(bool on) {
  for (auto& k : front_) k->setAlpha(on ? 1.0f : 0.4f);
  for (auto& k : backKnobs_) k->setAlpha(on ? 1.0f : 0.4f);
}

void BandStrip::setBack(bool back) {
  back_ = back;
  adv_.setButtonText(back ? "BACK" : "ADV");
  adv_.setDescription(back ? "BACK: return to Drive / Level." : "ADV: flip this band to its Bass / Mid / Treble and its CAB switch.");
  for (auto& k : front_) k->setVisible(!back);
  for (auto& k : backKnobs_) k->setVisible(back);
  if (split_ != nullptr) split_->setVisible(!back);
  cab_.setVisible(back);
  resized();
  repaint();
}

void BandStrip::refreshSlot() {
  const auto& slot = processor_.slot(band_);
  slotNote_ = {};
  if (slot.isLoaded()) {
    const auto info = slot.info();
    slotName_ = info.name;
    juce::StringArray notes;
    if (info.gearType.isNotEmpty()) notes.add(info.gearType);
    if (info.modeledBy.isNotEmpty()) notes.add("by " + info.modeledBy);
    if (info.fullRig) notes.add("includes a cab: skips the IR");
    slotNote_ = notes.joinIntoString(juce::String(juce::CharPointer_UTF8(" \xc2\xb7 ")));
  } else {
    slotName_ = band_ == kLow ? "No capture: clean DI (optional)" : "No capture: LOAD or drop a .nam here";
  }
  clear_.setVisible(slot.isLoaded());
  repaint();
}

void BandStrip::chooseFile() {
  chooser_ = std::make_unique<juce::FileChooser>("Load a NAM capture for the " + juce::String(kBandNames[band_]) + " band",
                                                 startDir(true, "NAMs"), "*.nam");
  chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                        [this](const juce::FileChooser& fc) {
                          const auto file = fc.getResult();
                          if (file.existsAsFile()) load(file);
                        });
}

void BandStrip::load(const juce::File& file) {
  const auto error = processor_.loadNam(band_, file);
  if (error.isEmpty()) Library::rememberFolder(file);
  refreshSlot();
  if (error.isNotEmpty()) {
    slotNote_ = error;
    repaint();
  }
}

bool BandStrip::isInterestedInFileDrag(const juce::StringArray& files) {
  return files.size() == 1 && files[0].endsWithIgnoreCase(".nam");
}

void BandStrip::filesDropped(const juce::StringArray& files, int, int) {
  dragOver_ = false;
  if (isInterestedInFileDrag(files)) load(juce::File(files[0]));
}

void BandStrip::paint(juce::Graphics& g) {
  const auto& L = PbzLookAndFeel::lookOf(*this);
  const auto colour = L.band[static_cast<size_t>(band_)];
  drawPlate(g, *this, getLocalBounds().toFloat());
  g.setColour(colour);
  g.fillRoundedRectangle(getLocalBounds().toFloat().removeFromTop(3.0f), 1.5f);

  g.setColour(L.ink);
  const auto font = PbzLookAndFeel::display(*this, 30.0f);
  g.setFont(font);
  const juce::String title = juce::String(kBandNames[band_]).toUpperCase();
  g.drawText(title, 14, 8, 120, 36, juce::Justification::centredLeft, false);
  g.setColour(L.muted);
  g.setFont(PbzLookAndFeel::label(9.5f));
  const float titleW = juce::GlyphArrangement::getStringWidth(font, title);
  if (back_ || split_ == nullptr)
    g.drawText(back_ ? juce::String(juce::CharPointer_UTF8("EQ \xc2\xb7 CAB")) : juce::String(juce::CharPointer_UTF8(kRoles[band_])),
               juce::roundToInt(14 + titleW + 10), 20, 170, 18, juce::Justification::centredLeft, false);
  if (split_ != nullptr) split_->setTopLeftPosition(juce::roundToInt(14 + titleW + 12), 17);

  auto slot = juce::Rectangle<float>(14.0f, static_cast<float>(kSlotY), static_cast<float>(getWidth() - 28), static_cast<float>(kSlotH));
  slot.removeFromRight(static_cast<float>(load_.getWidth() + (clear_.isVisible() ? clear_.getWidth() + 6 : 0) + 8));
  drawSlot(g, *this, slot.withRight(static_cast<float>(getWidth() - 14)), "NAM", colour, slotName_, slotNote_, dragOver_);

  if (back_) {
    g.setColour(L.muted);
    g.setFont(juce::Font(juce::FontOptions(10.5f)));
    juce::String route;
    if (!processor_.irLoaded()) route = "No cab IR loaded.";
    else if (processor_.slot(band_).isFullRig()) route = "Capture includes a cab: no IR.";
    else route = processor_.bandUsesCab(band_) ? "Through the cab IR." : "Skips the cab IR.";
    g.drawText(route, cab_.getRight() + 10, cab_.getY(), getWidth() - cab_.getRight() - 20, cab_.getHeight(),
               juce::Justification::centredLeft, true);
  }
}

void BandStrip::resized() {
  power_.setTopRightPosition(getWidth() - 12, 12);
  adv_.setBounds(power_.getX() - 60, 14, 52, 22);
  const int slotRight = getWidth() - 14 - 6;
  load_.setBounds(slotRight - 50, kSlotY + 7, 50, 20);
  clear_.setBounds(load_.getX() - 26, kSlotY + 7, 22, 20);

  const int top = kSlotY + kSlotH + 10;
  auto layoutRow = [&](std::vector<std::unique_ptr<Knob>>& row, int bottomReserve) {
    int total = 0;
    for (auto& k : row) total += k->getWidth();
    const int gap = 14;
    total += gap * (static_cast<int>(row.size()) - 1);
    int x = (getWidth() - total) / 2;
    const int avail = getHeight() - top - bottomReserve;
    for (auto& k : row) {
      k->setTopLeftPosition(x, top + juce::jmax(0, (avail - k->getHeight()) / 2));
      x += k->getWidth() + gap;
    }
  };
  layoutRow(front_, 6);
  layoutRow(backKnobs_, 36);
  cab_.setBounds(14, getHeight() - 34, 64, 24);
}

// ---------------------------------------------------------------- graphic EQ
juce::String GeqPanel::Fader::hint() const {
  const double v = getValue();
  return juce::String(juce::CharPointer_UTF8("Graphic EQ \xc2\xb7 ")) + label + juce::String(juce::CharPointer_UTF8(" Hz: boost or cut after the bands are mixed back together, \xc2\xb1""12 dB. "
         "Double-click to reset.  ")) + (v > 0.05 ? "+" : "") + juce::String(v, 1) + " dB";
}

GeqPanel::GeqPanel(juce::AudioProcessorValueTreeState& state) {
  const char* labels[6] = {"80", "250", "500", "1K", "2.5K", "6K"};
  for (int i = 0; i < 6; ++i) {
    auto& f = faders_[static_cast<size_t>(i)];
    f.label = labels[i];
    f.setSliderStyle(juce::Slider::LinearVertical);
    f.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    f.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    f.setDoubleClickReturnValue(true, 0.0);
    f.setTitle(juce::String("EQ ") + labels[i]);
    f.getProperties().set(kParamIdProp, "geq" + juce::String(i));
    addAndMakeVisible(f);
    attachments_[static_cast<size_t>(i)] =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "geq" + juce::String(i), f);
  }
}

void GeqPanel::paint(juce::Graphics& g) {
  const auto& L = PbzLookAndFeel::lookOf(*this);
  drawPlate(g, *this, getLocalBounds().toFloat());
  g.setColour(L.ink);
  g.setFont(PbzLookAndFeel::display(*this, 22.0f));
  g.drawFittedText("GRAPHIC\nEQ", 16, 0, 80, getHeight(), juce::Justification::centredLeft, 2, 1.0f);
  g.setFont(PbzLookAndFeel::label(9.5f));
  for (auto& f : faders_)
    g.drawText(f.label, f.getX() - 10, f.getBottom() + 4, f.getWidth() + 20, 14, juce::Justification::centred, false);
}

void GeqPanel::resized() {
  const int x0 = 110, w = getWidth() - x0 - 16;
  const int step = w / 6;
  for (int i = 0; i < 6; ++i)
    faders_[static_cast<size_t>(i)].setBounds(x0 + i * step + (step - 30) / 2, 12, 30, getHeight() - 38);
}

// ---------------------------------------------------------------- cab
CabPanel::CabPanel(ParabolaProcessor& processor)
    : processor_(processor),
      load_("LOAD", "Load a cab IR (.wav) for the mid and high bands. You can also drag a .wav onto the cab."),
      clear_(juce::CharPointer_UTF8("\xc3\x97"), "Remove the cab IR."),
      on_(processor.params, "cabOn", "CAB ON",
          "Cab On: run the bands that have CAB switched on (mid and high by default) through the IR. Off = straight DI-style "
          "tone from the captures.") {
  addAndMakeVisible(load_);
  addAndMakeVisible(clear_);
  addAndMakeVisible(on_);
  load_.onClick = [this] { chooseFile(); };
  clear_.onClick = [this] {
    processor_.clearIr();
    refresh();
  };
  on_.onClick = [this] { refresh(); };
  refresh();
}

void CabPanel::refresh() {
  name_ = processor_.irLoaded() ? processor_.irName() : "No IR: LOAD or drop a .wav here";
  juce::StringArray through;
  for (int b = 0; b < kNumBands; ++b)
    if (processor_.bandUsesCab(b)) through.add(juce::String(kBandNames[b]).toUpperCase());
  routing_ = through.isEmpty() ? juce::String("NO BANDS THROUGH THE CAB") : through.joinIntoString(" + ") + " THROUGH THE CAB";
  clear_.setVisible(processor_.irLoaded());
  repaint();
}

void CabPanel::chooseFile() {
  chooser_ = std::make_unique<juce::FileChooser>("Load a cab IR", startDir(false, "Cabs"),
                                                 "*.wav;*.aif;*.aiff;*.flac");
  chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                        [this](const juce::FileChooser& fc) {
                          const auto file = fc.getResult();
                          if (!file.existsAsFile()) return;
                          if (processor_.loadIr(file).isEmpty()) Library::rememberFolder(file);
                          refresh();
                        });
}

bool CabPanel::isInterestedInFileDrag(const juce::StringArray& files) {
  return files.size() == 1 && (files[0].endsWithIgnoreCase(".wav") || files[0].endsWithIgnoreCase(".aif") ||
                               files[0].endsWithIgnoreCase(".aiff") || files[0].endsWithIgnoreCase(".flac"));
}

void CabPanel::filesDropped(const juce::StringArray& files, int, int) {
  dragOver_ = false;
  if (!isInterestedInFileDrag(files)) return;
  if (processor_.loadIr(juce::File(files[0])).isEmpty()) Library::rememberFolder(juce::File(files[0]));
  refresh();
}

void CabPanel::paint(juce::Graphics& g) {
  const auto& L = PbzLookAndFeel::lookOf(*this);
  drawPlate(g, *this, getLocalBounds().toFloat());
  g.setColour(L.ink);
  g.setFont(PbzLookAndFeel::display(*this, 22.0f));
  g.drawText("CAB", 14, 6, 80, 30, juce::Justification::centredLeft, false);
  g.setColour(L.muted);
  g.setFont(PbzLookAndFeel::label(9.0f));
  g.drawText(routing_, 70, 12, getWidth() - 84, 20, juce::Justification::centredRight, true);
  auto slot = juce::Rectangle<float>(14.0f, 42.0f, static_cast<float>(getWidth() - 28), 34.0f);
  drawSlot(g, *this, slot, "IR", L.ink, name_, {}, dragOver_);
}

void CabPanel::resized() {
  load_.setBounds(getWidth() - 14 - 6 - 50, 49, 50, 20);
  clear_.setBounds(load_.getX() - 26, 49, 22, 20);
  on_.setBounds(14, getHeight() - 40, 100, 28);
}

// ---------------------------------------------------------------- settings
SettingsPanel::SettingsPanel(ParabolaProcessor& processor, std::function<void(const juce::String&)> onLook)
    : processor_(processor) {
  for (int i = 0; i < 5; ++i) {
    auto* b = looks_.add(new juce::TextButton(lookFor(kLookIds[i]).name));
    b->setRadioGroupId(31);
    b->setClickingTogglesState(false);
    b->setMouseCursor(juce::MouseCursor::PointingHandCursor);
    b->setHelpText("Skin: " + lookFor(kLookIds[i]).name + ". The photo and colours; saved with the plugin's state.");
    b->onClick = [onLook, i] { onLook(kLookIds[i]); };
    addAndMakeVisible(b);
  }
  addAndMakeVisible(done_);
  done_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
  done_.onClick = [this] {
    if (onClose) onClose();
  };
#if PARABOLA_HAS_STANDALONE_HOLDER
  if (auto* h = holder(processor_)) {
    devices_ = &h->deviceManager;
    selector_ = std::make_unique<juce::AudioDeviceSelectorComponent>(*devices_, 1, 2, 1, 2, false, false, false, false);
    viewport_.setViewedComponent(selector_.get(), false);
    viewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport_);
    hear_.setHelpText("Hear yourself: play the output while your input is live. Off mutes the input (protects "
                      "against feedback with a mic and speakers).");
    hear_.setToggleState(!static_cast<bool>(h->getMuteInputValue().getValue()), juce::dontSendNotification);
    hear_.onClick = [this, h] { h->getMuteInputValue().setValue(!hear_.getToggleState()); };
    addAndMakeVisible(hear_);
  }
#endif
  startTimerHz(2);
  updateLatency();
}

SettingsPanel::~SettingsPanel() { viewport_.setViewedComponent(nullptr, false); }

void SettingsPanel::syncLook(const juce::String& lookId) {
  for (int i = 0; i < looks_.size(); ++i) looks_[i]->setToggleState(lookId == kLookIds[i], juce::dontSendNotification);
}

juce::String SettingsPanel::hint() const {
  if (devices_ == nullptr)
    return "Settings. In a DAW, the DAW owns the audio device, inputs, outputs and buffer.";
  return "Settings: pick the driver type (ASIO for the lowest latency), your interface, which input your bass is on, "
         "the outputs, sample rate and buffer.";
}

void SettingsPanel::timerCallback() {
  updateLatency();
#if PARABOLA_HAS_STANDALONE_HOLDER
  if (auto* h = holder(processor_))
    hear_.setToggleState(!static_cast<bool>(h->getMuteInputValue().getValue()), juce::dontSendNotification);
#endif
}

void SettingsPanel::updateLatency() {
  juce::String text;
  const int own = processor_.getLatencySamples();
  const double rate = processor_.getSampleRate() > 0 ? processor_.getSampleRate() : 48000.0;
  const juce::String ownLine = own > 0 ? "Parabola adds " + juce::String(1000.0 * own / rate, 1) +
                                             " ms right now (Pitch while it isn't 0; resampling to 48 kHz at other "
                                             "sample rates)."
                                       : "Parabola adds 0 ms right now.";
  if (devices_ == nullptr) {
    text = "Running in a DAW: set the audio device, inputs, outputs and buffer in the DAW.\n\n" + ownLine;
  } else if (auto* device = devices_->getCurrentAudioDevice()) {
    const double sr = device->getCurrentSampleRate();
    const auto ms = [sr](int samples) { return sr > 0 ? 1000.0 * samples / sr : 0.0; };
    const int buffer = device->getCurrentBufferSizeSamples();
    const int in = device->getInputLatencyInSamples(), out = device->getOutputLatencyInSamples();
    text << device->getTypeName() << ": " << device->getName() << "\n"
         << juce::String(sr / 1000.0, 1) << juce::String(juce::CharPointer_UTF8(" kHz \xc2\xb7 buffer ")) << buffer << " (" << juce::String(ms(buffer), 1) << " ms)\n"
         << "Round trip ~" << juce::String(ms(in + out + buffer), 1) << " ms\n" << ownLine;
  } else {
    text = "No audio device is open. Pick a driver type and device on the right.";
  }
  if (text != latency_) {
    latency_ = text;
    repaint();
  }
}

void SettingsPanel::paint(juce::Graphics& g) {
  const auto& L = PbzLookAndFeel::lookOf(*this);
  const auto box = getLocalBounds().toFloat().reduced(0.5f);
  g.setColour(L.bg.withAlpha(0.95f));
  g.fillRoundedRectangle(box, 6.0f);
  g.setColour(L.border);
  g.drawRoundedRectangle(box, 6.0f, 1.0f);
  g.setColour(L.ink);
  g.setFont(PbzLookAndFeel::display(*this, 30.0f));
  g.drawText("SETTINGS", 18, 10, 220, 36, juce::Justification::centredLeft, false);
  g.setFont(PbzLookAndFeel::label(10.0f));
  g.setColour(L.ink.withAlpha(0.7f));
  g.drawText("SKIN", 18, 56, 200, 14, juce::Justification::centredLeft, false);
  g.drawText(devices_ != nullptr ? "LATENCY" : "AUDIO", 18, 226, 220, 14, juce::Justification::centredLeft, false);
  g.setColour(L.muted);
  g.setFont(juce::Font(juce::FontOptions(12.5f)));
  const int textBottom = devices_ != nullptr ? hear_.getY() - 4 : getHeight() - 10;
  g.drawFittedText(latency_, 18, 246, 300, juce::jmax(20, textBottom - 246), juce::Justification::topLeft, 8, 1.0f);
  if (devices_ == nullptr) {
    auto r = juce::Rectangle<int>(350, 18, getWidth() - 368, getHeight() - 36);
    g.setColour(L.plate);
    g.fillRoundedRectangle(r.toFloat(), 6.0f);
    g.setColour(L.ink.withAlpha(0.8f));
    g.setFont(juce::Font(juce::FontOptions(14.0f)));
    g.drawFittedText("Audio interface, ASIO driver, inputs, outputs, sample rate and buffer size are set by your DAW "
                     "when Parabola runs as a plugin.\n\nOpen the Parabola Bass Z app (standalone) to pick them here.",
                     r.reduced(20), juce::Justification::centredLeft, 6, 1.0f);
  } else {
    g.setColour(L.border);
    g.drawVerticalLine(336, 16.0f, static_cast<float>(getHeight() - 16));
  }
}

void SettingsPanel::resized() {
  done_.setBounds(230, 16, 84, 26);
  for (int i = 0; i < looks_.size(); ++i) looks_[i]->setBounds(18, 76 + i * 29, 296, 25);
  hear_.setBounds(14, getHeight() - 34, 300, 26);
  if (selector_ != nullptr) {
    viewport_.setBounds(346, 8, getWidth() - 354, getHeight() - 16);
    selector_->setSize(viewport_.getWidth() - viewport_.getScrollBarThickness() - 4, juce::jmax(selector_->getHeight(), 400));
  }
}

// ---------------------------------------------------------------- manual
std::vector<ManualPanel::Section> ManualPanel::load() {
  std::vector<Section> out;
  const auto json = juce::JSON::parse(juce::String::fromUTF8(ParabolaBinary::manual_json, ParabolaBinary::manual_jsonSize));
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
    const auto& s = sections_[static_cast<size_t>(i)];
    auto* tab = tabs_.add(new juce::TextButton(s.title));
    tab->setRadioGroupId(4712);
    tab->setMouseCursor(juce::MouseCursor::PointingHandCursor);
    tab->setHelpText(juce::String(juce::CharPointer_UTF8("Manual \xc2\xb7 ")) + s.title + ": " + s.tagline);
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
  const auto& L = PbzLookAndFeel::lookOf(*this);
  const auto box = getLocalBounds().toFloat().reduced(0.5f);
  g.setColour(L.bg.withAlpha(0.95f));
  g.fillRoundedRectangle(box, 6.0f);
  g.setColour(L.border);
  g.drawRoundedRectangle(box, 6.0f, 1.0f);
  g.setColour(L.ink);
  g.setFont(PbzLookAndFeel::display(*this, 30.0f));
  g.drawText("MANUAL", 18, 10, 140, 36, juce::Justification::centredLeft, false);
  g.setColour(L.border);
  g.drawVerticalLine(226, 16.0f, static_cast<float>(getHeight() - 16));
}

void ManualPanel::resized() {
  done_.setBounds(140, 16, 74, 26);
  const int step = juce::jmin(34, juce::jmax(20, (getHeight() - 64) / juce::jmax(1, tabs_.size())));
  for (int i = 0; i < tabs_.size(); ++i) tabs_[i]->setBounds(14, 58 + i * step, 200, step - 4);
  viewport_.setBounds(240, 10, getWidth() - 248, getHeight() - 20);
  showSection(current_);
}

void ManualPanel::Page::build(const Section& s, int width) {
  const auto& L = PbzLookAndFeel::lookOf(*this);
  const auto body = juce::Font(juce::FontOptions(14.5f));
  const auto bold = juce::Font(juce::FontOptions(14.5f, juce::Font::bold));
  const auto head = PbzLookAndFeel::label(10.5f);
  const auto bodyInk = L.ink.withAlpha(0.9f);
  juce::AttributedString a;
  a.setWordWrap(juce::AttributedString::byWord);
  a.setLineSpacing(2.0f);
  a.append(s.title.toUpperCase() + "\n", PbzLookAndFeel::display(*this, 32.0f), L.ink);
  a.append(s.tagline + "\n", juce::Font(juce::FontOptions(15.5f)).italicised(), L.accent);
  const auto section = [&](const juce::String& heading, const juce::String& text) {
    if (text.isEmpty()) return;
    a.append("\n" + heading + "\n", head, L.muted);
    a.append(text + "\n", body, bodyInk);
  };
  section("WHAT IT DOES", s.what);
  section("HOW IT WORKS", s.how);
  section("THE SOUND", s.sound);
  if (!s.tries.isEmpty()) {
    a.append("\nTRY THIS\n", head, L.muted);
    for (int i = 0; i < s.tries.size(); ++i) {
      a.append(juce::String(i + 1) + ".  ", bold, L.accent);
      a.append(s.tries[i] + "\n", body, bodyInk);
    }
  }
  if (!s.controls.empty()) {
    a.append("\nCONTROLS\n", head, L.muted);
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

// ---------------------------------------------------------------- preset bar
PresetBar::PresetBar(ParabolaProcessor& processor, PresetStore& store)
    : processor_(processor),
      store_(store),
      prev_(juce::String(juce::CharPointer_UTF8("\xe2\x80\xb9")), "Previous preset."),
      next_(juce::String(juce::CharPointer_UTF8("\xe2\x80\xba")), "Next preset."),
      save_("SAVE", "Save: store the current settings and captures as one of your presets (type a name, Enter)."),
      del_("DEL", "Delete the current preset (yours only; click twice). Starter presets can't be deleted."),
      gear_("MY GEAR", "My Gear: browse every capture and IR in Documents\\Parabola Bass Z and Documents\\T3KJ Tones, "
                       "and load them with a click.") {
  for (auto* c : std::initializer_list<juce::Component*>{&prev_, &next_, &save_, &del_, &gear_, &name_})
    addAndMakeVisible(c);
  addChildComponent(editor_);
  name_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
  name_.setHelpText("Presets: click for the list. Starter presets find their captures by file name in your tones "
                    "folders; anything missing is named in this bar.");
  name_.onClick = [this] { showMenu(); };
  prev_.onClick = [this] { step(-1); };
  next_.onClick = [this] { step(1); };
  save_.onClick = [this] { beginSave(); };
  gear_.onClick = [this] {
    if (onMyGear) onMyGear();
  };
  del_.onClick = [this] {
    const auto current = processor_.params.state.getProperty("preset").toString();
    const auto* p = store_.byName(current);
    if (p == nullptr || p->factory) {
      if (onMessage) onMessage("Starter presets can't be deleted.");
      return;
    }
    if (delArmed_ == 0) {
      delArmed_ = 1;
      del_.setButtonText("SURE?");
      juce::Timer::callAfterDelay(3000, [safe = juce::Component::SafePointer<PresetBar>(this)] {
        if (safe != nullptr && safe->delArmed_ == 1) {
          safe->delArmed_ = 0;
          safe->del_.setButtonText("DEL");
        }
      });
      return;
    }
    delArmed_ = 0;
    del_.setButtonText("DEL");
    store_.remove(current);
    processor_.params.state.setProperty("preset", "", nullptr);
    if (onMessage) onMessage("Deleted " + current + ".");
    refresh();
  };
  editor_.onReturnKey = [this] { finishSave(true); };
  editor_.onEscapeKey = [this] { finishSave(false); };
  editor_.onFocusLost = [this] { finishSave(false); };
  refresh();
}

juce::String PresetBar::hint() const { return name_.getHelpText(); }

void PresetBar::refresh() {
  const auto current = processor_.params.state.getProperty("preset").toString();
  name_.setButtonText(current.isEmpty() ? "Presets" : current);
}

void PresetBar::load(const juce::String& name) {
  if (const auto* p = store_.byName(name)) {
    const auto missing = PresetStore::apply(*p, processor_);
    if (onMessage)
      onMessage(missing.isEmpty() ? "Loaded " + name + "."
                                  : "Loaded " + name + ", but couldn't find " + missing +
                                        ". Put it in Documents\\Parabola Bass Z (or T3KJ Tones) and load the preset again.");
  }
  refresh();
}

void PresetBar::step(int delta) {
  const auto& all = store_.all();
  if (all.empty()) return;
  int i = store_.indexOf(processor_.params.state.getProperty("preset").toString());
  i = i < 0 ? (delta > 0 ? 0 : static_cast<int>(all.size()) - 1)
            : (i + delta + static_cast<int>(all.size())) % static_cast<int>(all.size());
  load(all[static_cast<size_t>(i)].name);
}

void PresetBar::showMenu() {
  juce::PopupMenu menu;
  const auto current = processor_.params.state.getProperty("preset").toString();
  const auto& all = store_.all();
  menu.addSectionHeader("STARTER");
  for (size_t i = 0; i < all.size(); ++i)
    if (all[i].factory) menu.addItem(static_cast<int>(i) + 1, all[i].name, true, all[i].name == current);
  bool header = false;
  for (size_t i = 0; i < all.size(); ++i)
    if (!all[i].factory) {
      if (!header) menu.addSectionHeader("YOURS");
      header = true;
      menu.addItem(static_cast<int>(i) + 1, all[i].name, true, all[i].name == current);
    }
  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&name_),
                     [safe = juce::Component::SafePointer<PresetBar>(this)](int result) {
                       if (safe == nullptr || result <= 0) return;
                       const auto& list = safe->store_.all();
                       if (result - 1 < static_cast<int>(list.size()))
                         safe->load(list[static_cast<size_t>(result - 1)].name);
                     });
}

void PresetBar::beginSave() {
  const auto current = processor_.params.state.getProperty("preset").toString();
  const auto* p = store_.byName(current);
  editor_.setText(p != nullptr && !p->factory ? current : juce::String(), false);
  editor_.setTextToShowWhenEmpty("Preset name, Enter to save", PbzLookAndFeel::lookOf(*this).muted);
  editor_.setVisible(true);
  name_.setVisible(false);
  editor_.grabKeyboardFocus();
  editor_.selectAll();
}

void PresetBar::finishSave(bool keep) {
  if (!editor_.isVisible()) return;
  const auto name = editor_.getText().trim();
  editor_.setVisible(false);
  name_.setVisible(true);
  if (!keep || name.isEmpty()) return;
  if (const auto* p = store_.byName(name); p != nullptr && p->factory) {
    if (onMessage) onMessage("That's a starter preset's name; pick another.");
    return;
  }
  store_.save(name, processor_);
  processor_.params.state.setProperty("preset", name, nullptr);
  if (onMessage) onMessage("Saved " + name + ".");
  refresh();
}

void PresetBar::paint(juce::Graphics& g) {
  const auto& L = PbzLookAndFeel::lookOf(*this);
  g.setColour(L.plate);
  g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
  g.setColour(L.border);
  g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f, 1.0f);
  g.setColour(L.muted);
  g.setFont(PbzLookAndFeel::label(8.5f));
  g.drawText("PRESET", 12, 6, 100, 12, juce::Justification::centredLeft, false);
}

void PresetBar::resized() {
  const int y = 22, h = 28;
  const int gearW = 84, buttons = 46 + 4 + 46 + 8 + gearW;
  prev_.setBounds(8, y, 24, h);
  name_.setBounds(36, y, getWidth() - 36 - 4 - 24 - 8 - buttons - 8, h);
  editor_.setBounds(name_.getBounds());
  next_.setBounds(name_.getRight() + 4, y, 24, h);
  save_.setBounds(next_.getRight() + 8, y, 46, h);
  del_.setBounds(save_.getRight() + 4, y, 46, h);
  gear_.setBounds(del_.getRight() + 8, y, gearW, h);
}

// ---------------------------------------------------------------- my gear
MyGearPanel::MyGearPanel(ParabolaProcessor& processor) : processor_(processor) {
  addAndMakeVisible(search_);
  search_.setTextToShowWhenEmpty("Search captures and IRs", juce::Colours::grey);
  search_.onTextChange = [this] { filter(); };
  for (auto* b : {&done_, &rescan_, &open_, &folders_}) {
    addAndMakeVisible(b);
    b->setMouseCursor(juce::MouseCursor::PointingHandCursor);
  }
  done_.onClick = [this] {
    if (onClose) onClose();
  };
  rescan_.onClick = [this] { rescan(); };
  folders_.onClick = [this] { folderMenu(); };
  folders_.setHelpText("Folders: add the folders where you keep captures and IRs (any folder, subfolders included), "
                       "or remove ones you added. Presets look in them too.");
  open_.onClick = [] {
    const auto root = Library::parabolaRoot();
    root.getChildFile("NAMs").createDirectory();
    root.getChildFile("Cabs").createDirectory();
    root.startAsProcess();
  };
  rescan_.setHelpText("Rescan: look through the tones folders again (after you add files).");
  open_.setHelpText("Open Documents\\Parabola Bass Z in File Explorer.");
  list_.setModel(this);
  list_.setRowHeight(30);
  list_.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
  addAndMakeVisible(list_);
}

juce::String MyGearPanel::hint() const {
  return "My Gear: click a capture to load it into a band, an IR to load it into the cab. Looks in Documents\\Parabola "
         "Bass Z and Documents\\T3KJ Tones (including subfolders).";
}

void MyGearPanel::folderMenu() {
  juce::PopupMenu menu;
  menu.addItem(1, "Add a folder...");
  const auto added = Library::addedRoots();
  if (!added.empty()) {
    menu.addSeparator();
    for (size_t i = 0; i < added.size(); ++i) menu.addItem(100 + static_cast<int>(i), "Remove " + added[i].getFullPathName());
  }
  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&folders_),
                     [safe = juce::Component::SafePointer<MyGearPanel>(this), added](int result) {
                       if (safe == nullptr || result <= 0) return;
                       if (result >= 100) {
                         Library::removeRoot(added[static_cast<size_t>(result - 100)]);
                         safe->rescan();
                         return;
                       }
                       safe->chooser_ = std::make_unique<juce::FileChooser>(
                           "Add a folder with captures or IRs",
                           juce::File::getSpecialLocation(juce::File::userHomeDirectory));
                       safe->chooser_->launchAsync(
                           juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                           [safe](const juce::FileChooser& fc) {
                             if (safe == nullptr) return;
                             const auto folder = fc.getResult();
                             if (!folder.isDirectory()) return;
                             Library::addRoot(folder);
                             safe->rescan();
                             if (safe->onMessage) safe->onMessage("My Gear now looks in " + folder.getFullPathName() + ".");
                           });
                     });
}

void MyGearPanel::rescan() {
  all_ = Library::scan();
  filter();
}

void MyGearPanel::filter() {
  const auto words = juce::StringArray::fromTokens(search_.getText().toLowerCase(), " ", "");
  shown_.clear();
  for (const auto& e : all_) {
    const auto hay = (e.name + " " + e.pack).toLowerCase();
    bool ok = true;
    for (const auto& w : words)
      if (w.isNotEmpty() && !hay.contains(w)) ok = false;
    if (ok) shown_.push_back(e);
  }
  list_.updateContent();
  repaint();
}

void MyGearPanel::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) {
  if (row < 0 || row >= static_cast<int>(shown_.size())) return;
  const auto& L = PbzLookAndFeel::lookOf(*this);
  const auto& e = shown_[static_cast<size_t>(row)];
  if (selected || list_.getRowContainingPosition(list_.getMouseXYRelative().x, list_.getMouseXYRelative().y) == row) {
    g.setColour(L.ink.withAlpha(0.08f));
    g.fillRect(0, 0, width, height);
  }
  const auto tag = juce::Rectangle<float>(6.0f, (height - 17) / 2.0f, 34.0f, 17.0f);
  g.setColour(e.isNam ? L.accent : L.ink);
  g.fillRoundedRectangle(tag, 2.0f);
  g.setColour(juce::Colour(0xff05080d));
  g.setFont(PbzLookAndFeel::label(9.0f));
  g.drawText(e.isNam ? "NAM" : "IR", tag, juce::Justification::centred, false);
  g.setColour(L.ink);
  g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
  g.drawText(e.name, 50, 0, width / 2 - 50, height, juce::Justification::centredLeft, true);
  g.setColour(L.muted);
  g.setFont(juce::Font(juce::FontOptions(12.0f)));
  g.drawText(e.pack, width / 2, 0, width / 2 - 10, height, juce::Justification::centredLeft, true);
}

void MyGearPanel::listBoxItemClicked(int row, const juce::MouseEvent&) {
  if (row < 0 || row >= static_cast<int>(shown_.size())) return;
  const auto entry = shown_[static_cast<size_t>(row)];
  if (!entry.isNam) {
    const auto error = processor_.loadIr(entry.file);
    if (error.isEmpty()) Library::rememberFolder(entry.file);
    if (onMessage) onMessage(error.isEmpty() ? "Cab: " + entry.name + "." : error);
    return;
  }
  juce::PopupMenu menu;
  menu.addSectionHeader(entry.name);
  for (int b = 0; b < kNumBands; ++b)
    menu.addItem(b + 1, "Load into " + juce::String(kBandNames[b]).toUpperCase());
  menu.showMenuAsync(juce::PopupMenu::Options().withMousePosition(),
                     [safe = juce::Component::SafePointer<MyGearPanel>(this), entry](int result) {
                       if (safe == nullptr || result <= 0) return;
                       const auto error = safe->processor_.loadNam(result - 1, entry.file);
                       if (error.isEmpty()) Library::rememberFolder(entry.file);
                       if (safe->onMessage)
                         safe->onMessage(error.isEmpty() ? juce::String(kBandNames[result - 1]) + ": " + entry.name + "."
                                                         : error);
                     });
}

void MyGearPanel::paint(juce::Graphics& g) {
  const auto& L = PbzLookAndFeel::lookOf(*this);
  const auto box = getLocalBounds().toFloat().reduced(0.5f);
  g.setColour(L.bg.withAlpha(0.95f));
  g.fillRoundedRectangle(box, 6.0f);
  g.setColour(L.border);
  g.drawRoundedRectangle(box, 6.0f, 1.0f);
  g.setColour(L.ink);
  g.setFont(PbzLookAndFeel::display(*this, 30.0f));
  g.drawText("MY GEAR", 18, 10, 160, 36, juce::Justification::centredLeft, false);
  g.setColour(L.muted);
  g.setFont(juce::Font(juce::FontOptions(12.0f)));
  juce::StringArray roots;
  for (const auto& r : Library::roots()) roots.add(r.getFullPathName());
  const int nams = static_cast<int>(std::count_if(all_.begin(), all_.end(), [](const auto& e) { return e.isNam; }));
  const auto text = roots.isEmpty()
                        ? juce::String("No tones folder yet. Click FOLDERS to add the folder where you keep your captures, "
                                       "or OPEN FOLDER to make Documents\\Parabola Bass Z.")
                        : juce::String(nams) + " captures, " + juce::String(static_cast<int>(all_.size()) - nams) +
                              " IRs in " + roots.joinIntoString(" + ");
  g.drawFittedText(text, 18, 50, getWidth() - 36, 16, juce::Justification::centredLeft, 1, 0.8f);
  if (shown_.empty() && !all_.empty()) {
    g.drawText("Nothing matches that search.", list_.getBounds(), juce::Justification::centred, false);
  }
}

void MyGearPanel::resized() {
  done_.setBounds(getWidth() - 18 - 80, 16, 80, 26);
  open_.setBounds(done_.getX() - 8 - 120, 16, 120, 26);
  rescan_.setBounds(open_.getX() - 8 - 90, 16, 90, 26);
  folders_.setBounds(rescan_.getX() - 8 - 96, 16, 96, 26);
  search_.setBounds(190, 16, folders_.getX() - 190 - 12, 26);
  list_.setBounds(12, 74, getWidth() - 24, getHeight() - 84);
}

// ---------------------------------------------------------------- tuner
// ---------------------------------------------------------------- rack
juce::String RackPanel::Fader::hint() const {
  const double v = getValue();
  return "DI Shaper " + label + " Hz: boost or cut around this frequency on the DI, before every band. "
         + (std::abs(v) < 0.05 ? juce::String("0 dB (flat)") : (v > 0 ? "+" : "") + juce::String(v, 1) + " dB") +
         ". Double-click resets.";
}

RackPanel::RackPanel(ParabolaProcessor& processor)
    : processor_(processor),
      diOn_(processor.params, "rackDiOn", "DI Shaper", 2,
            "DI Shaper: seven bands of EQ on your DI before the crossover, so every band and capture hears the change."),
      subOn_(processor.params, "rackSubOn", "Sub Comp", 0,
             "Sub Comp: a compressor on only the lows below FREQ. A steady, even bottom without squashing your attack."),
      driveOn_(processor.params, "rackDriveOn", "Split Drive", 1,
               "Split Drive: a built-in drive on everything above SPLIT, clean below. Grit without captures, or under them."),
      subAmount_(processor.params, "rackSubAmount", "Amount", 66,
                 "Amount: how hard the lows are squeezed (lower threshold, higher ratio, more makeup)."),
      subFreq_(processor.params, "rackSubFreq", "Freq", 62, "Freq: only the lows below this are compressed."),
      subBlend_(processor.params, "rackSubBlend", "Blend", 62,
                "Blend: compressed lows against the untouched lows. 100% is all compressed."),
      drive_(processor.params, "rackDrive", "Drive", 92, "Drive: how hard the highs are pushed into the clipper."),
      split_(processor.params, "rackDriveSplit", "Split", 64,
             "Split: everything below this stays clean, everything above gets the drive (150 to 500 Hz)."),
      tone_(processor.params, "rackDriveTone", "Tone", 64, "Tone: a low-pass on the drive. Lower is darker and less fizzy."),
      blend_(processor.params, "rackDriveBlend", "Blend", 64,
             "Blend: driven highs against the clean highs. The lows below Split are always clean."),
      level_(processor.params, "rackDriveLevel", "Level", 64, "Level: trims the drive."),
      solo_(processor.params, "rackSolo", "SOLO",
            "Solo: hear the rack's output alone (no bands, cab or EQ), to dial in the DI. Turn it off to hear everything again.") {
  for (auto* c : std::initializer_list<juce::Component*>{&diOn_, &subOn_, &driveOn_, &subAmount_, &subFreq_, &subBlend_, &drive_,
                                                          &split_, &tone_, &blend_, &level_, &solo_, &done_})
    addAndMakeVisible(c);
  subAmount_.setColourIndex(0); subFreq_.setColourIndex(0); subBlend_.setColourIndex(0);
  for (auto* k : {&drive_, &split_, &tone_, &blend_, &level_}) k->setColourIndex(1);
  level_.setBipolar();
  for (int i = 0; i < DiShaper::kBands; ++i) {
    auto& f = faders_[static_cast<size_t>(i)];
    const double hz = DiShaper::kHz[static_cast<size_t>(i)];
    f.label = hz >= 1000.0 ? juce::String(hz / 1000.0, 1) + "K" : juce::String(juce::roundToInt(hz));
    f.setSliderStyle(juce::Slider::LinearVertical);
    f.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    f.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    f.setDoubleClickReturnValue(true, 0.0);
    f.setTitle("DI Shaper " + f.label);
    f.getProperties().set(kParamIdProp, "rackDi" + juce::String(i));
    f.getProperties().set("band", 2);
    addAndMakeVisible(f);
    faderAtt_[static_cast<size_t>(i)] =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.params, "rackDi" + juce::String(i), f);
  }
  done_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
  done_.onClick = [this] {
    if (onClose) onClose();
  };
}

void RackPanel::visibilityChanged() {
  if (isVisible()) startTimerHz(20);
  else stopTimer();
}

void RackPanel::timerCallback() {
  const float gr = processor_.subCompReductionDb.load();
  const float shown = gr > grShown_ ? gr : juce::jmax(gr, grShown_ - 0.6f);
  if (std::abs(shown - grShown_) > 0.05f) {
    grShown_ = shown;
    repaint(meterBox_.expanded(4));
  }
}

void RackPanel::paint(juce::Graphics& g) {
  const auto& L = PbzLookAndFeel::lookOf(*this);
  drawPlate(g, *this, getLocalBounds().toFloat());
  g.setColour(L.ink);
  g.setFont(PbzLookAndFeel::display(*this, 30.0f));
  g.drawText("RACK", 22, 12, 120, 40, juce::Justification::centredLeft, false);
  g.setColour(L.muted);
  g.setFont(PbzLookAndFeel::label(10.5f));
  g.drawText("ON YOUR DI, BEFORE THE BANDS  /  SIGNAL FLOWS LEFT TO RIGHT", 112, 26, 460, 14, juce::Justification::centredLeft,
             false);
  const auto unit = [&](juce::Rectangle<int> box, const char* name, const char* sub) {
    g.setColour(L.scrim.withAlpha(0.35f));
    g.fillRoundedRectangle(box.toFloat(), 8.0f);
    g.setColour(L.border);
    g.drawRoundedRectangle(box.toFloat().reduced(0.5f), 8.0f, 1.0f);
    g.setColour(L.ink);
    g.setFont(PbzLookAndFeel::display(*this, 20.0f));
    g.drawText(name, box.getX() + 46, box.getY() + 12, box.getWidth() - 60, 26, juce::Justification::centredLeft, false);
    g.setColour(L.muted);
    g.setFont(PbzLookAndFeel::label(9.5f));
    g.drawFittedText(sub, box.getX() + 16, box.getBottom() - 46, box.getWidth() - 32, 36, juce::Justification::centredLeft, 3, 1.0f);
  };
  unit(diBox_, "DI SHAPER", "Seven bands on the DI that every band and capture hears. Cut boom, add clank, tame the pick.");
  unit(subBox_, "SUB COMP", "Compresses only the lows below FREQ, so the bottom sits still while the attack stays.");
  unit(driveBox_, "SPLIT DRIVE", "Clean below SPLIT, driven above it. Grit without loading captures, or under them.");
  // arrows between units
  g.setColour(L.muted);
  for (auto* b : {&diBox_, &subBox_}) {
    const float y = static_cast<float>(b->getCentreY()), x = static_cast<float>(b->getRight()) + 3.0f;
    juce::Path p;
    p.addTriangle(x, y - 6.0f, x, y + 6.0f, x + 9.0f, y);
    g.fillPath(p);
  }
  // DI fader labels
  g.setColour(L.ink);
  g.setFont(PbzLookAndFeel::label(9.5f));
  for (auto& f : faders_)
    g.drawText(f.label, f.getX() - 10, f.getBottom() + 4, f.getWidth() + 20, 14, juce::Justification::centred, false);
  // sub comp gain-reduction meter
  g.setColour(L.track);
  g.fillRoundedRectangle(meterBox_.toFloat(), 3.0f);
  g.setColour(bandColour(*this, 0));
  const float frac = juce::jlimit(0.0f, 1.0f, grShown_ / 18.0f);
  g.fillRoundedRectangle(meterBox_.toFloat().withWidth(meterBox_.getWidth() * frac), 3.0f);
  g.setColour(L.muted);
  g.setFont(PbzLookAndFeel::label(9.5f));
  g.drawText("SUB REDUCTION  " + juce::String(grShown_, 1) + " DB", meterBox_.getX(), meterBox_.getY() - 16,
             meterBox_.getWidth(), 14, juce::Justification::centredLeft, false);
}

void RackPanel::resized() {
  const int pad = 18, top = 64, gap = 22, bottom = 18;
  done_.setBounds(getWidth() - pad - 90, 16, 90, 32);
  solo_.setBounds(done_.getX() - 12 - 80, 20, 80, 26);
  const int w = getWidth() - 2 * pad - 2 * gap, h = getHeight() - top - bottom;
  const int diW = w * 40 / 100, subW = w * 26 / 100, drW = w - diW - subW;
  diBox_ = {pad, top, diW, h};
  subBox_ = {diBox_.getRight() + gap, top, subW, h};
  driveBox_ = {subBox_.getRight() + gap, top, drW, h};
  for (auto [led, box] : {std::pair{&diOn_, diBox_}, std::pair{&subOn_, subBox_}, std::pair{&driveOn_, driveBox_}})
    led->setTopLeftPosition(box.getX() + 14, box.getY() + 12);
  // DI faders
  const int fx0 = diBox_.getX() + 20, fw = diBox_.getWidth() - 40, step = fw / DiShaper::kBands;
  for (int i = 0; i < DiShaper::kBands; ++i)
    faders_[static_cast<size_t>(i)].setBounds(fx0 + i * step + (step - 30) / 2, diBox_.getY() + 56, 30, h - 56 - 82);
  // sub comp: three knobs in a column-ish row, meter under
  {
    const int kw = subAmount_.getWidth(), cx = subBox_.getCentreX();
    subAmount_.setTopLeftPosition(cx - kw / 2, subBox_.getCentreY() - 150);
    subFreq_.setTopLeftPosition(cx - kw - 8, subAmount_.getBottom() + 14);
    subBlend_.setTopLeftPosition(cx + 8, subAmount_.getBottom() + 14);
    meterBox_ = {subBox_.getX() + 18, subFreq_.getBottom() + 34, subBox_.getWidth() - 36, 10};
  }
  // split drive: Drive big on the left, the rest in a 2x2 grid
  {
    const int kw = split_.getWidth(), groupW = drive_.getWidth() + 26 + 2 * kw + 16;
    const int y0 = driveBox_.getCentreY() - 130, x0 = driveBox_.getCentreX() - groupW / 2;
    drive_.setTopLeftPosition(x0, y0 + 50);
    const int gx = drive_.getRight() + 26;
    split_.setTopLeftPosition(gx, y0);
    tone_.setTopLeftPosition(gx + kw + 16, y0);
    blend_.setTopLeftPosition(gx, split_.getBottom() + 18);
    level_.setTopLeftPosition(gx + kw + 16, split_.getBottom() + 18);
  }
}

TunerPanel::TunerPanel(ParabolaProcessor& processor) : processor_(processor) {
  addAndMakeVisible(done_);
  done_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
  done_.onClick = [this] {
    if (onClose) onClose();
  };
}

void TunerPanel::visibilityChanged() {
  processor_.setTunerActive(isVisible());
  if (isVisible()) startTimerHz(30);
  else stopTimer();
}

void TunerPanel::timerCallback() {
  const auto reading = processor_.tuner.getReading();
  const double hz = reading["frequency"];
  if (hz > 0.0) {
    const double midi = 69.0 + 12.0 * std::log2(hz / 440.0);
    const double cents = (midi - std::round(midi)) * 100.0;
    smoothCents_ = hz_ > 0.0 ? smoothCents_ + 0.35 * (cents - smoothCents_) : cents;
  }
  if (hz != hz_) {
    hz_ = hz;
    repaint();
  }
}

void TunerPanel::paint(juce::Graphics& g) {
  const auto& L = PbzLookAndFeel::lookOf(*this);
  const auto box = getLocalBounds().toFloat().reduced(0.5f);
  g.setColour(L.bg.withAlpha(0.95f));
  g.fillRoundedRectangle(box, 6.0f);
  g.setColour(L.border);
  g.drawRoundedRectangle(box, 6.0f, 1.0f);
  g.setColour(L.ink);
  g.setFont(PbzLookAndFeel::display(*this, 30.0f));
  g.drawText("TUNER", 18, 10, 160, 36, juce::Justification::centredLeft, false);
  g.setColour(L.muted);
  g.setFont(PbzLookAndFeel::label(10.0f));
  g.drawText("OUTPUT MUTED WHILE TUNING", 0, 18, getWidth() - 120, 20, juce::Justification::centredRight, false);

  const auto centre = getLocalBounds().getCentre();
  static const char* const names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  juce::String note = "-";
  int octave = 0;
  if (hz_ > 0.0) {
    const int midi = juce::roundToInt(69.0 + 12.0 * std::log2(hz_ / 440.0));
    note = names[((midi % 12) + 12) % 12];
    octave = midi / 12 - 1;
  }
  const bool inTune = hz_ > 0.0 && std::abs(smoothCents_) < 3.0;
  g.setColour(hz_ <= 0.0 ? L.muted : inTune ? juce::Colour(0xff3ddc84) : L.ink);
  g.setFont(PbzLookAndFeel::display(*this, 150.0f));
  g.drawText(note, centre.x - 200, centre.y - 150, 400, 170, juce::Justification::centred, false);
  if (hz_ > 0.0) {
    g.setFont(juce::Font(juce::FontOptions(15.0f)));
    g.setColour(L.muted);
    g.drawText(juce::String(octave) + juce::String(juce::CharPointer_UTF8("  \xc2\xb7  ")) + juce::String(hz_, 1) + juce::String(juce::CharPointer_UTF8(" Hz  \xc2\xb7  ")) +
                   (smoothCents_ >= 0 ? "+" : "") + juce::String(juce::roundToInt(smoothCents_)) + " cents",
               centre.x - 200, centre.y + 22, 400, 22, juce::Justification::centred, false);
  } else {
    g.setColour(L.muted);
    g.setFont(juce::Font(juce::FontOptions(15.0f)));
    g.drawText("Play an open string", centre.x - 200, centre.y + 22, 400, 22, juce::Justification::centred, false);
  }
  // Cents scale, -50 .. +50.
  const auto scale = juce::Rectangle<float>(static_cast<float>(centre.x - 300), static_cast<float>(centre.y + 70), 600.0f, 34.0f);
  g.setColour(L.track);
  g.fillRoundedRectangle(scale.withSizeKeepingCentre(scale.getWidth(), 4.0f), 2.0f);
  for (int c = -50; c <= 50; c += 10) {
    const float x = scale.getX() + scale.getWidth() * static_cast<float>(c + 50) / 100.0f;
    g.setColour(c == 0 ? L.ink : L.muted);
    g.fillRect(x - (c == 0 ? 1.5f : 0.5f), scale.getY() + (c == 0 ? 0.0f : 9.0f), c == 0 ? 3.0f : 1.0f, c == 0 ? 34.0f : 16.0f);
  }
  if (hz_ > 0.0) {
    const float x = scale.getX() + scale.getWidth() * static_cast<float>(juce::jlimit(-50.0, 50.0, smoothCents_) + 50.0) / 100.0f;
    g.setColour(inTune ? juce::Colour(0xff3ddc84) : L.accent);
    g.fillRoundedRectangle(juce::Rectangle<float>(10.0f, 40.0f).withCentre({x, scale.getCentreY()}), 3.0f);
  }
}

void TunerPanel::resized() { done_.setBounds(getWidth() - 18 - 84, 16, 84, 26); }

// ---------------------------------------------------------------- view
ParabolaView::ParabolaView(ParabolaProcessor& processor)
    : processor_(processor),
      gate_(processor.params, "gate", "Gate", 40,
            "Gate: silences hum and string noise between notes, on your DI before the bands. Fully down is off."),
      noise_(processor.params, "driveGate", "Noise", 40,
             "Noise: a second gate after the mid and high bands, following your DI. It shuts the drive bands' hiss "
             "as notes fade while the low band keeps ringing clean. Fully down is off; start around -55 dB."),
      pitch_(processor.params, "pitch", "Pitch", 40,
             "Pitch: transpose everything up or down, in semitones, before the bands (drop-tune without retuning). "
             "Off at 0; on, it adds about 16 ms of delay."),
      input_(processor.params, "input", "Input", 40, "Input: level into Parabola (and into every band's capture)."),
      output_(processor.params, "output", "Output", 40, "Output: level out of Parabola."),
      help_(IconButton::Kind::help, "Manual: what every part does, how it works, and settings to try."),
      gear_(IconButton::Kind::gear, "Settings: the skin, and in the standalone app your audio interface and latency."),
      tuner_("TUNER", "Tuner: opens the tuner and mutes the output while it's open."),
      hushChip_("HUSH", "Hush: the de-noiser on your DI (zero latency). Opens its panel: on / off, LEARN, Strength, "
                        "Sensitivity, Release."),
      rackChip_("RACK", "Rack: units on your DI before the bands. DI Shaper (7-band EQ), Sub Comp (compresses only the lows) "
                        "and Split Drive (drive above a split, clean below), plus SOLO."),
      presetBar_(processor, presets_),
      crossover_(processor),
      geq_(processor.params),
      cab_(processor),
      settings_(processor, [this](const juce::String& id) { setLook(id); }),
      myGear_(processor),
      tunerPanel_(processor),
      hushPanel_(processor.params, processor.hush,
                 [](const juce::Component& c, float px) { return PbzLookAndFeel::display(c, px); }, true),
      rackPanel_(processor) {
  setLookAndFeel(&lnf_);
  pitch_.setBipolar();
  input_.setBipolar();
  output_.setBipolar();
  for (auto* k : {&gate_, &noise_, &pitch_, &input_, &output_}) k->setSize(56, k->getHeight());
  for (auto* c : std::initializer_list<juce::Component*>{&gate_, &noise_, &pitch_, &input_, &output_, &help_, &gear_, &tuner_, &hushChip_, &rackChip_,
                                                          &presetBar_, &crossover_, &geq_, &cab_})
    addAndMakeVisible(c);
  for (int b = 0; b < kNumBands; ++b) {
    bands_[static_cast<size_t>(b)] = std::make_unique<BandStrip>(processor, b);
    addAndMakeVisible(*bands_[static_cast<size_t>(b)]);
  }
  help_.onClick = [this] { showManual(!manual_.isVisible()); };
  gear_.onClick = [this] { showSettings(!settings_.isVisible()); };
  tuner_.onClick = [this] { showTuner(!tunerPanel_.isVisible()); };
  hushChip_.onClick = [this] { showHush(!hushPanel_.isVisible()); };
  hushPanel_.onClose = [this] { showHush(false); };
  rackChip_.onClick = [this] { showRack(!rackPanel_.isVisible()); };
  rackPanel_.onClose = [this] { showRack(false); };
  presetBar_.onMessage = [this](const juce::String& m) { flash(m); };
  presetBar_.onMyGear = [this] { showMyGear(!myGear_.isVisible()); };
  myGear_.onMessage = [this](const juce::String& m) { flash(m, 150); };
  for (auto* c : std::initializer_list<juce::Component*>{&settings_, &manual_, &myGear_, &tunerPanel_, &hushPanel_, &rackPanel_})
    addChildComponent(c);
  settings_.onClose = [this] { showSettings(false); };
  manual_.onClose = [this] { showManual(false); };
  myGear_.onClose = [this] { showMyGear(false); };
  tunerPanel_.onClose = [this] { showTuner(false); };
  addMouseListener(this, true);  // right-clicks anywhere come here (MIDI learn)
  hint_ = kDefaultHint;
  setLook(processor.params.state.getProperty(kLookProp, "foil").toString());
  setSize(kWidth, kHeight);
  startTimerHz(30);
}

ParabolaView::~ParabolaView() { setLookAndFeel(nullptr); }

void ParabolaView::setLook(const juce::String& lookId) {
  const auto& look = lookFor(lookId);
  processor_.params.state.setProperty(kLookProp, look.id, nullptr);
  lnf_.setLook(look);
  settings_.syncLook(look.id);
  photo_ = loadPhoto(look.id);
  photoScaled_ = {};
  sendLookAndFeelChange();
  repaint();
}

void ParabolaView::showManual(bool show, int section) {
  if (show && section >= 0) manual_.showSection(section);
  showOverlay(show ? &manual_ : nullptr);
}

void ParabolaView::showMyGear(bool show) {
  if (show) myGear_.rescan();
  showOverlay(show ? &myGear_ : nullptr);
}

void ParabolaView::flash(const juce::String& message, int ticks) {
  flash_ = message;
  flashTicks_ = ticks;
  updateHint();
}

void ParabolaView::mouseDown(const juce::MouseEvent& e) {
  if (!e.mods.isPopupMenu()) return;
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
    menu.addSectionHeader(param != nullptr ? param->getName(64) : id);
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

void ParabolaView::paintOverChildren(juce::Graphics& g) {
  if (learnTarget_ == nullptr) return;
  const auto r = getLocalArea(learnTarget_, learnTarget_->getLocalBounds()).toFloat().expanded(4.0f);
  g.setColour(lnf_.look().accent);
  g.drawRoundedRectangle(r, 6.0f, 2.5f);
}

void ParabolaView::showAdvanced(bool show) {
  for (auto& b : bands_) b->setBack(show);
}

void ParabolaView::showOverlay(juce::Component* overlay) {
  settings_.setVisible(overlay == &settings_);
  manual_.setVisible(overlay == &manual_);
  myGear_.setVisible(overlay == &myGear_);
  tunerPanel_.setVisible(overlay == &tunerPanel_);
  hushPanel_.setVisible(overlay == &hushPanel_);
  rackPanel_.setVisible(overlay == &rackPanel_);
  hushChip_.setToggleState(overlay == &hushPanel_, juce::dontSendNotification);
  rackChip_.setToggleState(overlay == &rackPanel_, juce::dontSendNotification);
  gear_.setToggleState(overlay == &settings_, juce::dontSendNotification);
  help_.setToggleState(overlay == &manual_, juce::dontSendNotification);
  tuner_.setToggleState(overlay == &tunerPanel_, juce::dontSendNotification);
  const bool controls = overlay == nullptr;
  crossover_.setVisible(controls);
  geq_.setVisible(controls);
  cab_.setVisible(controls);
  for (auto& b : bands_) b->setVisible(controls);
  if (overlay != nullptr) overlay->toFront(false);
  repaint();
}

void ParabolaView::timerCallback() {
  const float db = processor_.outputPeakDb.load();
  const float shown = db > meterDb_ ? db : juce::jmax(db, meterDb_ - 1.5f);
  if (std::abs(shown - meterDb_) > 0.2f) {
    meterDb_ = shown;
    repaint(meterBox_);
  }
  if (const int v = processor_.filesVersion(); v != filesSeen_) {
    filesSeen_ = v;
    for (auto& b : bands_) b->refreshSlot();
    cab_.refresh();
    presetBar_.refresh();
  }
  if (const int learned = processor_.learnedCount.load(); learned != learnedSeen_) {
    learnedSeen_ = learned;
    if (learnTarget_ != nullptr) {
      const auto id = learnTarget_->getProperties()[kParamIdProp].toString();
      flash(learnName_ + " is on MIDI CC " + juce::String(processor_.ccFor(id)) + ". Right-click it to forget.", 120);
    }
    learnTarget_ = nullptr;
    repaint();
  }
  if (learnTarget_ != nullptr && processor_.learning().isEmpty()) {  // cancelled
    learnTarget_ = nullptr;
    repaint();
  }
  if (flashTicks_ > 0 && --flashTicks_ == 0) flash_ = {};
  hushChip_.setButtonText(processor_.params.getRawParameterValue("hushOn")->load() > 0.5f ? "HUSH ON" : "HUSH");
  rackChip_.setButtonText(processor_.rackActive() ? "RACK ON" : "RACK");
  crossover_.tick();
  updateHint();
}

void ParabolaView::updateHint() {
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
  else if (flash_.isNotEmpty())
    text = flash_;
  if (text != hint_) {
    hint_ = text;
    repaint(hintBox_);
  }
}

void ParabolaView::paint(juce::Graphics& g) {
  const auto& L = lnf_.look();
  g.fillAll(L.bg);
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
      sg.setColour(L.scrim);
      sg.fillAll();
      photoScaled_ = out;
      photoScaledAt_ = scale;
    }
    g.drawImage(photoScaled_, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
  }

  // Wordmark: PARABOLA BASS Z, "BASS Z" in the accent.
  const auto font = PbzLookAndFeel::display(*this, 50.0f);
  g.setFont(font);
  const float w1 = juce::GlyphArrangement::getStringWidth(font, "PARABOLA ");
  for (int pass = 0; pass < 2; ++pass) {
    const float off = pass == 0 ? 2.0f : 0.0f;
    g.setColour(pass == 0 ? juce::Colours::black.withAlpha(0.45f) : L.ink);
    g.drawText("PARABOLA ", juce::Rectangle<float>(kPad + off, 18.0f + off, w1 + 4, 56.0f), juce::Justification::topLeft, false);
    g.setColour(pass == 0 ? juce::Colours::black.withAlpha(0.45f) : L.accent);
    g.drawText("BASS Z", juce::Rectangle<float>(kPad + w1 + off, 18.0f + off, 220.0f, 56.0f), juce::Justification::topLeft, false);
  }
  g.setColour(L.muted);
  g.setFont(PbzLookAndFeel::label(10.0f));
  g.drawText(juce::String(juce::CharPointer_UTF8("3-BAND PARALLEL BASS \xc2\xb7 NAM")), kPad, 74, 400, 16, juce::Justification::centredLeft, false);

  // Header plate.
  g.setColour(L.plate);
  g.fillRoundedRectangle(headPlate_.toFloat(), 6.0f);
  g.setColour(L.border);
  g.drawRoundedRectangle(headPlate_.toFloat().reduced(0.5f), 6.0f, 1.0f);
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
  g.setColour(L.border);
  g.fillRect(hintBox_.getX(), hintBox_.getY(), hintBox_.getWidth(), 1);
  g.setColour(L.muted);
  g.setFont(juce::Font(juce::FontOptions(12.5f)));
  g.drawText(hint_, hintBox_.reduced(kPad, 0), juce::Justification::centredLeft, true);
}

void ParabolaView::resized() {
  hintBox_ = {0, getHeight() - kHintH, getWidth(), kHintH};
  const int right = getWidth() - kPad;
  gear_.setTopRightPosition(right - 10, (kHeaderH - gear_.getHeight()) / 2);
  help_.setTopRightPosition(gear_.getX() - 6, gear_.getY());
  meterBox_ = {help_.getX() - 20, (kHeaderH - 56) / 2, 9, 56};
  tuner_.setBounds(meterBox_.getX() - 12 - 66, kHeaderH / 2 - 34, 66, 20);
  hushChip_.setBounds(tuner_.getX(), kHeaderH / 2 - 10, 66, 20);
  rackChip_.setBounds(tuner_.getX(), kHeaderH / 2 + 14, 66, 20);
  output_.setTopRightPosition(tuner_.getX() - 8, (kHeaderH - output_.getHeight()) / 2 + 2);
  input_.setTopRightPosition(output_.getX() - 2, output_.getY());
  pitch_.setTopRightPosition(input_.getX() - 2, output_.getY());
  noise_.setTopRightPosition(pitch_.getX() - 2, output_.getY());
  gate_.setTopRightPosition(noise_.getX() - 2, output_.getY());
  headPlate_ = {gate_.getX() - 8, 16, right - gate_.getX() + 8, kHeaderH - 32};
  presetBar_.setBounds(286, 22, juce::jmin(470, headPlate_.getX() - 12 - 286), 60);

  const int w = getWidth() - 2 * kPad;
  crossover_.setBounds(kPad, kXoverY, w, kXoverH);
  const int gap = 12;
  const int bw = (w - 2 * gap) / 3;
  for (int b = 0; b < kNumBands; ++b) bands_[static_cast<size_t>(b)]->setBounds(kPad + b * (bw + gap), kBandsY, bw, kBandsH);
  const int geqW = (w - gap) * 2 / 3;
  geq_.setBounds(kPad, kBottomY, geqW, kBottomH);
  cab_.setBounds(kPad + geqW + gap, kBottomY, w - geqW - gap, kBottomH);
  settings_.setBounds(kPad, kXoverY, w, kBottomY + kBottomH - kXoverY);
  for (auto* c : std::initializer_list<juce::Component*>{&manual_, &myGear_, &tunerPanel_, &hushPanel_, &rackPanel_})
    c->setBounds(settings_.getBounds());
  photoScaled_ = {};
}

// ---------------------------------------------------------------- window
ParabolaEditor::ParabolaEditor(ParabolaProcessor& processor)
    : juce::AudioProcessorEditor(processor), processor_(processor), view_(processor) {
  addAndMakeVisible(view_);
  constexpr double kAspect = static_cast<double>(ParabolaView::kWidth) / ParabolaView::kHeight;
  setResizable(true, true);
  setResizeLimits(ParabolaView::kWidth * 6 / 10, ParabolaView::kHeight * 6 / 10, ParabolaView::kWidth * 2,
                  ParabolaView::kHeight * 2);
  if (auto* c = getConstrainer()) c->setFixedAspectRatio(kAspect);
  const int width = juce::jlimit(ParabolaView::kWidth * 6 / 10, ParabolaView::kWidth * 2,
                                 static_cast<int>(processor.params.state.getProperty(kUiWidthProp, ParabolaView::kWidth)));
  setSize(width, juce::roundToInt(width / kAspect));
}

void ParabolaEditor::resized() {
  const float scale = static_cast<float>(getWidth()) / static_cast<float>(ParabolaView::kWidth);
  view_.setBounds(0, 0, ParabolaView::kWidth, ParabolaView::kHeight);
  view_.setTransform(juce::AffineTransform::scale(scale));
  processor_.params.state.setProperty(kUiWidthProp, getWidth(), nullptr);
}

}  // namespace parabola
