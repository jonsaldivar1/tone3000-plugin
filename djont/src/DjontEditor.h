// dJONt's face: a bone-white enclosure, black ink, one safety-orange
// accent. Header: the wordmark, Input / Output and an output meter. Below,
// four modules side by side (Tight, Grit, Sub, Lo-Fi), each with a power LED;
// a module that's off dims its knobs.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "DjontProcessor.h"

namespace djont {

namespace colours {
inline const juce::Colour bone{0xffece6d6};
inline const juce::Colour ink{0xff151413};
inline const juce::Colour inkSoft{0xff5c574e};
inline const juce::Colour accent{0xffff5a1f};
}  // namespace colours

class DjontLookAndFeel : public juce::LookAndFeel_V4 {
public:
  DjontLookAndFeel();
  void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end,
                        juce::Slider&) override;
  // Anton, from whichever DjontLookAndFeel `c` paints with. The typeface
  // lives in the look-and-feel (editor lifetime), never in a static: a
  // process-lifetime typeface would be destroyed inside DllMain on unload.
  static juce::Font display(const juce::Component& c, float px);
  static juce::Font label(float px);  // bold sans

private:
  juce::Typeface::Ptr anton_;
};

// A knob with its label underneath; while it's being turned the label shows
// the value.
class Knob : public juce::Component {
public:
  Knob(juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& name, int size);
  void resized() override;
  void paint(juce::Graphics& g) override;
  // The value arc grows from 12 o'clock (Input / Output: 0 dB in the middle).
  void setBipolar() { slider_.getProperties().set("bipolar", true); }
  static constexpr int kLabelH = 18;

private:
  juce::Slider slider_;
  juce::String name_;
  int size_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
};

// Round power LED bound to a bool parameter.
class PowerLed : public juce::Button {
public:
  PowerLed(juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& name);
  void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
  std::function<void(bool)> onPower;

private:
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment_;
};

// CLEAN | SYNTH for the Sub module.
class ModeSwitch : public juce::Component {
public:
  ModeSwitch(juce::AudioProcessorValueTreeState& state, const juce::String& id);
  void paint(juce::Graphics& g) override;
  void mouseDown(const juce::MouseEvent& e) override;

private:
  juce::RangedAudioParameter* param_;
  int index_ = 0;
  std::unique_ptr<juce::ParameterAttachment> attachment_;
};

class Module : public juce::Component {
public:
  Module(juce::AudioProcessorValueTreeState& state, const juce::String& title, const juce::String& powerId);
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

class DjontEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
  static constexpr int kWidth = 860, kHeight = 372;

  explicit DjontEditor(DjontProcessor& processor);
  ~DjontEditor() override;
  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  void timerCallback() override;

  DjontProcessor& processor_;
  DjontLookAndFeel lnf_;
  Knob input_, output_;
  std::vector<std::unique_ptr<Module>> modules_;
  float meterDb_ = -60.0f;
  juce::Rectangle<int> meterBox_;
};

}  // namespace djont
