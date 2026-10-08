#include "MyGearModal.h"

#include <vector>

#include "core/Fonts.h"
#include "core/Paint.h"
#include "core/Theme.h"
#include "widgets/Clickable.h"
#include "widgets/DragScroller.h"
#include "widgets/IconButton.h"
#include "widgets/PillButton.h"

namespace t3k::ui {

namespace {
const juce::Colour kRowHover = juce::Colour(235, 235, 245).withAlpha(0.08f);

// Underlined text link (same look as the update notice's).
class LinkButton : public Clickable {
public:
  LinkButton(const juce::String& label, float px) : Clickable(label), font_(Fonts::sans(px)) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setLabel(label);
  }
  void setLabel(const juce::String& label) {
    label_ = label;
    setButtonText(label);
    setSize(static_cast<int>(std::ceil(Fonts::width(font_, label_))) + 1, Fonts::normalLineHeight(font_));
    repaint();
  }
  void paintButton(juce::Graphics& g, bool highlighted, bool) override {
    g.setColour(highlighted ? theme::kWhite : theme::kLinkBlue);
    const float baseline = Fonts::cssBaseline(font_, static_cast<float>(getHeight()));
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(font_, label_, 0, baseline);
    glyphs.draw(g);
  }

private:
  juce::String label_;
  juce::Font font_;
};

// One tone: name, then "Amps / Zuta · 8 NAM", and a pin toggle on the right.
class Row : public Clickable {
public:
  Row(const MyGear::Entry& entry, bool pinned) : Clickable(entry.name), entry_(entry), pin_(Icon::Bookmark, 28) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setTooltip(entry.file.getFullPathName());
    pin_.setName(pinned ? "Unpin" : "Pin");
    setPinned(pinned);
    pin_.onClick = [this] {
      if (onPinToggled) onPinToggled(entry_, !pinned_);
    };
    addAndMakeVisible(pin_);
  }

  std::function<void(const MyGear::Entry&, bool pin)> onPinToggled;
  const MyGear::Entry& entry() const { return entry_; }

  void setPinned(bool pinned) {
    pinned_ = pinned;
    pin_.setActive(pinned);
    pin_.setColour(pinned ? std::optional<juce::Colour>(theme::kBrandYellow) : std::nullopt);
  }

  void resized() override {
    pin_.setTopRightPosition(getWidth() - 8, (getHeight() - pin_.getHeight()) / 2);
  }

  void paintButton(juce::Graphics& g, bool highlighted, bool down) override {
    const auto box = getLocalBounds().toFloat();
    if (highlighted || down) paint::fill(g, box, 8.0f, kRowHover);
    const int textW = getWidth() - 12 - pin_.getWidth() - 16;
    paint::text(g, entry_.name, {12, 5, textW, 20}, Fonts::sans(MyGearModal::kNamePx, true), theme::kWhite);
    juce::String meta = entry_.summary();
    if (entry_.group.isNotEmpty()) meta = entry_.group + juce::String::fromUTF8("  \xc2\xb7  ") + meta;
    paint::text(g, meta, {12, 24, textW, 16}, Fonts::sans(MyGearModal::kMetaPx), theme::kMuted);
  }

private:
  MyGear::Entry entry_;
  IconButton pin_;
  bool pinned_ = false;
};

// Small uppercase section label ("PINNED", "RECENT", "LIBRARY").
class Section : public juce::Component {
public:
  explicit Section(juce::String text) : text_(std::move(text)) { setInterceptsMouseClicks(false, false); }
  void paint(juce::Graphics& g) override {
    paint::text(g, text_.toUpperCase(), getLocalBounds().withTrimmedLeft(12).withTrimmedTop(8),
                Fonts::sans(MyGearModal::kSectionPx, true), theme::kSubtle, juce::Justification::topLeft);
  }

private:
  juce::String text_;
};
}  // namespace

class MyGearModal::Card : public juce::Component {
public:
  Card(MyGearModal& owner, MyGear& gear)
      : owner_(owner),
        gear_(gear),
        close_(Icon::X, theme::kIconBoxSize, 16),
        folderLink_("Change folder", kMetaPx + 1),
        choose_("Choose tones folder", PillButton::Style::filled),
        browse_("Browse TONE3000", PillButton::Style::outline),
        scroller_(DragScroller::Axis::vertical) {
    setOpaque(false);
    close_.setActive(false);
    close_.setName("Close");
    close_.onClick = [this] {
      if (owner_.onClose) owner_.onClose();
    };
    folderLink_.onClick = [this] { pickFolder(); };
    choose_.onClick = [this] { pickFolder(); };
    browse_.onClick = [this] {
      if (owner_.onBrowse) owner_.onBrowse();
    };
    scroller_.setViewedComponent(&column_, false);
    scroller_.setScrollBarsShown(true, false);
    scroller_.setScrollBarThickness(6);
    scroller_.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, theme::kHighlight);
    scroller_.getVerticalScrollBar().setColour(juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(close_);
    addAndMakeVisible(folderLink_);
    addChildComponent(choose_);
    addAndMakeVisible(browse_);
    addAndMakeVisible(scroller_);
    setSize(kCardW, kMaxCardH);
    rebuild();
  }

