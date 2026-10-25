// Parabola Bass Z's face (design A: a dJONt sibling). A color-graded photo
// behind everything, dark glass plates over it.
//   Header:    the wordmark; Gate, Input, Output, the output meter, ? and
//              the settings gear.
//   Crossover: the three bands drawn as curves, with their levels; drag the
//              two handles to move the split points.
//   Bands:     LOW / MID / HIGH, each a NAM slot (load or drop a .nam),
//              Drive / Level and Comp (low) or Tone (mid, high). ADV flips a
//              band to its Bass / Mid / Treble and its CAB switch.
//   Bottom:    the 6-band graphic EQ and the cab (load or drop an IR).
//   Footer:    the hint bar: whatever the mouse is over, what it does, its
//              value.
// "?" opens the manual over the body; the gear opens Settings (skin, and in
// the standalone app the audio device). The window resizes with a fixed
// aspect ratio; the layout is drawn at its design size and scaled.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>
#include <vector>

#include "ParabolaProcessor.h"
#include "Presets.h"

namespace parabola {

struct Look {
  juce::String id, name;
  juce::Colour bg, ink, muted, accent, plate, border, rule, grid, faceHi, faceLo, track, slot, hintBg, scrim;
  std::array<juce::Colour, kNumBands> band;
};
const Look& lookFor(const juce::String& id);  // unknown ids get Foil
extern const char* const kLookIds[5];         // foil, inferno, hologram, ram, ember

class PbzLookAndFeel : public juce::LookAndFeel_V4 {
public:
  PbzLookAndFeel();
  void setLook(const Look& look);
  const Look& look() const { return *look_; }

  void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end,
                        juce::Slider&) override;
  void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max,
                        juce::Slider::SliderStyle, juce::Slider&) override;
  static juce::Font display(const juce::Component& c, float px);
  static juce::Font label(float px);
  static const Look& lookOf(const juce::Component& c);

private:
  juce::Typeface::Ptr anton_;
  const Look* look_;
};

// Anything the hint bar can describe.
struct HintSource {
  virtual ~HintSource() = default;
  virtual juce::String hint() const = 0;
};

// Controls bound to a parameter carry its id as this component property:
// the right-click MIDI menu and the hint bar's "MIDI CC n" read it.
inline const juce::Identifier kParamIdProp{"paramId"};

// Right-click is the MIDI menu, so these ignore it.
class KnobSlider : public juce::Slider {
public:
  void mouseDown(const juce::MouseEvent& e) override {
    if (!e.mods.isPopupMenu()) juce::Slider::mouseDown(e);
  }
  void mouseDrag(const juce::MouseEvent& e) override {
    if (!e.mods.isPopupMenu()) juce::Slider::mouseDrag(e);
  }
  void mouseUp(const juce::MouseEvent& e) override {
    if (!e.mods.isPopupMenu()) juce::Slider::mouseUp(e);
  }
};
template <typename Base>
class LeftClickOnly : public Base {
public:
  using Base::Base;
  void mouseDown(const juce::MouseEvent& e) override {
    if (!e.mods.isPopupMenu()) Base::mouseDown(e);
  }
  void mouseUp(const juce::MouseEvent& e) override {
    if (!e.mods.isPopupMenu()) Base::mouseUp(e);
  }
};

class Knob : public juce::Component, public HintSource {
public:
  Knob(juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& name, int size,
       juce::String description);
  void setColourIndex(int band) { band_ = band; slider_.getProperties().set("band", band); }
  void setBipolar() { slider_.getProperties().set("bipolar", true); }
  void paint(juce::Graphics& g) override;
  void resized() override;
  juce::String hint() const override;
  juce::String valueText() const;
  static constexpr int kLabelH = 14;

private:
  juce::String name_, description_;
  int size_, band_ = -1;
  KnobSlider slider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
  juce::RangedAudioParameter* param_ = nullptr;
};

// A small outlined text button (LOAD, ADV, BACK, x).
class Chip : public juce::Button, public HintSource {
public:
  Chip(const juce::String& text, juce::String description);
  void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
  juce::String hint() const override { return description_; }
  void setDescription(juce::String d) { description_ = std::move(d); }

private:
  juce::String description_;
};

