// The HUSH controls, shared by dJONt, Parabola Bass Z and the Hush plugin:
// on/off, LEARN, Strength / Sensitivity / Release, and a meter per band
// showing how far it's turned down right now. Colours come from the host's
// LookAndFeel (Label::textColourId for ink, Slider::thumbColourId for the
// accent, ResizableWindow::backgroundColourId for the panel), knobs are
// drawn by the host's drawRotarySlider, and every control carries a help
// text (the hosts' hint bars show it) and its parameter id as the
// "paramId" property (the hosts' right-click MIDI learn reads it).
//
// Parameters it expects in the APVTS: hushOn (bool), hushStrength (%),
// hushSensitivity (dB), hushRelease (ms).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

#include "Hush.h"

namespace hushui {

void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout);
// Reads the four parameters and hands them to the engine (audio thread; the
// ids are looked up once and cached by the caller through these pointers).
struct Params {
  std::atomic<float>*on = nullptr, *strength = nullptr, *sensitivity = nullptr, *release = nullptr;
  void attach(juce::AudioProcessorValueTreeState& state);
  void apply(Hush& hush) const;
};
// The learned floors live in the plugin state as the property "hushFloors".
void saveFloors(const Hush& hush, juce::ValueTree& state);
void loadFloors(Hush& hush, const juce::ValueTree& state);

class HushPanel : public juce::Component, private juce::Timer {
public:
  HushPanel(juce::AudioProcessorValueTreeState& state, Hush& hush,
            std::function<juce::Font(const juce::Component&, float)> displayFont, bool showDone);
  ~HushPanel() override;
  void paint(juce::Graphics& g) override;
  void resized() override;
  std::function<void()> onClose;
  juce::String hint() const;

private:
  void timerCallback() override;
  struct Knob {
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::String label;
  };
  void setupKnob(Knob& k, const juce::String& id, const juce::String& label, const juce::String& help);

  juce::AudioProcessorValueTreeState& state_;
  Hush& hush_;
  std::function<juce::Font(const juce::Component&, float)> display_;
  juce::TextButton power_{"OFF"}, learn_{"LEARN"}, done_{"DONE"};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment_;
  Knob strength_, sensitivity_, release_;
  std::array<float, Hush::kBands> shown_{};
  juce::String status_;
  int learnedSeen_ = 0;
  bool showDone_ = true;
  juce::Rectangle<int> meterBox_, textBox_;
};

}  // namespace hushui
