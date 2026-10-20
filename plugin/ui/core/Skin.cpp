#include "Skin.h"

#include <cmath>

#include "Fonts.h"
#include "Theme.h"
#include "views/gallery/GalleryGeometry.h"

namespace t3k::ui::skin {

namespace {

Id currentId = Id::stock;

// The art is authored on a 1440 x 900 frame and scales with the editor; its
// focal point sits right of centre, behind the chain's later blocks.
struct Frame {
  juce::Point<float> focus;
  float s;
};
Frame frameFor(juce::Rectangle<float> area) {
  const float s = juce::jmin(area.getWidth() / 1440.0f, area.getHeight() / 900.0f);
  return {{area.getX() + area.getWidth() * 0.77f, area.getY() + area.getHeight() * 0.47f}, s};
}

void radialGlow(juce::Graphics& g, juce::Rectangle<float> area, juce::Point<float> c, float r, juce::Colour colour) {
  juce::ColourGradient glow(colour, c.x, c.y, colour.withAlpha(0.0f), c.x + r, c.y, true);
  g.setGradientFill(glow);
  g.fillRect(area);
}

juce::Path polygon(juce::Point<float> c, float r, int sides) {
  juce::Path p;
  for (int i = 0; i < sides; ++i) {
    const float a = juce::MathConstants<float>::twoPi * static_cast<float>(i) / static_cast<float>(sides) -
                    juce::MathConstants<float>::halfPi;
    const juce::Point<float> v(c.x + r * std::cos(a), c.y + r * std::sin(a));
    if (i == 0)
      p.startNewSubPath(v);
    else
      p.lineTo(v);
  }
  p.closeSubPath();
  return p;
}

void dots(juce::Graphics& g, juce::Point<float> c, float r, int count, float dot, juce::Colour colour) {
  g.setColour(colour);
  for (int i = 0; i < count; ++i) {
    const float a = juce::MathConstants<float>::twoPi * static_cast<float>(i) / static_cast<float>(count) -
                    juce::MathConstants<float>::halfPi;
    g.fillEllipse(juce::Rectangle<float>(2 * dot, 2 * dot).withCentre({c.x + r * std::cos(a), c.y + r * std::sin(a)}));
  }
}

// Zenith: a nebula glow, the flower-of-life lattice in fine gold, two
// enclosing rings (one dotted), a tilted orbit and a fixed starfield.
void paintZenith(juce::Graphics& g, juce::Rectangle<float> area) {
  const auto [c, s] = frameFor(area);
  radialGlow(g, area, c, 640.0f * s, juce::Colour(0xff6f5bd6).withAlpha(0.20f));

  const auto gold = juce::Colour(0xffd9b45a);
  g.setColour(gold.withAlpha(0.26f));
  const float r = 110.0f * s;
  auto ring = [&](juce::Point<float> p, float radius) {
    g.drawEllipse(juce::Rectangle<float>(2 * radius, 2 * radius).withCentre(p), 1.0f);
  };
  ring(c, r);
  for (int k = 0; k < 6; ++k) {
    const float a = juce::degreesToRadians(60.0f * static_cast<float>(k));
    const float b = juce::degreesToRadians(30.0f + 60.0f * static_cast<float>(k));
    ring({c.x + r * std::cos(a), c.y + r * std::sin(a)}, r);
    ring({c.x + 2 * r * std::cos(a), c.y + 2 * r * std::sin(a)}, r);
    ring({c.x + r * std::sqrt(3.0f) * std::cos(b), c.y + r * std::sqrt(3.0f) * std::sin(b)}, r);
  }
  ring(c, 3 * r);

  juce::Path outer;
  outer.addEllipse(juce::Rectangle<float>(2 * 3.15f * r, 2 * 3.15f * r).withCentre(c));
  juce::Path dotted;
  const float dash[] = {2.0f, 10.0f};
  juce::PathStrokeType(1.0f).createDashedStroke(dotted, outer, dash, 2);
  g.fillPath(dotted);

  juce::Path orbit;
  orbit.addEllipse(juce::Rectangle<float>(2 * 560.0f * s, 2 * 150.0f * s).withCentre(c));
  g.setColour(gold.withAlpha(0.16f));
  g.strokePath(orbit, juce::PathStrokeType(1.0f),
               juce::AffineTransform::rotation(juce::degreesToRadians(-18.0f), c.x, c.y));

  juce::Random random(7);
  g.setColour(juce::Colour(0xfff1eee6).withAlpha(0.65f));
  for (int i = 0; i < 46; ++i) {
    const float x = area.getX() + random.nextFloat() * area.getWidth();
    const float y = area.getY() + (0.1f + random.nextFloat() * 0.75f) * area.getHeight();
    const float d = (0.8f + random.nextFloat() * 0.8f) * juce::jmax(1.0f, s);
    g.fillEllipse(juce::Rectangle<float>(2 * d, 2 * d).withCentre({x, y}));
  }
}

// Tidal: a 7-against-4 polyrhythm (heptagon and square on concentric
// rings, the downbeat shared at the top) over layered waveforms.
void paintTidal(juce::Graphics& g, juce::Rectangle<float> area) {
  const auto [c, s] = frameFor(area);
  const auto teal = juce::Colour(0xff4ff0d2), violet = juce::Colour(0xff9b7bff);
  radialGlow(g, area, c, 440.0f * s, teal.withAlpha(0.13f));

  const juce::PathStrokeType stroke(1.2f);
  g.setColour(teal.withAlpha(0.22f));
  g.drawEllipse(juce::Rectangle<float>(480.0f * s, 480.0f * s).withCentre(c), 1.2f);
  g.setColour(violet.withAlpha(0.30f));
  g.drawEllipse(juce::Rectangle<float>(300.0f * s, 300.0f * s).withCentre(c), 1.2f);
  g.setColour(teal.withAlpha(0.45f));
  g.strokePath(polygon(c, 240.0f * s, 7), stroke);
  g.setColour(violet.withAlpha(0.55f));
  g.strokePath(polygon(c, 150.0f * s, 4), stroke);
  dots(g, c, 240.0f * s, 7, 6.0f * s, teal.withAlpha(0.9f));
  dots(g, c, 150.0f * s, 4, 7.0f * s, violet.withAlpha(0.9f));

  struct Wave {
    float dy, amp, length, phase;
    juce::Colour colour;
  };
  const Wave waves[] = {{0, 18, 420, 0.0f, teal.withAlpha(0.22f)},
                        {44, 26, 360, 1.0f, violet.withAlpha(0.22f)},
                        {80, 14, 300, 2.0f, teal.withAlpha(0.14f)},
                        {98, 30, 520, 0.5f, violet.withAlpha(0.14f)}};
  const float base = area.getY() + area.getHeight() * 0.52f;
  for (const auto& w : waves) {
    juce::Path p;
    const float step = 12.0f;
    for (float x = 0; x <= area.getWidth() + step; x += step) {
      const float y = base + w.dy * s +
                      w.amp * s * std::sin(juce::MathConstants<float>::twoPi * x / (w.length * s) + w.phase);
      if (x == 0)
        p.startNewSubPath(area.getX(), y);
      else
        p.lineTo(area.getX() + x, y);
    }
    g.setColour(w.colour);
    g.strokePath(p, stroke);
  }
}

}  // namespace

Id fromString(const juce::String& name) {
  if (name == "zenith") return Id::zenith;
  if (name == "tidal") return Id::tidal;
  return Id::stock;
}

juce::String toString(Id id) {
  switch (id) {
    case Id::zenith: return "zenith";
    case Id::tidal: return "tidal";
    case Id::stock: break;
  }
  return "stock";
}

juce::String displayName(Id id) {
  switch (id) {
    case Id::zenith: return "Zenith";
    case Id::tidal: return "Tidal";
    case Id::stock: break;
  }
  return "Stock";
}

Id current() { return currentId; }

void apply(Id id) {
  currentId = id;
  using namespace theme;
  switch (id) {
    case Id::stock:
      kBackground = juce::Colour(0xff000000);
      kAccent = juce::Colour(0xffffff00);
      kWire = juce::Colour(0xffffffff);
      kMuted = juce::Colour(235, 235, 245).withAlpha(0.60f);
      kSubtle = juce::Colour(235, 235, 245).withAlpha(0.40f);
      kHighlight = juce::Colour(235, 235, 245).withAlpha(0.18f);
      kGray = juce::Colour(0xff8d8d93);
      kBorder = juce::Colour(84, 84, 88).withAlpha(0.65f);
      kSurface = juce::Colour(0xff151517);
      kSurfaceRaised = juce::Colour(0xff1c1c1e);
      kSegmentedTrack = juce::Colour(120, 120, 128).withAlpha(0.36f);
      kPanelBg = juce::Colour(0xff141416);
      gallery::kTileCorner = 16.0f;
      break;
    case Id::zenith:
      kBackground = juce::Colour(0xff07060f);
      kAccent = juce::Colour(0xffd9b45a);
      kWire = juce::Colour(0xff8a80c8);
      kMuted = juce::Colour(226, 220, 244).withAlpha(0.66f);
      kSubtle = juce::Colour(226, 220, 244).withAlpha(0.44f);
      kHighlight = juce::Colour(217, 180, 90).withAlpha(0.20f);
      kGray = juce::Colour(0xff8f8aac);
      kBorder = juce::Colour(75, 66, 128).withAlpha(0.85f);
      kSurface = juce::Colour(0xff0f0d20);
      kSurfaceRaised = juce::Colour(0xff0c0a19);
      kSegmentedTrack = juce::Colour(90, 81, 150).withAlpha(0.40f);
      kPanelBg = juce::Colour(0xff100e22);
      gallery::kTileCorner = 3.0f;
      break;
    case Id::tidal:
      kBackground = juce::Colour(0xff04090c);
      kAccent = juce::Colour(0xff4ff0d2);
      kWire = juce::Colour(0xff3fb3a6);
      kMuted = juce::Colour(214, 245, 239).withAlpha(0.64f);
      kSubtle = juce::Colour(214, 245, 239).withAlpha(0.42f);
      kHighlight = juce::Colour(79, 240, 210).withAlpha(0.17f);
      kGray = juce::Colour(0xff84a8a2);
      kBorder = juce::Colour(37, 74, 86).withAlpha(0.90f);
      kSurface = juce::Colour(0xff0c1d24);
      kSurfaceRaised = juce::Colour(0xff09171c);
      kSegmentedTrack = juce::Colour(47, 100, 112).withAlpha(0.42f);
      kPanelBg = juce::Colour(0xff0a1a20);
      gallery::kTileCorner = 26.0f;
      break;
  }
}

void paintBackdrop(juce::Graphics& g, juce::Rectangle<float> area) {
  if (currentId == Id::zenith) paintZenith(g, area);
  if (currentId == Id::tidal) paintTidal(g, area);
}

Logo logo() {
  switch (currentId) {
    case Id::zenith: return {juce::Colour(0xffd9b45a), juce::Colour(0xff6f5bd6), juce::Colour(0xff24194f), false};
    case Id::tidal: return {juce::Colour(0xff4ff0d2), juce::Colour(0xff9b7bff), juce::Colour(0xff2a1a6b), false};
    case Id::stock: break;
  }
  return {juce::Colour(0xffffff00), juce::Colour(0xffff0000), juce::Colour(0xff0000ff), true};
}

juce::Font logoNumberFont(float px) {
  // Michroma is very wide and Unbounded heavy: sized down to sit in the
  // stock logo's height.
  if (currentId == Id::zenith) return Fonts::michroma(px * 0.72f);
  if (currentId == Id::tidal) return Fonts::unbounded(px * 0.82f);
  return Fonts::tracked(Fonts::sans(px, true, true), -0.02f);
}

juce::Font logoGridFont(float px) {
  // Both faces run wide: shrunk so the four columns stay apart.
  if (currentId == Id::zenith) return Fonts::michroma(px * 0.5f);
  if (currentId == Id::tidal) return Fonts::unbounded(px * 0.62f);
  return Fonts::mono(px, true);
}

bool tileCornerTicks() { return currentId == Id::zenith; }

bool hasBackdrop() { return currentId != Id::stock; }

}  // namespace t3k::ui::skin
