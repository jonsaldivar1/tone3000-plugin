// dJONt's face. A color-graded photo behind everything (two looks: Chrome,
// safety orange on chrome foil; Red Foil, red on crumpled red/blue foil),
// with dark glass plates over it so the controls stay readable.
//   Header: the wordmark (d JON t, the name in the accent), Input / Output,
//           the output meter and the settings gear.
//   Body:   five modules (Wham, Tight, Grit, Sub, Lo-Fi), each with a power
//           LED; a module that's off dims its knobs. Wham carries its pedal
//           and named presets.
//   Footer: a plain-text hint line: whatever the mouse is over, what it does
//           and (for a knob) its current value. No popup tooltips.
// The "?" opens the manual over the body (topics from manual.json).
// The gear opens Settings over the body: the look, and in the standalone app
// the audio device (driver type such as ASIO, device, input / output
// channels, sample rate, buffer size), the latency it adds up to, and Hear
// Yourself. In a DAW the DAW owns all of that.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "DjontProcessor.h"
#include "WhamPresets.h"

namespace djont {

// The palette of one look.
struct Look {
  juce::String id, name;
  juce::Colour bg, ink, accent, plate, plateBorder, rule, faceHi, faceLo, knobRing, track, hintBg, hintText;
};
const Look& lookFor(const juce::String& id);  // unknown ids get Chrome
extern const char* const kLookIds[2];         // "chrome", "redfoil"

class DjontLookAndFeel : public juce::LookAndFeel_V4 {
public:
  DjontLookAndFeel();
  void setLook(const Look& look);
  const Look& look() const { return *look_; }

  void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end,
                        juce::Slider&) override;
  // Anton, from whichever DjontLookAndFeel `c` paints with. The typeface
  // lives in the look-and-feel (editor lifetime), never in a static: a
  // process-lifetime typeface would be destroyed inside DllMain on unload.
  static juce::Font display(const juce::Component& c, float px);
  static juce::Font label(float px);  // bold sans, tracked
  static const Look& lookOf(const juce::Component& c);

private:
  juce::Typeface::Ptr anton_;
  const Look* look_;
};

// Anything the hint bar can describe. Components without it fall back to
// their help text.
struct HintSource {
  virtual ~HintSource() = default;
  virtual juce::String hint() const = 0;
};

// Every control bound to a parameter carries its id as the component
// property "paramId": the editor's right-click menu (MIDI learn) and the
// hint bar's "MIDI CC n" read it.
inline const juce::Identifier kParamIdProp{"paramId"};

// A slider a right-click doesn't turn (right-click is the MIDI menu).
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

// A button a right-click doesn't press.
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

// A knob with its label underneath; while it's being turned the label shows
// the value.
class Knob : public juce::Component, public HintSource {
public:
  Knob(juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& name, int size,
       juce::String description);
  void resized() override;
  void paint(juce::Graphics& g) override;
  // The value arc grows from 12 o'clock (Input / Output: 0 dB in the middle).
  void setBipolar() { slider_.getProperties().set("bipolar", true); }
  juce::String hint() const override;
  static constexpr int kLabelH = 16;

private:
  juce::String valueText() const;
  juce::RangedAudioParameter* param_ = nullptr;
  KnobSlider slider_;
  juce::String name_, description_;
  int size_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
};

// Round power LED bound to a bool parameter.
class PowerLed : public LeftClickOnly<juce::Button>, public HintSource {
public:
  PowerLed(juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& name,
           juce::String description);
  void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
  juce::String hint() const override;
  std::function<void(bool)> onPower;

private:
  juce::String name_, description_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment_;
};

// Two-way text switch (Sub's CLEAN | SYNTH, Settings' look).
class Segmented : public juce::Component, public HintSource {
public:
  Segmented(juce::StringArray labels, juce::String description, int width);
  void setIndex(int index);
  int index() const { return index_; }
  void paint(juce::Graphics& g) override;
  void mouseDown(const juce::MouseEvent& e) override;
  juce::String hint() const override { return description_; }
  std::function<void(int)> onSelect;

private:
  juce::StringArray labels_;
  juce::String description_;
  int index_ = 0;
};

// Segmented bound to a choice parameter.
class ParamSegmented : public Segmented {
public:
  ParamSegmented(juce::AudioProcessorValueTreeState& state, const juce::String& id, juce::StringArray labels,
                 juce::String description, int width);

private:
  std::unique_ptr<juce::ParameterAttachment> attachment_;
};

