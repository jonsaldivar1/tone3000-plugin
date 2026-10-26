#include "Faceplate.h"

#include <array>

#include "core/CustomIcons.h"
#include "core/Design.h"
#include "core/Fonts.h"
#include "core/Icons.h"
#include "core/Paint.h"
#include "core/Theme.h"
#include "widgets/Clickable.h"
#include "widgets/Popover.h"

namespace t3k::ui {

namespace {

// Faceplate.tsx: padding 16 30 (input/output align with the UV meters).
constexpr int kPadX = 30, kPadY = 16;
constexpr int kGroupGap = 10;  // knob ↔ companion within a group
constexpr int kToneGap = 24;   // between the three tone knobs
// Gate ↔ Pitch inside the effects cluster: the tone stack's knob spacing,
// so the two read as one group against the wider peer gap.
constexpr int kEffectsGap = kToneGap;
// Chrome boxes in a bottom-aligned row sit on the secondary-knob centreline.
constexpr int kChromeLift = theme::faceplateChromeLift(theme::kKnobSizeSecondary);

// Input mode trigger: [glyph 17][gap 3][chevron 10] + 2px padding + 1px border.
constexpr int kInputModeWidth = 1 + 2 + (theme::kIconSize + 3) + 3 + 10 + 2 + 1;
constexpr int kMenuWidth = 224;  // fits "Stereo Dual Mono (L&R)"
constexpr int kMenuPadTop = 12, kMenuPadSide = 8, kMenuPadBottom = 8;
constexpr int kMenuTitleHeight = 13;  // 11px/600 line
constexpr int kMenuTitlePadBottom = 8;
constexpr int kMenuRowHeight = 33;    // 9px padding + 13px text line
constexpr int kMenuGap = 14;          // bottom: calc(100% + 14)

Knob::Options primaryKnob(const char* label, const KnobScale& scale, float def, help::Key help) {
  Knob::Options o;
  o.label = label;
  o.scale = &scale;
  o.defaultValue = def;
  o.help = help;
  return o;
}

Knob::Options secondaryKnob(const char* label, const KnobScale& scale, float def, help::Key help,
                            Knob::Variant variant = Knob::Variant::full) {
  auto o = primaryKnob(label, scale, def, help);
  o.size = theme::kKnobSizeSecondary;
  o.thumb = Knob::Thumb::secondary;
  o.variant = variant;
  return o;
}

// The mode's glyph, shared by the trigger and the menu rows.
void drawModeGlyph(juce::Graphics& g, InputMode mode, juce::Rectangle<float> box, juce::Colour colour) {
  if (mode == InputMode::stereo || mode == InputMode::dualMono) {
    // Both stereo feeds share the one glyph; the row label tells them apart.
    Icons::draw(g, custom_icons::kInputStereo, juce::Rectangle<float>(17, 10).withCentre(box.getCentre()),
                colour);
    return;
  }
  g.setFont(Fonts::mono(11, true));
  g.setColour(colour);
  g.drawText(mode == InputMode::left ? "L" : "R", box, juce::Justification::centred, false);
}

struct ModeOption {
  InputMode mode;
  const char* label;
  help::Key help;
};
// The menu's rows, in order. The first is the Stereo routing worded for a
// mono chain, where it sums; with stereo chains it reads "Stereo" (one
// channel per chain) and the Dual Mono row is dropped (it has nothing to add
// there; native treats the selection as Stereo).
constexpr std::array<ModeOption, 4> kModeOptions{
    ModeOption{InputMode::stereo, "Stereo SUM (L+R)", help::Key::inputModeSum},
    ModeOption{InputMode::dualMono, "Stereo Dual Mono (L&R)", help::Key::inputModeDualMono},
    ModeOption{InputMode::left, "Left", help::Key::inputModeLeft},
    ModeOption{InputMode::right, "Right", help::Key::inputModeRight},
};
constexpr ModeOption kStereoChainsOption{InputMode::stereo, "Stereo", help::Key::inputModeStereo};

}  // namespace

// Input mode
// How a stereo source feeds the chain. The trigger (current glyph + down
// caret) opens a flat floating menu above the plate listing the routings.
// Rows lead with the input mode they belong to, since that is the menu's
// heading: Stereo (the default) shows the two-circle glyph and on a mono
// chain sums the channels ("Stereo SUM (L+R)"), with stereo chains each
// takes its own ("Stereo"). "Stereo Dual Mono (L&R)" (mono chain only) runs
// each channel through its own copy of the chain under the same glyph; L/R
// take only that channel (mirrored onto both). Every non-default routing keeps the filled
// "engaged" look so it's obvious at a glance. With stereo chains the Dual
// Mono row is hidden and a Dual Mono selection shows as Stereo (that's what
// it does there). While a chain branch is active the chain has a single mono
// source, so the stereo feeds are unavailable (native enforces the same).
class Faceplate::InputModeButton : public Clickable {
public:
  explicit InputModeButton(Services& services) : Clickable("Input Mode"), services_(services) {
    setSize(kInputModeWidth, theme::kIconBoxSize);
    setHelpText(help::text(help::Key::inputMode));
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    onClick = [this] {
      if (menuOpen())
        closeMenu();
      else
        openMenu();
    };
  }