// A chip bound to a bool parameter (CAB ON, a band's CAB): filled when on.
class ParamChip : public LeftClickOnly<juce::Button>, public HintSource {
public:
  ParamChip(juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& text,
            juce::String description);
  void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
  juce::String hint() const override;

private:
  juce::String description_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment_;
};

class PowerLed : public LeftClickOnly<juce::Button>, public HintSource {
public:
  PowerLed(juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& name, int band);
  void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
  juce::String hint() const override;
  std::function<void(bool)> onPower;

private:
  juce::String name_;
  int band_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment_;
};

// PRE | POST for a drive band: split before the capture, or after it.
class SplitSwitch : public juce::Component, public HintSource {
public:
  SplitSwitch(juce::AudioProcessorValueTreeState& state, const juce::String& id, int band);
  void paint(juce::Graphics& g) override;
  void mouseDown(const juce::MouseEvent& e) override;
  juce::String hint() const override;

private:
  int band_, index_ = 0;
  std::unique_ptr<juce::ParameterAttachment> attachment_;
};

// The crossover display: the three bands' shapes (with their level and EQ),
// two draggable split handles.
class CrossoverView : public juce::Component, public HintSource {
public:
  explicit CrossoverView(ParabolaProcessor& processor);
  ~CrossoverView() override;
  void paint(juce::Graphics& g) override;
  void mouseMove(const juce::MouseEvent& e) override;
  void mouseDown(const juce::MouseEvent& e) override;
  void mouseDrag(const juce::MouseEvent& e) override;
  void mouseUp(const juce::MouseEvent& e) override;
  void mouseDoubleClick(const juce::MouseEvent& e) override;
  void mouseExit(const juce::MouseEvent&) override { hover_ = -1; repaint(); }
  juce::String hint() const override;
  void tick();  // 30 Hz: repaint when anything it draws changed

private:
  juce::Rectangle<float> plot() const;
  float xForHz(double hz) const;
  double hzForX(float x) const;
  float yForDb(double db) const;
  double bandDb(int band, double hz) const;
  int handleAt(juce::Point<float> p) const;

  ParabolaProcessor& processor_;
  std::array<std::unique_ptr<juce::ParameterAttachment>, 2> attachments_;
  std::array<float, 2> hz_{180.0f, 1600.0f};
  int drag_ = -1, hover_ = -1;
  juce::String signature_;
};

// One band: header, NAM slot, knobs; ADV flips to its EQ and CAB switch.
class BandStrip : public juce::Component, public juce::FileDragAndDropTarget {
public:
  BandStrip(ParabolaProcessor& processor, int band);
  void paint(juce::Graphics& g) override;
  void resized() override;
  void setBack(bool back);
  void refreshSlot();
  bool isInterestedInFileDrag(const juce::StringArray& files) override;
  void fileDragEnter(const juce::StringArray&, int, int) override { dragOver_ = true; repaint(); }
  void fileDragExit(const juce::StringArray&) override { dragOver_ = false; repaint(); }
  void filesDropped(const juce::StringArray& files, int, int) override;
  static constexpr int kSlotY = 50, kSlotH = 34;

private:
  void chooseFile();
  void load(const juce::File& file);
  void setPowered(bool on);

  ParabolaProcessor& processor_;
  int band_;
  bool back_ = false, dragOver_ = false;
  PowerLed power_;
  Chip adv_, load_, clear_;
  std::vector<std::unique_ptr<Knob>> front_, backKnobs_;
  ParamChip cab_;
  std::unique_ptr<SplitSwitch> split_;
  juce::String slotName_, slotNote_;
  std::unique_ptr<juce::FileChooser> chooser_;
};

class GeqPanel : public juce::Component {
public:
  explicit GeqPanel(juce::AudioProcessorValueTreeState& state);
  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  class Fader : public KnobSlider, public HintSource {
  public:
    juce::String hint() const override;
    juce::String label;
  };
  std::array<Fader, 6> faders_;
  std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 6> attachments_;
};

