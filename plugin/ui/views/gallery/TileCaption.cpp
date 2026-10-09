#include "TileCaption.h"

#include "core/Fonts.h"
#include "core/Icons.h"
#include "core/Paint.h"
#include "core/Theme.h"

namespace t3k::ui {

TileCaption::TileCaption(ToneNotes& notes) : notes_(notes) {
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  setRepaintsOnMouseActivity(true);
  for (auto* f : {&gear_, &product_}) {
    f->setFontSize(kTextPx);
    f->setCornerRadius(6.0f);
    f->setPadding(0, 8, 8);
    f->onEscape = [this] { endEdit(false); };
    f->onBlur = [this] { blurred(); };
    addChildComponent(*f);
  }
  gear_.setPlaceholder("Gear (Amp, Pedal, Cab IR...)");
  product_.setPlaceholder("Product");
  gear_.onEnter = [this] { product_.focus(); };
  product_.onEnter = [this] { endEdit(true); };
}

TileCaption::~TileCaption() = default;

void TileCaption::setTone(const ToneSummary& tone) {
  tone_ = tone;
  refresh();
}

void TileCaption::refresh() {
  info_ = notes_.resolved(tone_);
  setTooltip(info_.gear.isNotEmpty() ? info_.gear + " - " + info_.product : info_.product);
  repaint();
}

void TileCaption::resized() {
  gear_.setBounds(0, 0, getWidth(), kFieldH);
  product_.setBounds(0, kFieldH + kFieldGap, getWidth(), kFieldH);
}

void TileCaption::paint(juce::Graphics& g) {
  if (editing_) return;
  const auto keyFont = Fonts::sans(kTextPx);
  const auto valueFont = Fonts::sans(kTextPx, true);
  const bool hover = isMouseOverOrDragging();
  auto row = [&](int y, const juce::String& key, const juce::String& value) {
    const int keyW = juce::roundToInt(Fonts::width(keyFont, key)) + 4;
    paint::text(g, key, {0, y, keyW, kLineH}, keyFont, theme::kSubtle);
    paint::text(g, value.isNotEmpty() ? value : juce::String("-"), {keyW, y, getWidth() - keyW - 16, kLineH},
                valueFont, hover ? theme::kWhite : theme::kMuted);
  };
  row(0, "Gear:", info_.gear);
  row(kLineH, "Product:", info_.product);
  if (hover) {
    const float s = 11.0f;
    Icons::draw(g, Icon::Pencil, juce::Rectangle<float>(getWidth() - s, (kLineH - s) / 2.0f, s, s), theme::kMuted);
  }
}

void TileCaption::mouseUp(const juce::MouseEvent& e) {
  if (!editing_ && e.mouseWasClicked() && !e.mods.isPopupMenu()) beginEdit();
}

void TileCaption::beginEdit() {
  editing_ = true;
  const auto stored = notes_.stored(tone_);
  gear_.setText(stored.gear.isNotEmpty() ? stored.gear : ToneNotes::defaultGear(tone_));
  product_.setText(stored.product.isNotEmpty() ? stored.product : ToneNotes::defaultProduct(tone_));
  gear_.setVisible(true);
  product_.setVisible(true);
  setMouseCursor(juce::MouseCursor::NormalCursor);
  repaint();
  product_.focus();
}

void TileCaption::endEdit(bool save) {
  if (!editing_) return;
  editing_ = false;
  gear_.setVisible(false);
  product_.setVisible(false);
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  if (save) {
    auto stored = notes_.stored(tone_);
    // Typing the default back (or clearing the field) stores nothing, so the
    // label keeps following the tone's own data.
    const auto gear = gear_.text().trim();
    const auto product = product_.text().trim();
    stored.gear = gear == ToneNotes::defaultGear(tone_) ? juce::String() : gear;
    stored.product = product == ToneNotes::defaultProduct(tone_) ? juce::String() : product;
    notes_.store(tone_, stored);
  }
  refresh();
}

// Leaving one field for the other is still editing; leaving both saves.
void TileCaption::blurred() {
  juce::Component::SafePointer<TileCaption> safe(this);
  juce::MessageManager::callAsync([safe] {
    if (safe == nullptr || !safe->editing_) return;
    if (!safe->gear_.hasFocus() && !safe->product_.hasFocus()) safe->endEdit(true);
  });
}

}  // namespace t3k::ui
