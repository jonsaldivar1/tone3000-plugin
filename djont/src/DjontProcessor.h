// dJONt: the plugin. Mono in (a stereo input uses the left channel), the
// processed signal on every output channel. Input gain -> Tight -> Grit ->
// Sub -> Lo-Fi -> output gain. Each module's power glides in and out over
// ~20 ms, so stomping one mid-riff doesn't click.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

#include "Dsp.h"

namespace djont {

class DjontProcessor : public juce::AudioProcessor {
public:
  DjontProcessor();
  ~DjontProcessor() override = default;

  static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override {}
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }

  const juce::String getName() const override { return "dJONt"; }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }

  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String&) override {}

  void getStateInformation(juce::MemoryBlock& dest) override;
  void setStateInformation(const void* data, int size) override;

  juce::AudioProcessorValueTreeState params;

  // Output peak (dB, -60 floor) for the editor's meter.
  std::atomic<float> outputPeakDb{-60.0f};

private:
  enum Module { kTight, kGrit, kSub, kLoFi, kNumModules };

  float value(const char* id) const { return params.getRawParameterValue(id)->load(); }
  void processMono(float* x, int n);

  Tight tight_;
  Grit grit_;
  Sub sub_;
  LoFi lofi_;
  std::array<juce::SmoothedValue<float>, kNumModules> power_;
  juce::SmoothedValue<float> inGain_, outGain_;
  juce::AudioBuffer<float> dry_;  // one module's input, for the power crossfade

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DjontProcessor)
};

}  // namespace djont
