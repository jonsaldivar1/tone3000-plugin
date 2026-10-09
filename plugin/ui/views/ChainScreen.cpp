#include "ChainScreen.h"

#include <algorithm>

namespace t3k::ui {

ChainScreen::ChainScreen(Services& services) : services_(services), gallery_(services), scenes_(services) {
  // Fork (TK3J): the scene bar under the chain; hovering a scene previews it
  // on the tiles.
  scenes_.onPreview = [this](int scene) { gallery_.setScenePreview(scene); };
  addChildComponent(scenes_);
  updateSceneBar();
  services_.prefs.addListener(this);
  gallery_.onOpenBlock = [this](const std::string& id) { openDetail(id); };
  gallery_.onSelectTone = [this](ChainSide side, const std::string& id) {
    if (onSelectTone) onSelectTone(side, id);
  };
  addAndMakeVisible(gallery_);

  // Reopen the card a swap / OAuth round trip left behind, unless its block
  // is gone (the chain changed underneath the round trip).
  const auto saved = services_.prefs.session.find(UiPrefs::kDetailBlockId);
  if (saved != services_.prefs.session.end() && saved->second.isNotEmpty()) {
    const auto id = saved->second.toStdString();
    if (services_.chain.state().findBlock(id) != nullptr)
      openDetail(id);
    else
      services_.prefs.session.erase(UiPrefs::kDetailBlockId);
  }
}

ChainScreen::~ChainScreen() { services_.prefs.removeListener(this); }

void ChainScreen::prefChanged(const juce::String& key) {
  if (key == UiPrefs::kShowSceneBar) updateSceneBar();
}

void ChainScreen::updateSceneBar() {
  scenes_.setVisible(detail_ == nullptr && services_.prefs.getBool(UiPrefs::kShowSceneBar, true));
  resized();
}

void ChainScreen::openDetail(const std::string& blockId) {
  services_.prefs.session[UiPrefs::kDetailBlockId] = juce::String(blockId);
  detail_ = std::make_unique<BlockDetail>(services_, blockId);
  // The card may ask to close from inside a chain listener callback (its
  // block vanished); tear it down once that callback has unwound.
  detail_->onBack = [this] {
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<ChainScreen>(this)] {
      if (safe != nullptr) safe->closeDetail();
    });
  };
  detail_->onSwap = [this](const std::string& id) {
    // The swap keeps the block id; the detail id stays saved for the return.
    const auto& state = services_.chain.state();
    const bool right = state.chainRight && std::any_of(state.chainRight->begin(), state.chainRight->end(),
                                                       [&](const ChainItem& b) { return b.blockId == id; });
    if (onSelectTone) onSelectTone(right ? ChainSide::right : ChainSide::left, id);
  };
  addAndMakeVisible(*detail_);
  gallery_.setVisible(false);
  updateSceneBar();
}

void ChainScreen::closeDetail() {
  services_.prefs.session.erase(UiPrefs::kDetailBlockId);
  if (!detail_) return;
  detail_.reset();
  gallery_.setVisible(true);
  updateSceneBar();
}

void ChainScreen::returnToGallery() {
  closeDetail();
  gallery_.returnToGallery();
}

void ChainScreen::resized() {
  auto area = getLocalBounds();
  if (scenes_.isVisible()) scenes_.setBounds(area.removeFromBottom(SceneBar::kHeight));
  gallery_.setBounds(area);
  if (detail_) detail_->setBounds(area);
}

}  // namespace t3k::ui
