#include "AddTile.h"

#include "GalleryGeometry.h"
#include "core/Help.h"
#include "core/Icons.h"
#include "core/Paint.h"
#include "core/Theme.h"

namespace t3k::ui {

AddTile::Routing AddTile::routingFor(int index, int count) {
  if (count <= 1) return Routing::none;
  if (index == 0) return Routing::right;
  if (index == count - 1) return Routing::left;
  return Routing::both;
}

AddTile::AddTile(Services& services, std::string blockId, int size)
    : GalleryTile(services, std::move(blockId), size) {
  setHelpText(help::text(help::Key::addTile));
  setTitle("Add Tone");
  setRepaintsOnMouseActivity(true);  // a quiet slot lights up under the pointer
}

void AddTile::setRouting(Routing routing) {
  if (routing_ == routing) return;
  routing_ = routing;
  repaint();
}

void AddTile::setCanPaste(bool canPaste) { canPaste_ = canPaste; }

void AddTile::setQuiet(bool quiet) {
  if (quiet_ == quiet) return;
  quiet_ = quiet;
  repaint();
}

void AddTile::open() {
  if (onAdd) onAdd(blockId());
}

std::vector<ContextMenu::Item> AddTile::menuItems() {
  std::vector<ContextMenu::Item> items{
      {"Paste", Icon::ClipboardPaste, help::Key::pasteBlock,
       [this] { if (onPaste) onPaste(blockId()); }, /*disabled=*/!canPaste_},
  };
  for (auto& item : localLoadItems()) items.push_back(std::move(item));
  return items;
}

void AddTile::paint(juce::Graphics& g) {
  const auto box = getLocalBounds().toFloat();
  const bool armed = dropArmed();
  const bool quiet = quiet_ && !armed && !isMouseOverOrDragging();
  // A quiet slot still covers the lane's own slot circle beneath it.
  paint::fill(g, box, gallery::kTileCorner, quiet ? theme::kBackground : theme::kSurfaceRaised);
  paint::dashedBorder(g, box, gallery::kTileCorner,
                      armed ? gallery::kFileDropBorder
                            : quiet ? gallery::kAddTileBorder.withMultipliedAlpha(0.35f) : gallery::kAddTileBorder,
                      quiet ? 1.0f : gallery::kAddTileBorderWidth);

  if (armed) {
    const float s = gallery::kFileDropGlyphSize;
    Icons::draw(g, Icon::Upload, box.withSizeKeepingCentre(s, s), theme::kGray);
    return;
  }

  const int size = tileSize();
  const int icon = gallery::plusIconSize(size);
  // Routing lines are anchored inside the border (absolute children position
  // against the padding box), so the run to the plus ring is a border-width
  // shorter than measured from the tile's outer edge.
  if (!travelling() && routing_ != Routing::none) {
    const float run = size / 2.0f - icon / 2.0f + gallery::plusCircleInset(icon) -
                      gallery::kAddTileBorderWidth;
    const float y = box.getCentreY() - gallery::kLineWidth / 2;
    g.setColour(quiet ? theme::kWire.withMultipliedAlpha(0.35f) : theme::kWire);
    if (routing_ == Routing::left || routing_ == Routing::both)
      g.fillRect(juce::Rectangle<float>(gallery::kAddTileBorderWidth, y, run, gallery::kLineWidth));
    if (routing_ == Routing::right || routing_ == Routing::both)
      g.fillRect(juce::Rectangle<float>(box.getRight() - gallery::kAddTileBorderWidth - run, y, run,
                                        gallery::kLineWidth));
  }
  if (quiet) {
    const float small = icon * 0.6f;
    Icons::draw(g, Icon::PlusCircle, box.withSizeKeepingCentre(small, small), theme::kGray.withMultipliedAlpha(0.7f),
                /*strokeWidth=*/1.0f);
    return;
  }
  Icons::draw(g, Icon::PlusCircle, box.withSizeKeepingCentre(icon, icon), theme::kWire,
              /*strokeWidth=*/1.0f);
}

}  // namespace t3k::ui
