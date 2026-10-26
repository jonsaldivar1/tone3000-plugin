#include "HushEditor.h"

#include "HushBinary.h"

const juce::Colour HushLookAndFeel::bg{0xff0c0e11};
const juce::Colour HushLookAndFeel::ink{0xffe8ecef};
const juce::Colour HushLookAndFeel::accent{0xff3ee0b0};
const juce::Colour HushLookAndFeel::plate{0xff15191e};

namespace {
const juce::Identifier kUiWidthProp{"uiWidth"};
constexpr int kHeaderH = 70, kHintH = 28, kPad = 18;
const juce::String kDefaultHint = "Hover any control to see what it does. Double-click a knob to reset it.";
}  // namespace

HushLookAndFeel::HushLookAndFeel()
    : anton_(juce::Typeface::createSystemTypefaceFor(HushBinary::AntonRegular_ttf, HushBinary::AntonRegular_ttfSize)) {
  setColour(juce::ResizableWindow::backgroundColourId, plate);
  setColour(juce::Label::textColourId, ink);
  setColour(juce::Slider::thumbColourId, accent);
  setColour(juce::TextButton::buttonColourId, plate.brighter(0.15f));
  setColour(juce::TextButton::buttonOnColourId, accent);
  setColour(juce::TextButton::textColourOffId, ink);
  setColour(juce::TextButton::textColourOnId, juce::Colours::black);
  setColour(juce::PopupMenu::backgroundColourId, plate);
  setColour(juce::PopupMenu::textColourId, ink);
  setColour(juce::PopupMenu::highlightedBackgroundColourId, accent);
  setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
}

juce::Font HushLookAndFeel::display(const juce::Component& c, float px) {
  if (auto* lnf = dynamic_cast<HushLookAndFeel*>(&c.getLookAndFeel()))
    return juce::Font(juce::FontOptions(lnf->anton_).withHeight(px));
  return juce::Font(juce::FontOptions(px, juce::Font::bold));
}

void HushLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end,
                                       juce::Slider& slider) {
  const auto box = juce::Rectangle<int>(x, y, w, h).toFloat();
  const float r = juce::jmin(box.getWidth(), box.getHeight()) / 2.0f;
  const auto c = box.getCentre();
  const float angle = start + pos * (end - start);
  const float stroke = juce::jmax(3.0f, r * 0.12f), arcR = r - stroke / 2.0f;
  juce::Path track, value;
  track.addCentredArc(c.x, c.y, arcR, arcR, 0.0f, start, end, true);
  value.addCentredArc(c.x, c.y, arcR, arcR, 0.0f, start, angle, true);
  g.setColour(ink.withAlpha(0.15f));
  g.strokePath(track, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
  g.setColour(accent);
  g.strokePath(value, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
  const float bodyR = r - stroke - juce::jmax(2.0f, r * 0.06f);
  const auto face = juce::Rectangle<float>(2 * bodyR, 2 * bodyR).withCentre(c);
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2a3038), face.getX() + face.getWidth() * 0.4f,
                                         face.getY() + face.getHeight() * 0.3f, juce::Colour(0xff0b0d10), face.getRight(),
                                         face.getBottom(), true));
  g.fillEllipse(face);
  g.setColour(ink.withAlpha(0.15f));
  g.drawEllipse(face.reduced(0.5f), 1.0f);
  const juce::Point<float> dir(std::sin(angle), -std::cos(angle));
  g.setColour(slider.isMouseOverOrDragging() ? accent : ink);
  g.drawLine(juce::Line<float>(c + dir * (bodyR * 0.3f), c + dir * (bodyR * 0.86f)), juce::jmax(2.5f, r * 0.08f));
}

// ---------------------------------------------------------------- view
HushView::HushView(HushProcessor& processor)
    : processor_(processor),
      panel_(processor.params, processor.hush,
             [](const juce::Component& c, float px) { return HushLookAndFeel::display(c, px); }, false) {
  setLookAndFeel(&lnf_);
  addAndMakeVisible(panel_);
  hint_ = kDefaultHint;
  setSize(kWidth, kHeight);
  startTimerHz(30);
}

HushView::~HushView() { setLookAndFeel(nullptr); }