class Module : public juce::Component {
public:
  Module(juce::AudioProcessorValueTreeState& state, const juce::String& title, const juce::String& powerId,
         juce::String powerDescription);
  void addKnob(std::unique_ptr<Knob> knob);
  void setExtra(std::unique_ptr<juce::Component> extra);
  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  void setPowered(bool on);
  juce::String title_;
  PowerLed power_;
  juce::Component body_;  // dims when powered off
  std::vector<std::unique_ptr<Knob>> knobs_;
  std::unique_ptr<juce::Component> extra_;
};

// A small text toggle bound to a bool parameter (Wham's RETRIG).
class ParamToggle : public LeftClickOnly<juce::Button>, public HintSource {
public:
  ParamToggle(juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& label,
              juce::String description, int width);
  void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
  juce::String hint() const override { return description_; }

private:
  juce::String description_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment_;
};

// A small text button (SAVE / DEL).
class Chip : public LeftClickOnly<juce::Button>, public HintSource {
public:
  Chip(const juce::String& label, juce::String description, int width);
  void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
  juce::String hint() const override { return description_; }
  void setDescription(juce::String d) { description_ = std::move(d); }

private:
  juce::String description_;
};

// Wham's virtual expression pedal: a ridged treadle with a bar at the
// position (bottom = heel, top = toe). PEDAL: drag it (or scroll); it is the
// whamPedal parameter, so automation and MIDI move it too; double-click
// sends it back to the heel. AUTO: it follows the LFO.
class WhamPedal : public juce::Component, public HintSource {
public:
  explicit WhamPedal(DjontProcessor& processor);
  void tick();  // editor timer: follow the live position
  void paint(juce::Graphics& g) override;
  void mouseDown(const juce::MouseEvent& e) override;
  void mouseDrag(const juce::MouseEvent& e) override;
  void mouseUp(const juce::MouseEvent& e) override;
  void mouseDoubleClick(const juce::MouseEvent& e) override;
  void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
  juce::String hint() const override;

private:
  bool pedalMode() const;
  float shown() const;
  DjontProcessor& processor_;
  juce::RangedAudioParameter* pedal_;
  std::unique_ptr<juce::ParameterAttachment> attachment_;
  float value_ = 0.0f, dragStart_ = 0.0f, lastShown_ = -2.0f;
};

// The Wham module. Front: PEDAL | AUTO, RETRIG, CLASSIC | CHORDS, INTERVAL,
// Heel / Toe (Custom) or Mix, Rate / Shape (Auto) or Glide (Pedal), the
// pedal. Back (ADV): the voicing, Grain / Buffer / Tone / Mix. Both: the
// preset row (pick one to load it; SAVE names and saves the current
// settings; DEL deletes your own).
class WhamModule : public juce::Component {
public:
  WhamModule(DjontProcessor& processor);
  void tick();
  void setBack(bool back);  // the ADV side
  void paint(juce::Graphics& g) override;
  void resized() override;
  static constexpr int kWidth = 280;

private:
  void setPowered(bool on);
  void refreshPresets();
  void startNaming();
  void finishNaming(bool save);
  void syncVisibility(bool force = false);

  DjontProcessor& processor_;
  PowerLed power_;
  Chip adv_;
  juce::Component body_;
  ParamSegmented mode_;
  ParamToggle retrig_;
  ParamSegmented algo_;
  juce::ComboBox interval_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> intervalAttachment_;
  Knob heel_, toe_, rate_, shape_, glide_, mix_;
  Knob grain_, buffer_, tone_, mixBack_;
  bool back_ = false;
  int shownState_ = -1;
  WhamPedal pedal_;
  juce::ComboBox presets_;
  juce::TextEditor name_;
  Chip save_, del_;
  WhamPresets store_;
  juce::String shownPreset_;
  int ticks_ = 0, delArmedTicks_ = 0;
};

// The big HOLD switch: lit while holding. LATCH on: a click toggles it;
// off: it holds while the mouse is down (like a footswitch).
class HoldButton : public juce::Component, public HintSource {
public:
  HoldButton(juce::AudioProcessorValueTreeState& state);
  void paint(juce::Graphics& g) override;
  void mouseDown(const juce::MouseEvent& e) override;
  void mouseUp(const juce::MouseEvent& e) override;
  juce::String hint() const override;

private:
  bool latch() const;
  juce::AudioProcessorValueTreeState& state_;
  std::unique_ptr<juce::ParameterAttachment> attachment_;
  bool on_ = false;
};

