#include "ToneTile.h"

#include "GalleryGeometry.h"
#include "core/Help.h"
#include "core/Icons.h"
#include "core/Paint.h"
#include "core/Theme.h"
#include "core/Fonts.h"
#include "core/ForkColors.h"
#include "services/ToneArt.h"

namespace t3k::ui {

namespace {
constexpr int kImageFadeMs = 200;
constexpr float kDimmedImage = 0.35f;
const juce::Colour kStrip = juce::Colours::black.withAlpha(0.35f);
}  // namespace

void ToneTile::Strip::paint(juce::Graphics& g) {
  g.setColour(kStrip);
  g.fillRect(getLocalBounds().withHeight(kChromeHeight));
}

ToneTile::ToneTile(Services& services, const ChainItem& block, int size)
    : GalleryTile(services, block.blockId, size),
      block_(block),
      image_(services.images),
      power_(Icon::Power, ChromeIconButton::Tone::power, help::Key::blockPower),
      swap_(Icon::ArrowLeftRight, ChromeIconButton::Tone::plain, help::Key::swapTone),
      remove_(Icon::Trash2, ChromeIconButton::Tone::plain, help::Key::removeBlock),
      glow_(services.meters, MeterStore::blockOutId(block.blockId)),
      led_(services.meters, MeterStore::blockOutId(block.blockId)) {
  image_.setCornerRadius(gallery::kTileCorner);
  image_.setGlow(glow_.glow());
  glow_.onChange = [this] { image_.setGlow(glow_.glow()); };
  addAndMakeVisible(image_);
  addChildComponent(dots_);
  addChildComponent(retry_);
  retry_.onRetry = [this] { this->services().modelLoads.retry(blockId()); };

  addAndMakeVisible(chrome_);
  for (auto* b : {&power_, &swap_, &remove_}) chrome_.addAndMakeVisible(*b);
  power_.onClick = [this] { togglePower(); };
  swap_.onClick = [this] { if (onSwap) onSwap(blockId()); };
  remove_.onClick = [this] { this->services().chain.removeBlock(blockId()); };

  addAndMakeVisible(ledSlot_);
  ledSlot_.addChildComponent(led_);
  ledSlot_.setSize(BlockLed::kSize, BlockLed::kSize);
  addMouseListener(&hover_, true);
  services.pointer.addListener(this);
  services.toneArt.addChangeListener(this);

  setBlock(block);
  setHovered(false);
  resized();  // the base set the size before these children existed
}

ToneTile::~ToneTile() {
  if (picker_ != nullptr) picker_->close();
  services().toneArt.removeChangeListener(this);
  services().pointer.removeListener(this);
  removeMouseListener(&hover_);
}

void ToneTile::setBlock(const ChainItem& block) {
  block_ = block;
  enabled_ = block.params.enabled;
  setHelpText(help::toneTile(block.tone.title));
  setTitle(block.tone.title);
  refreshArt();
  syncState();
}

void ToneTile::refreshArt() {
  const auto& tone = block_.tone;
  image_.setTone(ToneArt::urlForTone(tone.local, tone.sourcePath, tone.image), tone.gear, tone.local, kGlyphSize);
}

void ToneTile::changeListenerCallback(juce::ChangeBroadcaster*) { refreshArt(); }

// A model download/prepare is in flight: `modelLoading` covers switches
// (the previous model keeps playing, so `loaded` stays true) and `!loaded`
// covers fresh blocks that have nothing to play yet.
void ToneTile::syncState() {
  const bool busy = block_.modelLoading || (!block_.loaded && !block_.loadFailed);
  const bool armed = dropArmed();
  image_.setVisible(!armed);
  imageFade_.animateTo(enabled_ && !busy && !block_.loadFailed ? 1.0f : kDimmedImage, kImageFadeMs);
  dots_.setVisible(!armed && busy && !block_.loadFailed);
  retry_.setVisible(!armed && block_.loadFailed);
  chrome_.setVisible(!armed);
  ledSlot_.setVisible(!armed);
  power_.setOn(enabled_);
  repaint();
}

void ToneTile::togglePower() {
  enabled_ = !enabled_;
  power_.setOn(enabled_);
  imageFade_.animateTo(enabled_ ? 1.0f : kDimmedImage, kImageFadeMs);
  this->services().chain.setBlockParam(blockId(), "enabled", enabled_);
}

void ToneTile::open() {
  if (onOpen) onOpen(blockId());
}

std::vector<ContextMenu::Item> ToneTile::menuItems() {
  std::vector<ContextMenu::Item> items{
      {"Copy", Icon::Copy, help::Key::copyBlock,
       [this] { this->services().chain.copyBlock(blockId()); }},
  };
  for (auto& item : localLoadItems()) items.push_back(std::move(item));

  // Fork (TK3J): put the block on a footswitch (opens the E-H picker).
  items.push_back({block_.footswitch.isNotEmpty() ? "Footswitch: " + block_.footswitch : juce::String("Footswitch..."),
                   Icon::Power, help::Key::footswitchTile, [this] {
                     // After the menu has gone (this runs inside its row's click).
                     juce::MessageManager::callAsync([safe = juce::Component::SafePointer<ToneTile>(this)] {
                       if (safe != nullptr) safe->openFootswitchPicker();
                     });
                   }});

  // Fork (TK3J): art for tones loaded from disk (ToneArt). Tones loaded
  // before TK3J recorded the source have no path: reload them to enable.
  const auto& tone = block_.tone;
  if (tone.local && tone.sourcePath.isNotEmpty() && juce::File::isAbsolutePath(tone.sourcePath)) {
    const juce::File source(tone.sourcePath);
    if (source.exists()) {
      items.push_back({"Set Art...", Icon::Pencil, help::Key::setArtTile, [this, source] {
                         // The tile may be gone by the time the dialog returns;
                         // the services outlive it.
                         auto& toast = this->services().toast;
                         this->services().toneArt.choose(source, [&toast](bool ok) {
                           if (!ok) toast.show("Couldn't set the art (JPG or PNG only)");
                         });
                       }});
      if (ToneArt::hasOwnArt(source))
        items.push_back({"Clear Art", Icon::X, help::Key::clearArtTile,
                         [this, source] { this->services().toneArt.clear(source); }});
    }
  }
  return items;
}

void ToneTile::openFootswitchPicker() {
  if (picker_ != nullptr) picker_->close();
  picker_ = std::make_unique<FootswitchPicker>(block_.footswitch);
  picker_->onPick = [this](const juce::String& letter) {
    this->services().chain.setBlockFootswitch(blockId(), letter);
  };
  picker_->openAt(*this, lastMenuPoint().translated(6, 6));
}

void ToneTile::setScenePreview(std::optional<bool> turnsOn) {
  if (scenePreview_ == turnsOn) return;
  scenePreview_ = turnsOn;
  repaint();
}

void ToneTile::dropArmedChanged(bool) { syncState(); }

void ToneTile::travellingChanged(bool) { setHovered(hovered_); }

void ToneTile::pointerMoved(const juce::MouseEvent& e, bool leaving) {
  // Leaving onto one of our own children is still hovering the tile.
  setHovered(!leaving || getLocalBounds().contains(e.getEventRelativeTo(this).getPosition()));
}

void ToneTile::setHovered(bool hovered) {
  hovered_ = hovered;
  const bool shown = services().pointer.coarse() || hovered_ || travelling();
  chrome_.setAlpha(shown ? 1.0f : 0.0f);
  chrome_.setInterceptsMouseClicks(false, shown);
}

void ToneTile::resized() {
  const auto box = getLocalBounds();
  image_.setBounds(box);
  dots_.setBounds(box.withSizeKeepingCentre(LoadingDots::kWidth, LoadingDots::kDot + 2 * LoadingDots::kMargin));
  retry_.setBounds(box.withSizeKeepingCentre(retry_.getWidth(), retry_.getHeight()));

  chrome_.setBounds(box.withHeight(kChromeHeight));
  const int y = kChromePad;
  power_.setBounds(kChromePad, y, theme::kIconBoxSize, theme::kIconBoxSize);
  remove_.setBounds(getWidth() - kChromePad - theme::kIconBoxSize, y, theme::kIconBoxSize,
                    theme::kIconBoxSize);
  swap_.setBounds(remove_.getX() - kChromeGap - theme::kIconBoxSize, y, theme::kIconBoxSize,
                  theme::kIconBoxSize);

  ledSlot_.setTopLeftPosition(getWidth() - kLedInset - BlockLed::kSize,
                              getHeight() - kLedInset - BlockLed::kSize);
}

void ToneTile::paint(juce::Graphics& g) {
  paint::fill(g, getLocalBounds().toFloat(), gallery::kTileCorner, theme::kSurface);
  if (dropArmed()) {
    const float s = gallery::kFileDropGlyphSize;
    Icons::draw(g, Icon::Upload, getLocalBounds().toFloat().withSizeKeepingCentre(s, s), theme::kGray);
  }
}

void ToneTile::paintOverChildren(juce::Graphics& g) {
  // Fork (TK3J): footswitch outline + letter badge (bottom-left; the clip LED
  // owns bottom-right), and the scene-bar hover preview.
  const auto box = getLocalBounds().toFloat();
  if (block_.footswitch.isNotEmpty() && !dropArmed()) {
    const auto colour = fork_colors::footswitch(block_.footswitch);
    g.setColour(colour);
    g.drawRoundedRectangle(box.reduced(1.5f), gallery::kTileCorner - 1.0f, 3.0f);
    const float d = juce::jlimit(18.0f, 26.0f, box.getWidth() * 0.13f);
    const juce::Rectangle<float> badge(8.0f, box.getBottom() - 8.0f - d, d, d);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillEllipse(badge.expanded(2.0f));
    g.setColour(colour);
    g.fillEllipse(badge);
    paint::text(g, block_.footswitch, badge.toNearestInt(), Fonts::sans(d * 0.55f, true), juce::Colours::black,
                juce::Justification::centred);
  }
  if (scenePreview_.has_value() && !dropArmed()) {
    g.setColour(theme::kWhite);
    g.drawRoundedRectangle(box.reduced(1.0f), gallery::kTileCorner, 2.0f);
    const auto text = *scenePreview_ ? juce::String("TURNS ON") : juce::String("TURNS OFF");
    const auto font = Fonts::sans(11.0f, true);
    const float w = Fonts::width(font, text) + 16.0f;
    const juce::Rectangle<float> tag(box.getCentreX() - w / 2, box.getBottom() - 30.0f, w, 20.0f);
    g.setColour(theme::kWhite);
    g.fillRoundedRectangle(tag, 10.0f);
    paint::text(g, text, tag.toNearestInt(), font, juce::Colours::black, juce::Justification::centred);
  }
  if (dropArmed())
    paint::dashedBorder(g, getLocalBounds().toFloat(), gallery::kTileCorner,
                        gallery::kFileDropBorder, gallery::kAddTileBorderWidth);
}

}  // namespace t3k::ui
