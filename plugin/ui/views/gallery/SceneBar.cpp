#include "SceneBar.h"

#include "core/Fonts.h"
#include "core/ForkColors.h"
#include "core/Paint.h"
#include "core/Theme.h"
#include "widgets/Clickable.h"

namespace t3k::ui {

namespace {
const juce::Colour kChipBg{0xff151517};
const juce::Colour kChipHover{0xff2c2c2e};
const juce::Colour kChipBorder{0xff3a3a3d};
}  // namespace

juce::String SceneBar::sceneName(const ChainState& state, int index) {
  const auto name = index < static_cast<int>(state.sceneNames.size())
                        ? state.sceneNames[static_cast<size_t>(index)]
                        : juce::String();
  return name.isNotEmpty() ? name : "Scene " + fork_colors::sceneLetter(index);
}

// A scene: letter dot + name pill. Active = filled with the scene color.
class SceneBar::SceneChip : public Clickable {
public:
  SceneChip(SceneBar& bar, int index) : Clickable(fork_colors::sceneLetter(index)), bar_(bar), index_(index) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setHelpText("Scene " + fork_colors::sceneLetter(index) +
                ": switch to this scene. Double-click: rename. Tweaks you make in a scene stay in it.");
  }

  void set(const juce::String& name, bool active) {
    name_ = name;
    active_ = active;
    setTitle("Scene " + fork_colors::sceneLetter(index_) + ": " + name);
    const int w = 6 + kDot + 10 + juce::roundToInt(Fonts::width(Fonts::sans(kTextPx, true), name_)) + 16;
    setSize(w, kChipH);
    repaint();
  }

  void paintButton(juce::Graphics& g, bool highlighted, bool) override {
    const auto box = getLocalBounds().toFloat().reduced(0.5f);
    const auto colour = fork_colors::scene(index_);
    const float r = box.getHeight() / 2;
    paint::fill(g, box, r, active_ ? colour : (highlighted ? kChipHover : kChipBg));
    paint::border(g, box, r, active_ ? colour : kChipBorder);
    const auto dot = juce::Rectangle<float>(6, (getHeight() - kDot) / 2.0f, kDot, kDot);
    g.setColour(active_ ? juce::Colours::black.withAlpha(0.18f) : colour);
    g.fillEllipse(dot);
    paint::text(g, fork_colors::sceneLetter(index_), dot.toNearestInt(), Fonts::sans(12.0f, true),
                juce::Colours::black, juce::Justification::centred);
    paint::text(g, name_, {6 + kDot + 10, 0, getWidth() - (6 + kDot + 10), getHeight()},
                Fonts::sans(kTextPx, active_), active_ ? juce::Colours::black : theme::kWhite,
                juce::Justification::centredLeft);
  }

  void clicked() override { bar_.services_.chain.selectScene(index_); }
  void mouseDoubleClick(const juce::MouseEvent&) override { bar_.beginRename(index_); }
  void mouseEnter(const juce::MouseEvent& e) override {
    Clickable::mouseEnter(e);
    if (bar_.onPreview) bar_.onPreview(active_ ? -1 : index_);
  }
  void mouseExit(const juce::MouseEvent& e) override {
    Clickable::mouseExit(e);
    if (bar_.onPreview) bar_.onPreview(-1);
  }

private:
  static constexpr int kDot = 26;
  SceneBar& bar_;
  int index_;
  juce::String name_;
  bool active_ = false;
};

// A footswitch: colored letter square + the blocks on it; lit while they're on.
class SceneBar::SwitchChip : public Clickable {
public:
  SwitchChip(SceneBar& bar, juce::String letter, juce::String label, bool lit)
      : Clickable(letter), bar_(bar), letter_(std::move(letter)), label_(std::move(label)), lit_(lit) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setTitle("Footswitch " + letter_ + ": " + label_);
    setHelpText("Footswitch " + letter_ + ": turn " + label_ + " " + (lit_ ? "off" : "on") +
                ". Right-click a block: Footswitch to change what's on it.");
    const int textW = juce::jmin(220, juce::roundToInt(Fonts::width(Fonts::sans(kTextPx), label_)) + 1);
    setSize(6 + kSquare + 10 + textW + 14, kChipH);
  }

  void paintButton(juce::Graphics& g, bool highlighted, bool) override {
    const auto colour = fork_colors::footswitch(letter_);
    const auto box = getLocalBounds().toFloat().reduced(0.5f);
    paint::fill(g, box, 10.0f, highlighted ? kChipHover : kChipBg);
    paint::border(g, box, 10.0f, lit_ ? colour : kChipBorder);
    const auto sq = juce::Rectangle<float>(6, (getHeight() - kSquare) / 2.0f, kSquare, kSquare);
    g.setColour(lit_ ? colour : kChipHover);
    g.fillRoundedRectangle(sq, 7.0f);
    paint::text(g, letter_, sq.toNearestInt(), Fonts::sans(12.0f, true), lit_ ? juce::Colours::black : colour,
                juce::Justification::centred);
    paint::text(g, label_, {6 + kSquare + 10, 0, getWidth() - (6 + kSquare + 10) - 10, getHeight()},
                Fonts::sans(kTextPx), lit_ ? theme::kWhite : theme::kMuted, juce::Justification::centredLeft);
  }

  void clicked() override { bar_.services_.chain.toggleFootswitch(letter_); }