  void setState(InputMode mode, bool branched, bool stereoChains) {
    // Dual Mono has no meaning for stereo chains: present it as Stereo.
    if (stereoChains && mode == InputMode::dualMono) mode = InputMode::stereo;
    if (mode_ == mode && branched_ == branched && stereoChains_ == stereoChains) return;
    mode_ = mode;
    branched_ = branched;
    stereoChains_ = stereoChains;
    repaint();
    if (menuOpen()) {
      closeMenu();
      openMenu();
    }
  }

  void paintButton(juce::Graphics& g, bool, bool) override {
    const auto box = getLocalBounds().toFloat();
    if (mode_ != InputMode::stereo) paint::fill(g, box, theme::kIconBoxRadius, theme::kHighlight);
    auto content = getLocalBounds().reduced(1 + 2, 1);
    drawModeGlyph(g, mode_, content.removeFromLeft(theme::kIconSize + 3).toFloat(), theme::kWhite);
    content.removeFromLeft(3);
    Icons::draw(g, Icon::ChevronDown, juce::Rectangle<float>(10, 10).withCentre(content.toFloat().getCentre()),
                theme::kMuted);
  }

private:
  class Row : public Clickable {
  public:
    Row(const ModeOption& option, bool selected)
        : Clickable(option.label), option_(option), selected_(selected) {
      setHelpText(help::text(option.help));
      setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    InputMode mode() const { return option_.mode; }
    void paintButton(juce::Graphics& g, bool highlighted, bool) override {
      if (highlighted) paint::fill(g, getLocalBounds().toFloat(), 8.0f, theme::kHighlight);
      const auto colour = selected_ ? theme::kWhite : theme::kMuted;
      auto content = getLocalBounds().reduced(12, 0);
      drawModeGlyph(g, option_.mode, content.removeFromLeft(theme::kIconBoxSize).toFloat(), colour);
      content.removeFromLeft(12);
      paint::text(g, option_.label, content, Fonts::sans(13), colour);
    }

  private:
    ModeOption option_;
    bool selected_;
  };