  void rebuild() {
    rows_.clear();
    sections_.clear();
    column_.removeAllChildren();

    const auto root = gear_.root();
    rootText_ = root != juce::File() ? root.getFullPathName() : juce::String("No tones folder chosen yet");
    folderLink_.setLabel(root != juce::File() ? "Change folder" : "Choose folder");

    int y = 0;
    auto addSection = [&](const juce::String& title, const std::vector<MyGear::Entry>& entries) {
      if (entries.empty()) return;
      auto s = std::make_unique<Section>(title);
      s->setBounds(0, y, 0, kSectionH);
      column_.addAndMakeVisible(*s);
      sections_.push_back(std::move(s));
      y += kSectionH;
      for (const auto& e : entries) {
        auto row = std::make_unique<Row>(e, gear_.isPinned(e.file));
        row->setBounds(0, y, 0, kRowH);
        row->onClick = [this, entry = e] {
          if (owner_.onPick) owner_.onPick(entry);
        };
        row->onPinToggled = [this](const MyGear::Entry& entry, bool pin) {
          gear_.setPinned(entry.file, pin);
          // Rebuilding destroys the row whose button is calling us.
          juce::MessageManager::callAsync([safe = juce::Component::SafePointer(this)] {
            if (safe != nullptr) safe->rebuild();
          });
        };
        column_.addAndMakeVisible(*row);
        rows_.push_back(std::move(row));
        y += kRowH;
      }
    };
    const auto pinned = gear_.pinned();
    const auto recent = gear_.recent();
    const auto library = gear_.library();
    addSection("Pinned", pinned);
    addSection("Recent", recent);
    addSection("Library", library);
    empty_ = rows_.empty();
    choose_.setVisible(empty_ && root == juce::File());
    columnHeight_ = y;
    resized();
    repaint();
  }

  void resized() override {
    auto area = getLocalBounds().reduced(1 + kCardPad);
    close_.setTopRightPosition(getWidth() - 1 - 12, 1 + 12);
    titleBox_ = area.removeFromTop(Fonts::normalLineHeight(kTitlePx));
    area.removeFromTop(6);
    auto pathRow = area.removeFromTop(Fonts::normalLineHeight(kMetaPx + 1));
    folderLink_.setTopRightPosition(pathRow.getRight(), pathRow.getY());
    pathBox_ = pathRow.withTrimmedRight(folderLink_.getWidth() + 12);
    area.removeFromTop(kGap);

    auto footer = area.removeFromBottom(browse_.getHeight());
    browse_.setTopRightPosition(footer.getRight(), footer.getY());
    footerBox_ = footer.withTrimmedRight(browse_.getWidth() + 12);
    area.removeFromBottom(kGap);

    listBox_ = area;
    scroller_.setBounds(area);
    const int w = area.getWidth() - (columnHeight_ > area.getHeight() ? scroller_.getScrollBarThickness() : 0);
    column_.setSize(w, juce::jmax(columnHeight_, 1));
    for (auto& r : rows_) r->setSize(w, kRowH);
    for (auto& s : sections_) s->setSize(w, kSectionH);
    choose_.setTopLeftPosition(area.getCentreX() - choose_.getWidth() / 2, area.getCentreY() + 16);
  }

  void paint(juce::Graphics& g) override {
    const auto box = getLocalBounds().toFloat();
    paint::fill(g, box, kCardRadius, theme::kSurface);
    paint::border(g, box, kCardRadius, theme::kBorder);
    paint::text(g, "My Gear", titleBox_, Fonts::sans(kTitlePx, true), theme::kWhite);
    paint::text(g, rootText_, pathBox_, Fonts::sans(kMetaPx + 1), theme::kMuted);
    paint::text(g, "Click a tone to load it. Folders load every model inside.", footerBox_, Fonts::sans(kMetaPx),
                theme::kSubtle);
    if (empty_) {
      const auto message = gear_.root() == juce::File()
                               ? juce::String("Pick the folder where you keep your .nam and IR files.\n"
                                              "Each folder inside it shows up here as one tone.")
                               : juce::String("No .nam or IR .wav files found in that folder yet.");
      g.setFont(Fonts::sans(kNamePx));
      g.setColour(theme::kMuted);
      g.drawFittedText(message, listBox_.withTrimmedBottom(listBox_.getHeight() / 2 - 8).withTrimmedTop(0),
                       juce::Justification::centredBottom, 3);
    }
  }

private:
  void pickFolder() {
    gear_.chooseRoot([safe = juce::Component::SafePointer(this)] {
      if (safe != nullptr) safe->rebuild();
    });
  }

  MyGearModal& owner_;
  MyGear& gear_;
  IconButton close_;
  LinkButton folderLink_;
  PillButton choose_, browse_;
  DragScroller scroller_;
  juce::Component column_;
  std::vector<std::unique_ptr<Row>> rows_;
  std::vector<std::unique_ptr<Section>> sections_;
  juce::String rootText_;
  juce::Rectangle<int> titleBox_, pathBox_, footerBox_, listBox_;
  int columnHeight_ = 0;
  bool empty_ = true;
};

MyGearModal::MyGearModal(Backdrop backdrop, MyGear& gear)
    : ModalLayer(std::move(backdrop)), card_(std::make_unique<Card>(*this, gear)) {
  setName("my gear");
  setTitle("My Gear");
  setWantsKeyboardFocus(true);
  setContent(*card_);
}

MyGearModal::~MyGearModal() = default;

bool MyGearModal::keyPressed(const juce::KeyPress& key) {
  if (key == juce::KeyPress::escapeKey) {
    if (onClose) onClose();
    return true;
  }
  return false;
}

}  // namespace t3k::ui
