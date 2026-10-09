#include "ForkLogo.h"

#include "Fonts.h"

namespace t3k::ui::fork_logo {

namespace {
const juce::Colour kYellow{0xffffff00};
const juce::Colour kRed{0xffff0000};
const juce::Colour kBlue{0xff0000ff};

// Grid cell pitch and the "3000" size, as fractions of the logo height.
constexpr float kCell = 0.25f;        // four rows
constexpr float kGridGlyph = 0.29f;  // letter size in a cell
constexpr float kNumber = 0.66f;      // "3000" cap-ish size
constexpr float kGap = 0.16f;         // grid -> number

// Each glyph run printed three times, back to front: blue, red, yellow.
void stacked(juce::Graphics& g, const juce::Font& font, const juce::String& text, juce::Rectangle<float> box,
             juce::Justification just, float offset) {
  const std::pair<juce::Colour, float> layers[] = {{kBlue, 2.0f}, {kRed, 1.0f}, {kYellow, 0.0f}};
  g.setFont(font);
  for (const auto& [colour, k] : layers) {
    g.setColour(colour);
    g.drawText(text, box.translated(offset * k * 0.7f, offset * k), just, false);
  }
}

juce::Font numberFont(float height) {
  return Fonts::tracked(Fonts::sans(height * kNumber, true, true), -0.02f);
}
}  // namespace

float widthFor(float height) {
  const float grid = 4 * height * kCell;
  juce::GlyphArrangement glyphs;
  glyphs.addLineOfText(numberFont(height), "3000", 0, 0);
  return grid + height * kGap + glyphs.getBoundingBox(0, -1, true).getWidth() + 4.0f;
}

void draw(juce::Graphics& g, juce::Rectangle<float> box) {
  const float h = box.getHeight();
  const float cell = h * kCell;
  const auto gridFont = Fonts::mono(h * kGridGlyph, true);
  const float gridOffset = juce::jmax(0.6f, h / 48.0f);

  // T at the shared corner; across, down and diagonal from it.
  const char* across = "TOAN";
  auto letter = [&](int row, int col, char c) {
    const juce::Rectangle<float> cellBox(box.getX() + col * cell, box.getY() + row * cell, cell, cell);
    stacked(g, gridFont, juce::String::charToString(c), cellBox, juce::Justification::centred, gridOffset);
  };
  for (int i = 0; i < 4; ++i) {
    letter(0, i, across[i]);
    if (i > 0) {
      letter(i, 0, across[i]);
      letter(i, i, across[i]);
    }
  }

  const float x = box.getX() + 4 * cell + h * kGap;
  stacked(g, numberFont(h), "3000", {x, box.getY(), box.getRight() - x, h}, juce::Justification::centredLeft,
          juce::jmax(1.0f, h / 24.0f));
}

}  // namespace t3k::ui::fork_logo
