#include "Skin.h"

#include <cmath>

#include "Fonts.h"
#include "UiBinaryData.h"
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
  for (const auto id : kAll)
    if (name == toString(id)) return id;
  return Id::stock;
}

juce::String toString(Id id) {
  switch (id) {
    case Id::zenith: return "zenith";
    case Id::tidal: return "tidal";
    case Id::inferno: return "inferno";
    case Id::ironwave: return "ironwave";
    case Id::hologram: return "hologram";
    case Id::gilded: return "gilded";
    case Id::stock: break;
  }
  return "stock";
}

juce::String displayName(Id id) {
  switch (id) {
    case Id::zenith: return "Zenith";
    case Id::tidal: return "Tidal";
    case Id::inferno: return "Inferno";
    case Id::ironwave: return "Ironwave";
    case Id::hologram: return "Hologram";
    case Id::gilded: return "Gilded";
    case Id::stock: break;
  }
  return "Stock";
}

Id current() { return currentId; }

namespace {
struct Tokens {
  juce::uint32 background, accent, wire;
  juce::Colour ink;  // the muted / subtle text family's base
  juce::Colour highlight;
  juce::uint32 gray;
  juce::Colour border;
  juce::uint32 surface, surfaceRaised;
  juce::Colour segmentedTrack;
  juce::uint32 panel;
  float tileCorner;
  float mutedAlpha = 0.64f, subtleAlpha = 0.42f;
};

Tokens tokensFor(Id id) {
  switch (id) {
    case Id::zenith:
      return {0xff07060f, 0xffd9b45a, 0xff8a80c8, juce::Colour(226, 220, 244), juce::Colour(217, 180, 90).withAlpha(0.20f),
              0xff8f8aac, juce::Colour(75, 66, 128).withAlpha(0.85f), 0xff0f0d20, 0xff0c0a19,
              juce::Colour(90, 81, 150).withAlpha(0.40f), 0xff100e22, 3.0f, 0.66f, 0.44f};
    case Id::tidal:
      return {0xff04090c, 0xff4ff0d2, 0xff3fb3a6, juce::Colour(214, 245, 239), juce::Colour(79, 240, 210).withAlpha(0.17f),
              0xff84a8a2, juce::Colour(37, 74, 86).withAlpha(0.90f), 0xff0c1d24, 0xff09171c,
              juce::Colour(47, 100, 112).withAlpha(0.42f), 0xff0a1a20, 26.0f};
    case Id::inferno:
      return {0xff0a0203, 0xffff3b2f, 0xffc2423a, juce::Colour(251, 230, 225), juce::Colour(255, 59, 47).withAlpha(0.18f),
              0xffa98a86, juce::Colour(90, 26, 28).withAlpha(0.90f), 0xff1c0708, 0xff140506,
              juce::Colour(120, 40, 40).withAlpha(0.40f), 0xff180607, 8.0f, 0.68f, 0.46f};
    case Id::ironwave:
      return {0xff05080d, 0xff9fd3ff, 0xff6f90ad, juce::Colour(226, 238, 248), juce::Colour(159, 211, 255).withAlpha(0.16f),
              0xff8aa0b3, juce::Colour(43, 62, 82).withAlpha(0.95f), 0xff111b26, 0xff0b121a,
              juce::Colour(61, 82, 104).withAlpha(0.45f), 0xff0d1620, 4.0f, 0.68f, 0.46f};
    case Id::hologram:
      return {0xff06060a, 0xffff9a4a, 0xff5fd3e8, juce::Colour(240, 238, 255), juce::Colour(255, 154, 74).withAlpha(0.17f),
              0xff9a97ad, juce::Colour(70, 70, 88).withAlpha(0.90f), 0xff15151f, 0xff0e0e15,
              juce::Colour(74, 74, 88).withAlpha(0.45f), 0xff121219, 22.0f, 0.66f, 0.44f};
    case Id::gilded:
      return {0xff000000, 0xffe2b44f, 0xffa8893f, juce::Colour(246, 239, 224), juce::Colour(226, 180, 79).withAlpha(0.18f),
              0xffa39579, juce::Colour(92, 74, 34).withAlpha(0.90f), 0xff15110a, 0xff0d0a06,
              juce::Colour(92, 74, 34).withAlpha(0.45f), 0xff110e08, 2.0f, 0.68f, 0.46f};
    case Id::stock: break;
  }
  return {0xff000000, 0xffffff00, 0xffffffff, juce::Colour(235, 235, 245), juce::Colour(235, 235, 245).withAlpha(0.18f),
          0xff8d8d93, juce::Colour(84, 84, 88).withAlpha(0.65f), 0xff151517, 0xff1c1c1e,
          juce::Colour(120, 120, 128).withAlpha(0.36f), 0xff141416, 16.0f, 0.60f, 0.40f};
}

bool isPhoto(Id id) {
  return id == Id::inferno || id == Id::ironwave || id == Id::hologram || id == Id::gilded;
}

juce::Image loadPhoto(Id id) {
  switch (id) {
    case Id::inferno: return juce::ImageFileFormat::loadFrom(UiBinaryData::skininferno_jpg, UiBinaryData::skininferno_jpgSize);
    case Id::ironwave: return juce::ImageFileFormat::loadFrom(UiBinaryData::skinironwave_jpg, UiBinaryData::skinironwave_jpgSize);
    case Id::hologram: return juce::ImageFileFormat::loadFrom(UiBinaryData::skinhologram_jpg, UiBinaryData::skinhologram_jpgSize);
    case Id::gilded: return juce::ImageFileFormat::loadFrom(UiBinaryData::skingilded_jpg, UiBinaryData::skingilded_jpgSize);
    case Id::stock: case Id::zenith: case Id::tidal: break;
  }
  return {};
}

// The photo, rendered once at the editor's size (and the display's pixel
// scale): covering the area, or for Gilded standing at the right at full
// height and fading out to the left into the background.
void paintPhoto(juce::Graphics& g, juce::Rectangle<float> area) {
  PhotoHold cache;
  if (cache->id != currentId || !cache->source.isValid()) {
    cache->id = currentId;
    cache->source = loadPhoto(currentId);
    cache->scaled = {};
  }
  if (!cache->source.isValid() || area.isEmpty()) return;
  const float pixelScale = juce::jmax(1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
  const auto bounds = area.toNearestInt();
  if (!cache->scaled.isValid() || cache->scaledFor != bounds || cache->scaledAt != pixelScale) {
    const int w = juce::roundToInt(bounds.getWidth() * pixelScale), h = juce::roundToInt(bounds.getHeight() * pixelScale);
    juce::Image out(juce::Image::RGB, juce::jmax(1, w), juce::jmax(1, h), true);
    juce::Graphics sg(out);
    sg.fillAll(theme::kBackground);
    const auto& src = cache->source;
    const float sw = static_cast<float>(src.getWidth()), sh = static_cast<float>(src.getHeight());
    if (currentId == Id::gilded) {
      const float scale = static_cast<float>(h) / sh;
      const float dw = sw * scale, x = static_cast<float>(w) - dw * 0.92f;
      sg.drawImageTransformed(src, juce::AffineTransform::scale(scale).translated(x, 0.0f), false);
      juce::ColourGradient fade(theme::kBackground, x, 0.0f, theme::kBackground.withAlpha(0.0f), x + dw * 0.45f, 0.0f, false);
      sg.setGradientFill(fade);
      sg.fillRect(0.0f, 0.0f, x + dw * 0.45f, static_cast<float>(h));
      sg.setColour(theme::kBackground);
      sg.fillRect(0.0f, 0.0f, x, static_cast<float>(h));
    } else {
      const float scale = juce::jmax(static_cast<float>(w) / sw, static_cast<float>(h) / sh);
      const float x = (static_cast<float>(w) - sw * scale) / 2.0f, y = (static_cast<float>(h) - sh * scale) / 2.0f;
      sg.drawImageTransformed(src, juce::AffineTransform::scale(scale).translated(x, y), false);
      // A scrim so the tile labels and meters stay readable over busy art.
      sg.setColour(theme::kBackground.withAlpha(0.28f));
      sg.fillAll();
    }
    cache->scaled = out;
    cache->scaledFor = bounds;
    cache->scaledAt = pixelScale;
  }
  g.drawImage(cache->scaled, area, juce::RectanglePlacement::stretchToFit);
}
}  // namespace

void apply(Id id) {
  currentId = id;
  using namespace theme;
  const auto t = tokensFor(id);
  kBackground = juce::Colour(t.background);
  kAccent = juce::Colour(t.accent);
  kWire = juce::Colour(t.wire);
  kMuted = t.ink.withAlpha(t.mutedAlpha);
  kSubtle = t.ink.withAlpha(t.subtleAlpha);
  kHighlight = t.highlight;
  kGray = juce::Colour(t.gray);
  kBorder = t.border;
  kSurface = juce::Colour(t.surface);
  kSurfaceRaised = juce::Colour(t.surfaceRaised);
  kSegmentedTrack = t.segmentedTrack;
  kPanelBg = juce::Colour(t.panel);
  gallery::kTileCorner = t.tileCorner;
}

void paintBackdrop(juce::Graphics& g, juce::Rectangle<float> area) {
  if (currentId == Id::zenith) paintZenith(g, area);
  if (currentId == Id::tidal) paintTidal(g, area);
  if (isPhoto(currentId)) paintPhoto(g, area);
}

Logo logo() {
  switch (currentId) {
    case Id::zenith: return {juce::Colour(0xffd9b45a), juce::Colour(0xff6f5bd6), juce::Colour(0xff24194f), false};
    case Id::tidal: return {juce::Colour(0xff4ff0d2), juce::Colour(0xff9b7bff), juce::Colour(0xff2a1a6b), false};
    case Id::inferno: return {juce::Colour(0xfffbeee9), juce::Colour(0xffff3b2f), juce::Colour(0xff4a0b0b), false};
    case Id::ironwave: return {juce::Colour(0xffeef5fb), juce::Colour(0xff9fd3ff), juce::Colour(0xff24364a), false};
    case Id::hologram: return {juce::Colour(0xfff5f3ff), juce::Colour(0xffff9a4a), juce::Colour(0xff1aa6c0), false};
    case Id::gilded: return {juce::Colour(0xffe2b44f), juce::Colour(0xff7a5a1c), juce::Colour(0xff2a1e08), false};
    case Id::stock: break;
  }
  return {juce::Colour(0xffffff00), juce::Colour(0xffff0000), juce::Colour(0xff0000ff), true};
}

juce::Font logoNumberFont(float px) {
  // Michroma is very wide, Unbounded heavy, Anton tall: each sized to sit in
  // the stock logo's height.
  switch (currentId) {
    case Id::zenith: case Id::ironwave: return Fonts::michroma(px * 0.72f);
    case Id::tidal: case Id::hologram: return Fonts::unbounded(px * 0.82f);
    case Id::inferno: return Fonts::anton(px * 1.12f);
    case Id::gilded: return Fonts::cinzel(px * 0.9f);
    case Id::stock: break;
  }
  return Fonts::tracked(Fonts::sans(px, true, true), -0.02f);
}

juce::Font logoGridFont(float px) {
  // The display faces run wide: shrunk so the four columns stay apart.
  switch (currentId) {
    case Id::zenith: case Id::ironwave: return Fonts::michroma(px * 0.5f);
    case Id::tidal: case Id::hologram: return Fonts::unbounded(px * 0.62f);
    case Id::inferno: return Fonts::anton(px * 0.95f);
    case Id::gilded: return Fonts::cinzel(px * 0.72f);
    case Id::stock: break;
  }
  return Fonts::mono(px, true);
}

bool tileCornerTicks() { return currentId == Id::zenith; }

bool hasBackdrop() { return currentId != Id::stock; }

}  // namespace t3k::ui::skin
