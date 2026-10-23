// dJONt: the plugin. Mono in (a stereo input uses the left channel), the
// processed signal on every output channel. Input gain -> Wham -> Sustain ->
// Tight -> Grit -> Sub -> Lo-Fi -> output gain. Any parameter can be put on
// a MIDI CC (MIDI learn, from the editor); the mappings save with the state. Each module's power glides in and out over
// ~20 ms, so stomping one mid-riff doesn't click.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

#include "Dsp.h"
#include "PitchShift.h"
#include "Sustain.h"
#include "ClassicShift.h"
#include "WhamModes.h"
#include "Wham.h"

namespace djont {

class DjontProcessor : public juce::AudioProcessor,
                       private juce::AudioProcessorValueTreeState::Listener,
                       private juce::AsyncUpdater {
public:
  DjontProcessor();
  ~DjontProcessor() override;

  static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override {}
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }

  const juce::String getName() const override { return "dJONt"; }
  bool acceptsMidi() const override { return true; }
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
  // The wham pedal's live position (0..1) for the editor's rocker; -1 off.
  std::atomic<float> whamPosition{-1.0f};

  // MIDI learn. startLearn: the next CC that arrives is mapped to `id` (one
  // CC per parameter). Bool parameters follow the CC (>= 64 on), except
  // HOLD with LATCH on, which flips on each press so a momentary footswitch
  // latches. Called from the message thread.
  void startLearn(const juce::String& id);
  void cancelLearn() { learnIndex_.store(-1); }
  juce::String learning() const;  // the parameter waiting for a CC, or ""
  int ccFor(const juce::String& id) const;  // -1 when unmapped
  void forget(const juce::String& id);
  // The most recent learn that completed (for the editor's confirmation).
  std::atomic<int> learnedCount{0};

  // Wham's shifter buffer: the latency it adds while on.
  static constexpr PitchShift::Window kWhamWindow = PitchShift::Window::ms30;

private:
  enum Module { kSustain, kTight, kGrit, kSub, kLoFi, kNumModules };

  float value(const char* id) const { return params.getRawParameterValue(id)->load(); }
  void processMono(float* x, int n);
  void processWham(float* x, int n);
  void processSustain(float* x, int n);
  void handleMidi(const juce::MidiBuffer& midi);
  int indexOf(const juce::String& id) const;
  void writeMidiState();
  void readMidiState();
  // Latency follows Wham's power (reported from the message thread).
  void parameterChanged(const juce::String& id, float value) override;
  void handleAsyncUpdate() override;
  void updateLatency();

  Tight tight_;
  Grit grit_;
  Sub sub_;
  LoFi lofi_;
  Hold hold_;
  Sustainer sustainer_;
  bool sustainHoldMode_ = true;
  PitchShift whamShift_;  // CHORDS engine
  ClassicShift classic_;   // CLASSIC engine
  juce::SmoothedValue<float> whamFade_;
  juce::AudioBuffer<float> whamDry_, whamScratch_;
  Biquad whamTone_;
  bool whamToneOn_ = false, whamClassic_ = true;
  int whamInterval_ = 0;
  float whamMix_ = 1.0f;
  Wham wham_;
  Wham::Settings whamSettings_;
  float whamHeel_ = 0.0f, whamToe_ = 12.0f;
  double sampleRate_ = 48000.0;
  std::vector<juce::RangedAudioParameter*> midiParams_;
  std::array<std::atomic<int>, 128> ccMap_{};
  std::array<int, 128> ccLast_{};
  std::atomic<int> learnIndex_{-1};
  std::array<juce::SmoothedValue<float>, kNumModules> power_;
  juce::SmoothedValue<float> inGain_, outGain_;
  juce::AudioBuffer<float> dry_;  // one module's input, for the power crossfade

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DjontProcessor)
};

}  // namespace djont