// The Sustain module: HOLD | SUSTAINER picks the page.
//   Hold:      the HOLD switch, LATCH, CHORD | PILE-UP, Fade, Level.
//   Sustainer: FUND | HARM | MIX, Sustain, Bloom.
class SustainModule : public juce::Component {
public:
  explicit SustainModule(DjontProcessor& processor);
  void tick();
  void setBack(bool back);  // the ADV side
  void paint(juce::Graphics& g) override;
  void resized() override;
  static constexpr int kWidth = 250;

private:
  void setPowered(bool on);
  void syncVisibility(bool force = false);
  DjontProcessor& processor_;
  PowerLed power_;
  Chip adv_;
  juce::Component body_;
  ParamSegmented page_;
  HoldButton hold_;
  ParamToggle latch_;
  ParamSegmented holdType_;
  Knob fade_, level_;
  ParamSegmented sustainerType_;
  Knob amount_, bloom_;
  // ADV: Hold's Shimmer / Low Cut / Tone; the Sustainer's Interval / Floor /
  // Response / Tone.
  Knob shimmer_, lowCut_, holdTone_;
  ParamSegmented sustainerInterval_;
  Knob floor_, response_, sustainerTone_;
  bool back_ = false;
  int shownState_ = -1;
};

// The gear in the header.
class GearButton : public juce::Button, public HintSource {
public:
  GearButton();
  void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
  juce::String hint() const override;
};

// Settings, over the body.
class SettingsPanel : public juce::Component, public HintSource, private juce::Timer {
public:
  SettingsPanel(DjontProcessor& processor, std::function<void(const juce::String&)> onLook);
  ~SettingsPanel() override;
  void paint(juce::Graphics& g) override;
  void resized() override;
  void syncLook(const juce::String& lookId);
  juce::String hint() const override;
  std::function<void()> onClose;

private:
  void timerCallback() override;
  void updateLatency();

  DjontProcessor& processor_;
  juce::AudioDeviceManager* devices_ = nullptr;  // standalone only
  Segmented look_;
  juce::TextButton done_{"DONE"};
  juce::ToggleButton hear_{"Hear yourself"};
  juce::Viewport viewport_;
  std::unique_ptr<juce::AudioDeviceSelectorComponent> selector_;
  juce::String latency_;
};

// The "?" in the header.
class HelpButton : public juce::Button, public HintSource {
public:
  HelpButton();
  void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
  juce::String hint() const override;
};

// The manual, over the body: topics on the left, the selected one on the
// right (what it does, how it works, the sound, things to try, controls).
// The text comes from djont/manual/manual.json, the same file the PDF
// manual is built from, so the two never disagree.
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
  static std::vector<Section> load();  // parses the embedded manual.json

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

// Everything you see, laid out at its design size (kWidth x kHeight). The
// editor scales it to the window, so resizing keeps the layout.
class DjontView : public juce::Component, private juce::Timer {
public:
  static constexpr int kWidth = 1290, kHeight = 490;

  explicit DjontView(DjontProcessor& processor);
  ~DjontView() override;
  void paint(juce::Graphics& g) override;
  void resized() override;

  void setLook(const juce::String& lookId);
  void showSettings(bool show);
  void showManual(bool show, int section = -1);
  void showAdvanced(bool show);  // flips Wham and Sustain to their ADV side
  juce::String currentHint() const { return hint_; }

  // MIDI learn: right-click any control.
  void mouseDown(const juce::MouseEvent& e) override;

private:
  void timerCallback() override;
  void updateHint();
  void showOverlay(juce::Component* overlay);  // settings, manual, or nullptr for the controls
  void paintOverChildren(juce::Graphics& g) override;
  juce::Component* learnTarget_ = nullptr;
  juce::String learnName_, confirm_;
  int confirmTicks_ = 0, learnedSeen_ = 0;

  DjontProcessor& processor_;
  DjontLookAndFeel lnf_;
  Knob input_, output_;
  GearButton gear_;
  HelpButton help_;
  WhamModule wham_;
  SustainModule sustain_;
  std::vector<std::unique_ptr<Module>> modules_;
  SettingsPanel settings_;
  ManualPanel manual_;
  juce::Image photo_, photoScaled_;
  float photoScaledAt_ = 0.0f;
  float meterDb_ = -60.0f;
  juce::Rectangle<int> meterBox_, ioPlate_, hintBox_;
  juce::String hint_;
};

// The plugin window: resizable (drag the corner), fixed aspect ratio, 60% to
// 200% of the design size; the size is remembered in the plugin's state.
class DjontEditor : public juce::AudioProcessorEditor {
public:
  explicit DjontEditor(DjontProcessor& processor);
  void resized() override;
  void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::black); }

  void setLook(const juce::String& lookId) { view_.setLook(lookId); }
  void showSettings(bool show) { view_.showSettings(show); }
  void showManual(bool show, int section = -1) { view_.showManual(show, section); }
  void showAdvanced(bool show) { view_.showAdvanced(show); }
  juce::String currentHint() const { return view_.currentHint(); }

private:
  DjontProcessor& processor_;
  DjontView view_;
};

}  // namespace djont