  class Menu : public Popover {
  public:
    Menu(InputMode mode, bool branched, bool stereoChains, std::function<void(InputMode)> onPick) {
      for (const auto& entry : kModeOptions) {
        const bool stereoFeed = entry.mode == InputMode::stereo || entry.mode == InputMode::dualMono;
        if (branched && stereoFeed) continue;
        if (stereoChains && entry.mode == InputMode::dualMono) continue;
        const ModeOption& option =
            stereoChains && entry.mode == InputMode::stereo ? kStereoChainsOption : entry;
        auto row = std::make_unique<Row>(option, option.mode == mode);
        row->onClick = [this, onPick, m = option.mode] {
          close();
          onPick(m);
        };
        addAndMakeVisible(*row);
        rows_.push_back(std::move(row));
      }
      setSize(kMenuWidth, kBorder * 2 + kMenuPadTop + kMenuTitleHeight + kMenuTitlePadBottom +
                              kMenuRowHeight * static_cast<int>(rows_.size()) + kMenuPadBottom);
    }
    void paint(juce::Graphics& g) override {
      const auto box = getLocalBounds().toFloat();
      paint::fill(g, box, theme::kPanelCorner, theme::kPanelBg);
      paint::border(g, box, theme::kPanelCorner, theme::kBorder);
      auto title = contentBounds().reduced(kMenuPadSide, 0).removeFromTop(kMenuPadTop + kMenuTitleHeight);
      title.removeFromTop(kMenuPadTop);
      paint::text(g, "Input Mode", title.reduced(12, 0), Fonts::sans(11, true), theme::kSubtle);
    }
    void resized() override {
      auto area = contentBounds().reduced(kMenuPadSide, 0);
      area.removeFromTop(kMenuPadTop + kMenuTitleHeight + kMenuTitlePadBottom);
      for (auto& row : rows_) row->setBounds(area.removeFromTop(kMenuRowHeight));
    }

  private:
    std::vector<std::unique_ptr<Row>> rows_;
  };

  bool menuOpen() const { return menu_ != nullptr && menu_->isOpen(); }
  void closeMenu() {
    if (menu_ != nullptr) menu_->close();  // kept alive: we may be inside its row's click
  }
  // Rebuilt per open: the row set depends on `branched` and `stereoChains`.
  void openMenu() {
    if (auto* old = menu_.release()) juce::MessageManager::callAsync([old] { delete old; });
    menu_ = std::make_unique<Menu>(mode_, branched_, stereoChains_,
                                   [this](InputMode mode) { services_.chain.setInputMode(mode); });
    menu_->open(*this, Popover::Align::left, kMenuGap, 0, Popover::Placement::above);
  }

  Services& services_;
  InputMode mode_ = InputMode::stereo;
  bool branched_ = false;
  bool stereoChains_ = false;
  std::unique_ptr<Menu> menu_;
};

// Fork (T3KJ): split channel strip switch. Three segments in the input
// group's chrome row: Shared (the link glyph: one strip for both lanes, the
// stock behaviour), 1 and 2 (the strip is split; the plate's knobs edit that
// lane). Only shown while two lanes run (stereo chains or dual mono).
class Faceplate::LaneSwitch : public juce::Component {
public:
  static constexpr int kSegment = 24;
  static constexpr int kWidth = 3 * kSegment + 2;  // + the frame

  explicit LaneSwitch(std::function<void(int)> onPick) {
    static const help::Key keys[] = {help::Key::laneStripLinked, help::Key::laneStrip1, help::Key::laneStrip2};
    static const char* titles[] = {"Shared Strip", "Lane 1 Strip", "Lane 2 Strip"};
    for (int i = 0; i < 3; ++i) {
      auto seg = std::make_unique<Segment>(*this, i, titles[i]);
      seg->setHelpText(help::text(keys[i]));
      seg->onClick = [onPick, i] { onPick(i); };
      addAndMakeVisible(*seg);
      segments_.push_back(std::move(seg));
    }
    setSize(kWidth, theme::kIconBoxSize);
  }

  void setSelected(int index) {
    if (selected_ == index) return;
    selected_ = index;
    for (auto& s : segments_) s->repaint();
  }