class CabPanel : public juce::Component, public juce::FileDragAndDropTarget {
public:
  explicit CabPanel(ParabolaProcessor& processor);
  void paint(juce::Graphics& g) override;
  void resized() override;
  void refresh();
  bool isInterestedInFileDrag(const juce::StringArray& files) override;
  void fileDragEnter(const juce::StringArray&, int, int) override { dragOver_ = true; repaint(); }
  void fileDragExit(const juce::StringArray&) override { dragOver_ = false; repaint(); }
  void filesDropped(const juce::StringArray& files, int, int) override;

private:
  void chooseFile();
  ParabolaProcessor& processor_;
  Chip load_, clear_;
  ParamChip on_;
  juce::String name_, routing_;
  bool dragOver_ = false;
  std::unique_ptr<juce::FileChooser> chooser_;
};

class IconButton : public juce::Button, public HintSource {
public:
  enum class Kind { gear, help };
  IconButton(Kind kind, juce::String description);
  void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
  juce::String hint() const override { return description_; }

private:
  Kind kind_;
  juce::String description_;
};

class SettingsPanel : public juce::Component, public HintSource, private juce::Timer {
public:
  SettingsPanel(ParabolaProcessor& processor, std::function<void(const juce::String&)> onLook);
  ~SettingsPanel() override;
  void paint(juce::Graphics& g) override;
  void resized() override;
  void syncLook(const juce::String& lookId);
  juce::String hint() const override;
  std::function<void()> onClose;

private:
  void timerCallback() override;
  void updateLatency();

  ParabolaProcessor& processor_;
  juce::AudioDeviceManager* devices_ = nullptr;
  juce::OwnedArray<juce::TextButton> looks_;
  juce::TextButton done_{"DONE"};
  juce::ToggleButton hear_{"Hear yourself"};
  juce::Viewport viewport_;
  std::unique_ptr<juce::AudioDeviceSelectorComponent> selector_;
  juce::String latency_;
};

// The manual, from manual.json (the PDF's module reference uses the same file).
class ManualPanel : public juce::Component, public HintSource {
public:
  ManualPanel();
  ~ManualPanel() override;
  void paint(juce::Graphics& g) override;
  void resized() override;
  void lookAndFeelChanged() override;
  juce::String hint() const override;
  void showSection(int index);
  int sectionCount() const { return static_cast<int>(sections_.size()); }
  std::function<void()> onClose;

  struct Section {
    juce::String id, title, tagline, what, how, sound;
    juce::StringArray tries;
    std::vector<std::pair<juce::String, juce::String>> controls;
  };
  static std::vector<Section> load();

private:
  class Page : public juce::Component {
  public:
    void build(const Section& s, int width);
    void paint(juce::Graphics& g) override;

  private:
    juce::TextLayout layout_;
  };
  std::vector<Section> sections_;
  juce::OwnedArray<juce::TextButton> tabs_;
  juce::TextButton done_{"DONE"};
  juce::Viewport viewport_;
  Page page_;
  int current_ = 0;
};

// The header's preset bar: < name > SAVE DEL, and MY GEAR.
class PresetBar : public juce::Component, public HintSource {
public:
  PresetBar(ParabolaProcessor& processor, PresetStore& store);
  void paint(juce::Graphics& g) override;
  void resized() override;
  juce::String hint() const override;
  void refresh();
  void step(int delta);
  std::function<void(const juce::String&)> onMessage;  // flashes in the hint bar
  std::function<void()> onMyGear;

private:
  void showMenu();
  void load(const juce::String& name);
  void beginSave();
  void finishSave(bool keep);
  ParabolaProcessor& processor_;
  PresetStore& store_;
  Chip prev_, next_, save_, del_, gear_;
  juce::TextButton name_;
  juce::TextEditor editor_;
  int delArmed_ = 0;
};

// My Gear: every capture and IR in the tones folders. Click a capture to
// load it into a band, an IR to load it into the cab.
class MyGearPanel : public juce::Component, public HintSource, private juce::ListBoxModel {
public:
  explicit MyGearPanel(ParabolaProcessor& processor);
  void paint(juce::Graphics& g) override;
  void resized() override;
  juce::String hint() const override;
  void rescan();
  std::function<void()> onClose;
  std::function<void(const juce::String&)> onMessage;

private:
  int getNumRows() override { return static_cast<int>(shown_.size()); }
  void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
  void listBoxItemClicked(int row, const juce::MouseEvent& e) override;
  void filter();