private:
  static constexpr int kSquare = 26;
  SceneBar& bar_;
  juce::String letter_, label_;
  bool lit_;
};

SceneBar::SceneBar(Services& services) : services_(services) {
  for (int i = 0; i < 4; ++i) {
    scenes_.push_back(std::make_unique<SceneChip>(*this, i));
    addAndMakeVisible(*scenes_.back());
  }
  rename_.setFontSize(kTextPx);
  rename_.setCornerRadius(kChipH / 2.0f);
  rename_.setPadding(0, 14, 14);
  rename_.onEnter = [this] { endRename(true); };
  rename_.onEscape = [this] { endRename(false); };
  rename_.onBlur = [this] { endRename(true); };
  addChildComponent(rename_);
  services_.chain.addListener(this);
  chainChanged(services_.chain.state());
}

SceneBar::~SceneBar() { services_.chain.removeListener(this); }

void SceneBar::chainChanged(const ChainState& state) {
  for (int i = 0; i < 4; ++i) scenes_[static_cast<size_t>(i)]->set(sceneName(state, i), i == state.activeScene);
  rebuildSwitches();
  resized();
  repaint();
}

void SceneBar::rebuildSwitches() {
  const auto& state = services_.chain.state();
  juce::StringArray key;
  std::vector<std::tuple<juce::String, juce::String, bool>> next;
  for (const auto* letter : fork_colors::kFootswitches) {
    juce::StringArray names;
    bool allOn = true;
    for (const auto* item : state.toneBlocks()) {
      if (item->footswitch != letter) continue;
      names.add(services_.toneNotes.resolved(item->tone).product);
      allOn = allOn && item->params.enabled;
    }
    if (names.isEmpty()) continue;
    const auto label = names.joinIntoString(" + ");
    next.emplace_back(letter, label, allOn);
    key.add(juce::String(letter) + "|" + label + "|" + (allOn ? "1" : "0"));
  }
  // Rebuilding destroys the chip whose click may be running; defer when the
  // set changed, keep the old ones otherwise.
  if (key == switchKey_) return;
  switchKey_ = key;
  juce::MessageManager::callAsync([safe = juce::Component::SafePointer<SceneBar>(this), next] {
    if (safe == nullptr) return;
    safe->switches_.clear();
    for (const auto& [letter, label, lit] : next) {
      safe->switches_.push_back(std::make_unique<SwitchChip>(*safe, letter, label, lit));
      safe->addAndMakeVisible(*safe->switches_.back());
    }
    safe->resized();
  });
}

void SceneBar::resized() {
  const int cy = (getHeight() - kChipH) / 2;
  int x = 24;
  for (auto& chip : scenes_) {
    chip->setTopLeftPosition(x, cy);
    x += chip->getWidth() + kGap;
  }
  int right = getWidth() - 24;
  for (auto it = switches_.rbegin(); it != switches_.rend(); ++it) {
    (*it)->setTopLeftPosition(right - (*it)->getWidth(), cy);
    right -= (*it)->getWidth() + kGap;
  }
  if (renaming_ >= 0) {
    const auto& chip = *scenes_[static_cast<size_t>(renaming_)];
    rename_.setBounds(chip.getX(), cy, juce::jmax(160, chip.getWidth()), kChipH);
  }
}

void SceneBar::paint(juce::Graphics& g) {
  g.setColour(theme::kBorder);
  g.fillRect(0, 0, getWidth(), 1);
}

void SceneBar::beginRename(int index) {
  renaming_ = index;
  rename_.setPlaceholder("Scene " + fork_colors::sceneLetter(index));
  rename_.setText(services_.chain.state().sceneNames.size() > static_cast<size_t>(index)
                      ? services_.chain.state().sceneNames[static_cast<size_t>(index)]
                      : juce::String());
  rename_.setVisible(true);
  rename_.toFront(false);
  resized();
  rename_.focus();
}

void SceneBar::endRename(bool save) {
  if (renaming_ < 0) return;
  const int index = renaming_;
  renaming_ = -1;
  rename_.setVisible(false);
  if (save) services_.chain.renameScene(index, rename_.text());
}

}  // namespace t3k::ui
