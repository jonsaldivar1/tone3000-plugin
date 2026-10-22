// dJONt's face. A color-graded photo behind everything (two looks: Chrome,
// safety orange on chrome foil; Red Foil, red on crumpled red/blue foil),
// with dark glass plates over it so the controls stay readable.
//   Header: the wordmark (d JON t, the name in the accent), Input / Output,
//           the output meter and the settings gear.
//   Body:   four modules (Tight, Grit, Sub, Lo-Fi), each with a power LED;
//           a module that's off dims its knobs.
//   Footer: a plain-text hint line: whatever the mouse is over, what it does
//           and (for a knob) its current value. No popup tooltips.
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
  juce::Slider slider_;
  juce::String name_, description_;
  int size_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
};

// Round power LED bound to a bool parameter.
class PowerLed : public juce::Button, public HintSource {
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

class DjontEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
  static constexpr int kWidth = 860, kHeight = 412;

  explicit DjontEditor(DjontProcessor& processor);
  ~DjontEditor() override;
  void paint(juce::Graphics& g) override;
  void resized() override;

  void setLook(const juce::String& lookId);
  void showSettings(bool show);
  juce::String currentHint() const { return hint_; }

private:
  void timerCallback() override;
  void updateHint();

  DjontProcessor& processor_;
  DjontLookAndFeel lnf_;
  Knob input_, output_;
  GearButton gear_;
  std::vector<std::unique_ptr<Module>> modules_;
  SettingsPanel settings_;
  juce::Image photo_, photoScaled_;
  float photoScaledAt_ = 0.0f;
  float meterDb_ = -60.0f;
  juce::Rectangle<int> meterBox_, ioPlate_, hintBox_;
  juce::String hint_;
};

}  // namespace djont
