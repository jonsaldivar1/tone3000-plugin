// Hush as its own plugin: the DI de-noiser from dJONt and Parabola Bass Z
// (shared/Hush.h), for any guitar or bass DI. Mono in (the first input),
// the same signal on every output; zero latency.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Hush.h"
#include "HushPanel.h"

class HushProcessor : public juce::AudioProcessor {
public:
  HushProcessor();
  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override {}
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  using AudioProcessor::processBlock;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }
  const juce::String getName() const override { return "Hush"; }
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
  Hush hush;
  std::atomic<float> inputPeakDb{-100.0f}, outputPeakDb{-100.0f};

private:
  hushui::Params hushParams_;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HushProcessor)
};