  ParabolaProcessor& processor_;
  std::vector<Library::Entry> all_, shown_;
  juce::TextEditor search_;
  void folderMenu();
  juce::TextButton done_{"DONE"}, rescan_{"RESCAN"}, open_{"OPEN FOLDER"}, folders_{"FOLDERS"};
  juce::ListBox list_;
  std::unique_ptr<juce::FileChooser> chooser_;
};

// The tuner (the output is muted while it's open).
class TunerPanel : public juce::Component, public HintSource, private juce::Timer {
public:
  explicit TunerPanel(ParabolaProcessor& processor);
  void paint(juce::Graphics& g) override;
  void resized() override;
  void visibilityChanged() override;
  juce::String hint() const override {
    return "Tuner: play one open string. The output is muted while the tuner is open. DONE (or TUNER) closes it.";
  }
  std::function<void()> onClose;

private:
  void timerCallback() override;
  ParabolaProcessor& processor_;
  juce::TextButton done_{"DONE"};
  double hz_ = 0.0, smoothCents_ = 0.0;
};

// Everything, laid out at its design size (kWidth x kHeight).
class ParabolaView : public juce::Component, private juce::Timer {
public:
  static constexpr int kWidth = 1200, kHeight = 780;

  explicit ParabolaView(ParabolaProcessor& processor);
  ~ParabolaView() override;
  void paint(juce::Graphics& g) override;
  void resized() override;

  void setLook(const juce::String& lookId);
  void showSettings(bool show) { showOverlay(show ? &settings_ : nullptr); }
  void showManual(bool show, int section = -1);
  void showMyGear(bool show);
  void showTuner(bool show) { showOverlay(show ? &tunerPanel_ : nullptr); }
  void flash(const juce::String& message, int ticks = 240);
  void mouseDown(const juce::MouseEvent& e) override;
  void showAdvanced(bool show);
  juce::String currentHint() const { return hint_; }

private:
  void timerCallback() override;
  void updateHint();
  void showOverlay(juce::Component* overlay);
  void paintOverChildren(juce::Graphics& g) override;

  ParabolaProcessor& processor_;
  PbzLookAndFeel lnf_;
  PresetStore presets_;
  Knob gate_, noise_, pitch_, input_, output_;
  IconButton help_, gear_;
  Chip tuner_;
  PresetBar presetBar_;
  CrossoverView crossover_;
  std::array<std::unique_ptr<BandStrip>, kNumBands> bands_;
  GeqPanel geq_;
  CabPanel cab_;
  SettingsPanel settings_;
  ManualPanel manual_;
  MyGearPanel myGear_;
  TunerPanel tunerPanel_;
  juce::Component* learnTarget_ = nullptr;
  juce::String learnName_, flash_;
  int flashTicks_ = 0, learnedSeen_ = 0;
  juce::Image photo_, photoScaled_;
  float photoScaledAt_ = 0.0f;
  float meterDb_ = -60.0f;
  int filesSeen_ = -1;
  juce::Rectangle<int> meterBox_, headPlate_, hintBox_;
  juce::String hint_;
};

class ParabolaEditor : public juce::AudioProcessorEditor {
public:
  explicit ParabolaEditor(ParabolaProcessor& processor);
  void resized() override;
  void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::black); }

  void setLook(const juce::String& lookId) { view_.setLook(lookId); }
  void showSettings(bool show) { view_.showSettings(show); }
  void showManual(bool show, int section = -1) { view_.showManual(show, section); }
  void showMyGear(bool show) { view_.showMyGear(show); }
  void showTuner(bool show) { view_.showTuner(show); }
  void showAdvanced(bool show) { view_.showAdvanced(show); }
  juce::String currentHint() const { return view_.currentHint(); }

private:
  ParabolaProcessor& processor_;
  ParabolaView view_;
};

}  // namespace parabola
