#include "WhamPedal.h"

#include "core/Fonts.h"
#include "core/Help.h"
#include "core/Paint.h"
#include "core/Theme.h"

namespace t3k::ui {

namespace {
constexpr float kLabelH = 12.0f;
}  // namespace

WhamPedal::WhamPedal(Services& services) : services_(services), pedal_(services.backend, "whamPedal") {
  pedal_.onChange = [this] { repaint(); };
  setHelpText(help::text(help::Key::whamPedal));
  setTitle("Wham Pedal");
  setViewportIgnoreDragFlag(true);
  services_.meters.addListener(this);
}

WhamPedal::~WhamPedal() { services_.meters.removeListener(this); }

void WhamPedal::setMode(Mode mode) {
  mode_ = mode;
  setMouseCursor(mode == Mode::pedal ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
  repaint();
}

void WhamPedal::whamChanged() {
  if (mode_ == Mode::autoSweep) repaint();
}

float WhamPedal::shown() const {
  switch (mode_) {
    case Mode::pedal: return pedal_.normalised();
    case Mode::autoSweep: return juce::jmax(0.0f, services_.meters.whamPosition());
    case Mode::off: break;
  }
  return 0.0f;
}

juce::Rectangle<float> WhamPedal::track() const {
  auto r = getLocalBounds().toFloat();
  r.removeFromTop(kLabelH + 2);
  r.removeFromBottom(kLabelH + 2);
  return r;
}

void WhamPedal::paint(juce::Graphics& g) {
  const float pos = shown();
  const auto area = getLocalBounds().toFloat();

  g.setFont(Fonts::mono(9.0f, true));
  g.setColour(pos > 0.5f ? theme::kAccent : theme::kGray);
  g.drawText("TOE", area.withHeight(kLabelH), juce::Justification::centred, false);
  g.setColour(pos <= 0.5f ? theme::kWhite.withAlpha(0.8f) : theme::kGray);
  g.drawText("HEEL", area.withTrimmedTop(area.getHeight() - kLabelH), juce::Justification::centred, false);

  const auto t = track();
  // The housing.
  paint::fill(g, t, 8.0f, theme::kSurfaceRaised);
  paint::border(g, t, 8.0f, theme::kBorder);

  // The position fills the narrow rail on the right edge, bottom-up.
  auto rail = t.reduced(5.0f).removeFromRight(4.0f);
  paint::fill(g, rail, 2.0f, theme::kSegmentedTrack);
  const float fillH = rail.getHeight() * pos;
  paint::fill(g, rail.withTrimmedTop(rail.getHeight() - fillH), 2.0f, theme::kAccent);

  // The treadle: a ridged plate, with a bar across it at the pedal's
  // position (bottom = heel, top = toe) and the plate lit below the bar.
  const auto bed = t.reduced(6.0f).withTrimmedRight(8.0f);
  paint::fill(g, bed, 4.0f, theme::kSurface.brighter(0.15f));
  const float barY = bed.getBottom() - bed.getHeight() * pos;
  paint::fill(g, bed.withTrimmedTop(barY - bed.getY()), 4.0f, theme::kAccent.withAlpha(0.18f));
  g.setColour(theme::kBackground.withAlpha(0.6f));
  for (int i = 1; i < 9; ++i) {
    const float y = bed.getY() + bed.getHeight() * static_cast<float>(i) / 9.0f;
    g.drawLine(bed.getX() + 4.0f, y, bed.getRight() - 4.0f, y, 1.0f);
  }
  const auto bar = juce::Rectangle<float>(bed.getX() - 2.0f, barY - 2.0f, bed.getWidth() + 4.0f, 4.0f);
  paint::fill(g, bar, 2.0f, mode_ == Mode::off ? theme::kGray : theme::kAccent);
}

void WhamPedal::mouseDown(const juce::MouseEvent& e) {
  if (mode_ != Mode::pedal) return;
  if (e.mods.isAltDown()) {
    pedal_.set(0.0f);
    return;
  }
  dragStartValue_ = pedal_.normalised();
  pedal_.beginGesture();
}

void WhamPedal::mouseDrag(const juce::MouseEvent& e) {
  if (mode_ != Mode::pedal || !pedal_.dragging()) return;
  const float travel = juce::jmax(20.0f, track().getHeight());
  const float scale = e.mods.isShiftDown() ? 0.25f : 1.0f;
  pedal_.dragTo(juce::jlimit(0.0f, 1.0f, dragStartValue_ - scale * static_cast<float>(e.getDistanceFromDragStartY()) / travel));
  repaint();
}

void WhamPedal::mouseUp(const juce::MouseEvent&) {
  if (pedal_.dragging()) pedal_.endGesture();
}

void WhamPedal::mouseDoubleClick(const juce::MouseEvent&) {
  if (mode_ == Mode::pedal) pedal_.set(0.0f);
}

void WhamPedal::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
  if (mode_ != Mode::pedal) {
    Component::mouseWheelMove(e, wheel);
    return;
  }
  pedal_.set(juce::jlimit(0.0f, 1.0f, pedal_.normalised() + wheel.deltaY * 0.5f));
  repaint();
}

}  // namespace t3k::ui