void HushView::timerCallback() {
  const auto follow = [](float& shown, float db) {
    const float next = db > shown ? db : juce::jmax(db, shown - 1.5f);
    const bool moved = std::abs(next - shown) > 0.2f;
    shown = next;
    return moved;
  };
  bool dirty = follow(inDb_, processor_.inputPeakDb.load());
  dirty = follow(outDb_, processor_.outputPeakDb.load()) || dirty;

  juce::String text = kDefaultHint;
  auto* c = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
  if (c != nullptr && (c == this || isParentOf(c))) {
    for (; c != nullptr && c != this; c = c->getParentComponent()) {
      if (c->getHelpText().isNotEmpty()) {
        text = c->getHelpText();
        break;
      }
      if (auto* panel = dynamic_cast<hushui::HushPanel*>(c)) {
        text = panel->hint();
        break;
      }
    }
  }
  if (text != hint_) {
    hint_ = text;
    dirty = true;
  }
  if (dirty) repaint();
}

void HushView::paint(juce::Graphics& g) {
  using L = HushLookAndFeel;
  g.fillAll(L::bg);
  g.setColour(L::ink);
  g.setFont(L::display(*this, 46.0f));
  g.drawText("HUSH", kPad, 10, 160, 52, juce::Justification::centredLeft, false);
  g.setColour(L::ink.withAlpha(0.6f));
  g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)).withExtraKerningFactor(0.14f));
  g.drawText("DI DE-NOISER  /  ZERO LATENCY  /  FROM dJONt", 130, 30, 400, 16, juce::Justification::centredLeft, false);

  // In / out meters.
  const auto meter = [&](int x, const char* label, float db) {
    const auto r = juce::Rectangle<float>(static_cast<float>(x), 22.0f, 120.0f, 8.0f);
    g.setColour(L::ink.withAlpha(0.12f));
    g.fillRoundedRectangle(r, 2.0f);
    g.setColour(db > -1.0f ? juce::Colour(0xffd7261e) : L::accent);
    g.fillRoundedRectangle(r.withWidth(r.getWidth() * juce::jlimit(0.0f, 1.0f, (db + 72.0f) / 72.0f)), 2.0f);
    g.setColour(L::ink.withAlpha(0.6f));
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)).withExtraKerningFactor(0.12f));
    g.drawText(label, juce::Rectangle<float>(r.getX(), r.getBottom() + 4.0f, 120.0f, 12.0f), juce::Justification::centredLeft, false);
  };
  meter(getWidth() - kPad - 260, "IN", inDb_);
  meter(getWidth() - kPad - 120, "OUT", outDb_);

  const auto hint = juce::Rectangle<int>(0, getHeight() - kHintH, getWidth(), kHintH);
  g.setColour(L::plate);
  g.fillRect(hint);
  g.setColour(L::ink.withAlpha(0.65f));
  g.setFont(juce::Font(juce::FontOptions(12.5f)));
  g.drawText(hint_, hint.reduced(kPad, 0), juce::Justification::centredLeft, true);
}

void HushView::resized() {
  panel_.setBounds(kPad, kHeaderH, getWidth() - 2 * kPad, getHeight() - kHeaderH - kHintH - 12);
}

// ---------------------------------------------------------------- window
HushEditor::HushEditor(HushProcessor& processor)
    : juce::AudioProcessorEditor(processor), processor_(processor), view_(processor) {
  addAndMakeVisible(view_);
  constexpr double kAspect = static_cast<double>(HushView::kWidth) / HushView::kHeight;
  setResizable(true, true);
  setResizeLimits(HushView::kWidth * 6 / 10, HushView::kHeight * 6 / 10, HushView::kWidth * 2, HushView::kHeight * 2);
  if (auto* c = getConstrainer()) c->setFixedAspectRatio(kAspect);
  const int width = juce::jlimit(HushView::kWidth * 6 / 10, HushView::kWidth * 2,
                                 static_cast<int>(processor.params.state.getProperty(kUiWidthProp, HushView::kWidth)));
  setSize(width, juce::roundToInt(width / kAspect));
}

void HushEditor::resized() {
  const float scale = static_cast<float>(getWidth()) / static_cast<float>(HushView::kWidth);
  view_.setBounds(0, 0, HushView::kWidth, HushView::kHeight);
  view_.setTransform(juce::AffineTransform::scale(scale));
  processor_.params.state.setProperty(kUiWidthProp, getWidth(), nullptr);
}
