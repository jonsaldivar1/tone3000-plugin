// Parabola Bass Z: a 3-band parallel bass processor, in the spirit of a
// Parallax X. Mono in, the same signal out on every output channel.
//
//   DI -> Input -> Gate -> Crossover (LR4, two splits)
//      LOW : Drive -> [NAM, optional] -> Comp -> Bass/Mid/Treble -> Level
//      MID : Drive -> NAM -> Tone (low-pass) -> Bass/Mid/Treble -> Level
//      HIGH: Drive -> NAM -> Tone (low-pass) -> Bass/Mid/Treble -> Level
//   bands with CAB on (mid and high by default) -> cab IR; the rest bypass it
//   -> sum -> 6-band graphic EQ -> Output
//
// A band whose capture already includes a cab ("full rig" in the .nam
// metadata) skips the IR automatically. Every capture's output is
// normalised from its loudness metadata, so captures swap at matched level.
//
// All of it runs at 48 kHz (the rate NAM A2 captures are trained at). At any
// other host rate the chain sits behind one Lanczos resampling boundary
// (AudioDSPTools, the same one NAM's own plugin uses) and reports its delay.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "Dsp.h"
#include "NamSlot.h"
#include "PitchShift.h"
#include "TunerDetector.h"

namespace dsp {
template <typename T, int NCHANS, size_t A> class ResamplingContainer;
}

namespace parabola {

enum Band { kLow = 0, kMid = 1, kHigh = 2 };
inline constexpr int kNumBands = 3;
inline const char* const kBandIds[kNumBands] = {"low", "mid", "high"};
inline const char* const kBandNames[kNumBands] = {"Low", "Mid", "High"};

class ParabolaProcessor : public juce::AudioProcessor, private juce::AsyncUpdater {
public:
  static constexpr double kInternalRate = 48000.0;

  ParabolaProcessor();
  ~ParabolaProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override {}
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  using AudioProcessor::processBlock;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }
  const juce::String getName() const override { return "Parabola Bass Z"; }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  double getTailLengthSeconds() const override { return 0.5; }
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String&) override {}
  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  // Files (message thread). Return "" on success, else why not.
  juce::String loadNam(int band, const juce::File& file);
  void clearNam(int band);
  juce::String loadIr(const juce::File& file);
  void clearIr();
  const NamSlot& slot(int band) const { return slots_[static_cast<size_t>(band)]; }
  bool irLoaded() const { return irLoaded_.load(); }
  juce::String irName() const;
  juce::String irPath() const;
  // Whether a band currently goes through the cab IR (its CAB switch, the
  // global CAB ON, an IR loaded, and the capture isn't a full rig).
  bool bandUsesCab(int band) const;
  // Bumps whenever a NAM or the IR changes (the editor polls it).
  int filesVersion() const { return filesVersion_.load(); }
  juce::String lastError() const;

  juce::AudioProcessorValueTreeState params;

  // Meters (audio thread writes, UI reads).
  std::atomic<float> outputPeakDb{-100.0f};
  std::array<std::atomic<float>, kNumBands> bandPeakDb{};
  std::atomic<float> compReductionDb{0.0f};
  std::atomic<bool> gateOpen{true};

  // Tuner: while it's on, the detector listens to the DI and the output is
  // muted (silent tuning, like a stage tuner pedal).
  TunerDetector tuner;
  void setTunerActive(bool on);
  bool tunerActive() const { return tunerOn_.load(); }

  // MIDI learn (as in dJONt). startLearn: the next CC that arrives is mapped
  // to `id`. Bool parameters follow the CC (>= 64 on). Message thread.
  void startLearn(const juce::String& id);
  void cancelLearn() { learnIndex_.store(-1); }
  juce::String learning() const;
  int ccFor(const juce::String& id) const;  // -1 when unmapped
  void forget(const juce::String& id);
  std::atomic<int> learnedCount{0};

  // The shifter's buffer: Parabola's added latency while Pitch isn't 0.
  static constexpr PitchShift::Window kPitchWindow = PitchShift::Window::ms30;

private:
  static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
  void prepareInternal(int maxInternalBlock);
  void processInternal(float* in, float* out, int n);
  void processInternalChunk(float* in, float* out, int n);
  float param(const char* id) const { return params.getRawParameterValue(id)->load(); }
  void bumpFiles() { filesVersion_.fetch_add(1); }
  void handleAsyncUpdate() override { updateLatency(); }
  void updateLatency();
  int indexOf(const juce::String& id) const;
  void handleMidi(const juce::MidiBuffer& midi);
  void writeMidiState();
  void readMidiState();

  PitchShift pitch_;
  std::atomic<float>* pitchP_ = nullptr;
  bool pitchWasOn_ = false;
  std::atomic<bool> tunerOn_{false};
  SmoothGain muteGain_;
  std::vector<juce::RangedAudioParameter*> midiParams_;
  std::array<std::atomic<int>, 128> ccMap_{};
  std::array<int, 128> ccLast_{};
  std::atomic<int> learnIndex_{-1};

  std::array<NamSlot, kNumBands> slots_;
  std::array<std::atomic<float>*, kNumBands> on_{}, drive_{}, level_{}, bass_{}, mid_{}, treble_{}, cab_{}, toneP_{};
  std::atomic<float> *input_ = nullptr, *output_ = nullptr, *gate_p_ = nullptr, *xoverLow_ = nullptr,
                     *xoverHigh_ = nullptr, *cabOn_ = nullptr, *lowComp_ = nullptr;
  std::array<std::atomic<float>*, 6> geqP_{};

  Gate gate_;
  Crossover crossover_;
  Compressor comp_;
  std::array<Biquad, kNumBands> tone_;
  std::array<float, kNumBands> toneHz_{};
  std::array<BandEq, kNumBands> eq_;
  GraphicEq geq_;
  std::array<SmoothGain, kNumBands> driveGain_, levelGain_;
  SmoothGain inputGain_, outputGain_;
  juce::dsp::Convolution convolver_;
  std::atomic<bool> irLoaded_{false};
  juce::String irName_, irPath_, lastError_;
  juce::CriticalSection fileLock_;
  std::atomic<int> filesVersion_{0};

  std::array<std::vector<float>, kNumBands> bandBuf_;
  std::vector<float> cabBuf_, directBuf_, monoIn_, monoOut_;
  int internalMax_ = 0;

  using Resampler = dsp::ResamplingContainer<float, 1, 12>;
  std::unique_ptr<Resampler> resampler_;
  std::function<void(float**, float**, int)> internalFn_;
  double hostRate_ = kInternalRate;
  int hostMaxBlock_ = 512;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParabolaProcessor)
};

}  // namespace parabola