  void paint(juce::Graphics& g) override {
    paint::border(g, getLocalBounds().toFloat(), theme::kIconBoxRadius, theme::kBorder);
  }
  void resized() override {
    for (int i = 0; i < 3; ++i)
      segments_[static_cast<size_t>(i)]->setBounds(1 + i * kSegment, 1, kSegment, getHeight() - 2);
  }

private:
  class Segment : public Clickable {
  public:
    Segment(LaneSwitch& owner, int index, const juce::String& title)
        : Clickable(title), owner_(owner), index_(index) {
      setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    void paintButton(juce::Graphics& g, bool highlighted, bool) override {
      const bool on = owner_.selected_ == index_;
      const auto box = getLocalBounds().toFloat();
      if (on || highlighted) paint::fill(g, box, theme::kIconBoxRadius, theme::kHighlight);
      const auto colour = on ? theme::kWhite : theme::kMuted;
      if (index_ == 0) {
        // Link glyph: two interlocked rings, the strip shared by both lanes.
        g.setColour(colour);
        const auto c = box.getCentre();
        g.drawEllipse(juce::Rectangle<float>(8.0f, 8.0f).withCentre(c.translated(-2.5f, 0)), 1.4f);
        g.drawEllipse(juce::Rectangle<float>(8.0f, 8.0f).withCentre(c.translated(2.5f, 0)), 1.4f);
      } else {
        g.setFont(Fonts::mono(11, true));
        g.setColour(colour);
        g.drawText(juce::String(index_), getLocalBounds(), juce::Justification::centred, false);
      }
    }

  private:
    LaneSwitch& owner_;
    int index_;
  };

  std::vector<std::unique_ptr<Segment>> segments_;
  int selected_ = 0;
};

// Plate
Faceplate::Faceplate(Services& services)
    : services_(services),
      input_(services.backend, "inputLevel",
             primaryKnob("Input", scales::gainDb(), 0.5f, help::Key::inputLevel)),
      inputMode_(std::make_unique<InputModeButton>(services)),
      gate_(services),
      pitch_(services),
      gateEnabled_(services.backend, "gateEnabled"),
      pitchEnabled_(services.backend, "pitchEnabled"),
      bass_(services.backend, "toneBass",
            primaryKnob("Bass", scales::tone(), 0.5f, help::Key::toneBass)),
      middle_(services.backend, "toneMid",
              primaryKnob("Middle", scales::tone(), 0.5f, help::Key::toneMiddle)),
      treble_(services.backend, "toneTreble",
              primaryKnob("Treble", scales::tone(), 0.5f, help::Key::toneTreble)),
      tonePower_(services.backend, "toneEqEnabled", help::Key::tonePower),
      spread_(services, ImageFeature::spread),
      align_(services, ImageFeature::align),
      autoBalance_(Icon::Equal, ChromeIconButton::Tone::armed, help::Key::autoBalance),
      balance_(services.backend, "outputBalance",
               secondaryKnob("Bal", scales::balanceDb(), 0.5f, help::Key::outputBalance,
                             Knob::Variant::bipolar)),
      output_(services.backend, "outputLevel",
              primaryKnob("Output", scales::gainDb(), 0.5f, help::Key::outputLevel)),
      spreadEnabled_(services.backend, "spreadEnabled"),
      laneSwitch_(std::make_unique<LaneSwitch>([this](int lane) { pickLane(lane); })),
      splitStrip_(services.backend, "tk3jSplitStrip"),
      input2_(services.backend, "inputLevel2",
              primaryKnob("Input 2", scales::gainDb(), 0.5f, help::Key::inputLevel)),
      gate2_(services, "2"),
      gateEnabled2_(services.backend, "gateEnabled2"),
      bass2_(services.backend, "toneBass2", primaryKnob("Bass 2", scales::tone(), 0.5f, help::Key::toneBass)),
      middle2_(services.backend, "toneMid2",
               primaryKnob("Middle 2", scales::tone(), 0.5f, help::Key::toneMiddle)),
      treble2_(services.backend, "toneTreble2",
               primaryKnob("Treble 2", scales::tone(), 0.5f, help::Key::toneTreble)),
      tonePower2_(services.backend, "toneEqEnabled2", help::Key::tonePower),
      output2_(services.backend, "outputLevel2",
               primaryKnob("Output 2", scales::gainDb(), 0.5f, help::Key::outputLevel)) {
  setOpaque(true);

  addAndMakeVisible(input_);
  addChildComponent(*inputMode_);
  // The effects start hidden; syncFlags shows what the view settings and
  // the power switches ask for.
  addChildComponent(gate_);
  addChildComponent(pitch_);

  // Powered-off section: knobs + labels dim and go inert; the power button
  // stays outside the dimmed wrapper, bright and clickable.
  for (auto* k : {&bass_, &middle_, &treble_}) toneDim_.addAndMakeVisible(*k);
  toneDim_.setOff(!tonePower_.value(), false);
  tonePower_.onValueChange = [this](bool on) { toneDim_.setOff(!on); };
  addAndMakeVisible(toneDim_);
  addAndMakeVisible(tonePower_);

  // Its hint (the reason it's dimmed) is set by syncFlags.
  imageDim_.addChildComponent(spread_);
  imageDim_.addChildComponent(align_);
  addAndMakeVisible(imageDim_);

  autoBalance_.onClick = [this] { services_.autoBalance.toggle(); };
  addChildComponent(autoBalance_);
  addChildComponent(balance_);
  addAndMakeVisible(output_);

  // Fork (T3KJ): lane 2's strip, laid out over the stock one (syncFlags
  // shows one set at a time) and the Shared | 1 | 2 switch.
  addChildComponent(*laneSwitch_);
  addChildComponent(input2_);
  addChildComponent(gate2_);
  for (auto* k : {&bass2_, &middle2_, &treble2_}) toneDim2_.addAndMakeVisible(*k);
  toneDim2_.setOff(!tonePower2_.value(), false);
  tonePower2_.onValueChange = [this](bool on) { toneDim2_.setOff(!on); };
  addChildComponent(toneDim2_);
  addChildComponent(tonePower2_);
  addChildComponent(output2_);

  for (auto* p : {&spreadEnabled_, &gateEnabled_, &pitchEnabled_, &splitStrip_, &gateEnabled2_})
    p->onChange = [this] { syncFlags(); };
  services_.prefs.addListener(this);
  services_.chain.addListener(this);
  services_.autoBalance.addListener(this);
  syncFlags();
}

Faceplate::~Faceplate() {
  services_.autoBalance.removeListener(this);
  services_.chain.removeListener(this);
  services_.prefs.removeListener(this);
}

void Faceplate::autoMeasureChanged() { autoBalance_.setOn(services_.autoBalance.listening()); }

void Faceplate::prefChanged(const juce::String& key) {
  if (key == UiPrefs::kShowGateControl || key == UiPrefs::kShowPitchControl) syncFlags();
}

void Faceplate::pickLane(int lane) {
  if (lane == 0) {
    splitStrip_.set(false);
  } else {
    editLane_ = lane - 1;
    if (!splitStrip_.boolValue()) {
      seedLane2FromLane1();
      splitStrip_.set(true);
    }
  }
  syncFlags();
}

void Faceplate::seedLane2FromLane1() {
  static const std::pair<const char*, const char*> pairs[] = {
      {"inputLevel", "inputLevel2"},       {"gateEnabled", "gateEnabled2"}, {"gateThreshold", "gateThreshold2"},
      {"toneEqEnabled", "toneEqEnabled2"}, {"toneBass", "toneBass2"},       {"toneMid", "toneMid2"},
      {"toneTreble", "toneTreble2"},       {"outputLevel", "outputLevel2"},
  };
  for (const auto& [from, to] : pairs) {
    auto* p = services_.backend.parameter(to);
    if (p == nullptr || std::abs(p->getValue() - p->getDefaultValue()) > 1.0e-4f) return;  // lane 2 has its own
  }
  for (const auto& [from, to] : pairs) {
    auto* src = services_.backend.parameter(from);
    auto* dst = services_.backend.parameter(to);
    if (src == nullptr || dst == nullptr) continue;
    dst->beginChangeGesture();
    dst->setValueNotifyingHost(src->getValue());
    dst->endChangeGesture();
  }
}

void Faceplate::syncFlags() {
  const auto& chain = services_.chain.state();

  // Fork (T3KJ): split channel strip. The switch shows while two lanes run;
  // split, the plate shows the edited lane's set.
  const bool twoLanes = chain.stereoEnabled || chain.dualMonoActive;
  const bool split = twoLanes && splitStrip_.boolValue();
  const bool lane2 = split && editLane_ == 1;
  bool relayout = false;
  if (laneSwitch_->isVisible() != twoLanes) {
    laneSwitch_->setVisible(twoLanes);
    relayout = true;
  }
  laneSwitch_->setSelected(split ? editLane_ + 1 : 0);
  input_.setVisible(!lane2);
  input2_.setVisible(lane2);
  toneDim_.setVisible(!lane2);
  tonePower_.setVisible(!lane2);
  toneDim2_.setVisible(lane2);
  tonePower2_.setVisible(lane2);
  output_.setVisible(!lane2);
  output2_.setVisible(lane2);

  // Effects: the view setting decides (gate on, pitch off by default),
  // except that a powered effect always shows. A preset or host state that
  // switches one on brings its group out; switching it off again lets the
  // setting hide it. Split, either lane's powered gate counts.
  const bool showGate = services_.prefs.getBool(UiPrefs::kShowGateControl, true) || gateEnabled_.boolValue() ||
                        (split && gateEnabled2_.boolValue());
  const bool showPitch = services_.prefs.getBool(UiPrefs::kShowPitchControl, false) || pitchEnabled_.boolValue();
  relayout = relayout || showGate != showGate_ || showPitch != showPitch_;
  showGate_ = showGate;
  showPitch_ = showPitch;
  gate_.setVisible(showGate && !lane2);
  gate2_.setVisible(showGate && lane2);
  pitch_.setVisible(showPitch);
  if (relayout) resized();

  // The rig can drive two distinct output channels at all (stereo host bus /
  // 2+ channel output device). False dims the Spread group and makes it
  // inert, power button included: the feature is unavailable, not merely off.
  const bool stereoOutput = chain.stereoOutput;
  // Two independent chains are running: auto balance shows and the slot
  // swaps Spread for Align. Mono-mode spread doesn't need auto balance: both
  // channels carry the same chain, so their energy already matches.
  const bool stereoChains = chain.stereoEnabled;
  // Dual mono running: the mono chain outputs two real channels (one take
  // each). Spread has nothing to double, so it dims with its own reason;
  // Balance and auto balance apply to the two voices like to two chains.
  const bool dualMono = chain.dualMonoActive;
  // The balance trim is audible (stereo chains on any rig, spread on a
  // stereo rig, or dual mono; on a mono rig it trims the chains inside the
  // mono sum).
  const bool balanceActive = stereoChains || (spreadEnabled_.boolValue() && stereoOutput) || dualMono;

  if (inputMode_->isVisible() != chain.stereoInput) {
    // The input group grows by the button, so the plate spreads again.
    inputMode_->setVisible(chain.stereoInput);
    resized();
  }
  inputMode_->setState(chain.inputMode, chain.branch.has_value(), stereoChains);

  spread_.setVisible(!stereoChains);
  align_.setVisible(stereoChains);
  // The dimmed group's hint names the reason (mono rig, or dual mono).
  imageDim_.setHelpText(help::text(dualMono ? help::Key::spreadDualMono : help::Key::spreadMonoOutput));
  imageDim_.setOff(!stereoChains && (!stereoOutput || dualMono));

  autoBalance_.setVisible(stereoChains || dualMono);
  autoBalance_.setOn(services_.autoBalance.listening());
  balance_.setVisible(balanceActive);
}

void Faceplate::paint(juce::Graphics& g) {
  g.fillAll(theme::kSurfaceRaised);
  paint::hairlineH(g, 0, static_cast<float>(getWidth()), 0, theme::kBorder);
}

void Faceplate::resized() {
  const auto content = getLocalBounds().reduced(kPadX, kPadY);
  const int baseline = content.getBottom();  // every label slot ends here
  const int knobBottom = baseline + Knob::kEditorOverflow;
  const auto knobY = [&](int size) { return knobBottom - Knob::heightFor(size); };
  const int chromeY = baseline - theme::kIconBoxSize + kChromeLift;
  const int primary = theme::kKnobSizePrimary, secondary = theme::kKnobSizeSecondary, box = theme::kIconBoxSize;

  // The peers share the plate width (space-between). Their footprints are
  // fixed; the input group grows when the input-mode button shows, and the
  // effects cluster holds whichever of gate / pitch is showing (none: the
  // cluster leaves the row and four peers spread).
  const int inputW = primary + (inputMode_->isVisible() ? kGroupGap + inputMode_->getWidth() : 0) +
                     (laneSwitch_->isVisible() ? kGroupGap + LaneSwitch::kWidth : 0);
  const bool gate = showGate_, pitch = showPitch_;
  const int effectsW =
      (gate ? GateGroup::kWidth : 0) + (gate && pitch ? kEffectsGap : 0) + (pitch ? PitchGroup::kWidth : 0);
  const int toneW = 3 * primary + 2 * kToneGap + kGroupGap + box;
  const int imageW = StereoImageGroup::kWidth;
  const int outputW = box + kGroupGap + secondary + kGroupGap + primary;
  const int peers = 4 + (effectsW > 0 ? 1 : 0);
  const float gap =
      (content.getWidth() - (inputW + effectsW + toneW + imageW + outputW)) / static_cast<float>(peers - 1);

  float x = static_cast<float>(content.getX());
  const auto at = design::snap;

  // Input [+ mode]
  input_.setTopLeftPosition(at(x), knobY(primary));
  input2_.setTopLeftPosition(at(x), knobY(primary));
  inputMode_->setTopLeftPosition(at(x) + primary + kGroupGap, chromeY);
  laneSwitch_->setTopLeftPosition(
      at(x) + primary + kGroupGap + (inputMode_->isVisible() ? inputMode_->getWidth() + kGroupGap : 0), chromeY);
  x += inputW + gap;

  // Effects: Gate + power (+ deck), Pitch + power (+ deck)
  if (gate) {
    gate_.setTopLeftPosition(at(x), knobY(secondary));
    gate2_.setTopLeftPosition(at(x), knobY(secondary));
    x += GateGroup::kWidth + (pitch ? kEffectsGap : 0);
  }
  if (pitch) {
    pitch_.setTopLeftPosition(at(x), knobY(secondary));
    x += PitchGroup::kWidth;
  }
  if (effectsW > 0) x += gap;

  // Bass / Middle / Treble + power
  for (auto* dim : {&toneDim_, &toneDim2_})
    dim->setBounds(at(x), knobY(primary), 3 * primary + 2 * kToneGap, Knob::heightFor(primary));
  for (auto* k : {&bass_, &bass2_}) k->setTopLeftPosition(0, 0);
  for (auto* k : {&middle_, &middle2_}) k->setTopLeftPosition(primary + kToneGap, 0);
  for (auto* k : {&treble_, &treble2_}) k->setTopLeftPosition(2 * (primary + kToneGap), 0);
  for (auto* p : {&tonePower_, &tonePower2_})
    p->setTopLeftPosition(at(x) + 3 * primary + 2 * kToneGap + kGroupGap, chromeY);
  x += toneW + gap;

  // Spread / Align slot
  imageDim_.setBounds(at(x), knobBottom - StereoImageGroup::height(), imageW, StereoImageGroup::height());
  spread_.setTopLeftPosition(0, 0);
  align_.setTopLeftPosition(0, 0);
  x += imageW + gap;

  // [=][Bal][Output]: the (=) sits on the outer edge, keeping Bal next to the
  // main knob.
  autoBalance_.setTopLeftPosition(at(x), chromeY);
  balance_.setTopLeftPosition(at(x) + box + kGroupGap, knobY(secondary));
  for (auto* k : {&output_, &output2_})
    k->setTopLeftPosition(at(x) + box + kGroupGap + secondary + kGroupGap, knobY(primary));
}

}  // namespace t3k::ui
