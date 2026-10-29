// dJONt: the plugin. Mono in (a stereo input uses the left channel), the
// processed signal on every output channel. Input gain -> Wham -> Sustain ->
// Tight -> Grit -> Sub -> Lo-Fi -> output gain. Any parameter can be put on
// a MIDI CC (MIDI learn, from the editor); the mappings save with the state. Each module's power glides in and out over
// ~20 ms, so stomping one mid-riff doesn't click.
//
// Scenes (A-H, 1-8 of them): each keeps its own copy of every setting except
// the performance ones (HOLD, the Wham pedal's position) and Hush's learned
// noise. Picked by clicking, by the host-automatable "Scene" parameter, or by
// MIDI (a CC learned on Scene picks by value: 0 = A, 1 = B, ...).
// Presets (Presets.h) hold everything a scene set holds; Program Change n
// loads the nth preset in the list.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <memory>

#include "Dsp.h"
#include "Hush.h"
#include "HushPanel.h"
#include "PitchShift.h"
#include "Sustain.h"
#include "ClassicShift.h"
#include "WhamModes.h"
#include "Wham.h"
#include "Presets.h"

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

  // Host programs = the preset list (Program Change n loads the nth preset;
  // VST3 hosts deliver PC this way, raw MIDI formats via handleMidi).
  static constexpr int kNumPrograms = 128;
  int getNumPrograms() override { return kNumPrograms; }
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int, const juce::String&) override {}

  void getStateInformation(juce::MemoryBlock& dest) override;
  void setStateInformation(const void* data, int size) override;

  juce::AudioProcessorValueTreeState params;

  // Output peak (dB, -60 floor) for the editor's meter.
  std::atomic<float> outputPeakDb{-60.0f};
  // The wham pedal's live position (0..1) for the editor's rocker; -1 off.
  std::atomic<float> whamPosition{-1.0f};

  // HUSH, the DI de-noiser (first in the chain, before Wham; shared/Hush.h).
  Hush hush;

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

  // ---- Scenes (message thread). Tweaks made in a scene stay in it: the
  // live parameters are the active scene, folded back into its slot when it
  // is left. A scene never visited starts as a copy of the one you left.
  static constexpr int kMaxScenes = 8;
  static const juce::StringArray& sceneParamIds();  // what a scene keeps
  int sceneCount() const { return sceneCount_; }
  int activeScene() const { return activeScene_; }
  juce::String sceneName(int index) const;  // "" = unnamed
  bool selectScene(int index);
  bool addScene();          // a copy of the current one, switched to; up to 8
  bool removeScene(int index);  // later scenes move up a letter; one stays
  bool renameScene(int index, const juce::String& name);
  // Bumped on every scene change (the editor repaints on it).
  std::atomic<int> sceneRevision{0};

  // ---- Presets (message thread).
  PresetStore& presets() { return presets_; }
  juce::String currentPreset() const { return params.state.getProperty("presetName").toString(); }
  bool loadPreset(const juce::String& name);
  // Saves the current settings and scenes under `name` (replacing one of
  // that name in place) and makes it current.
  bool savePreset(const juce::String& name);
  bool renamePreset(const juce::String& from, const juce::String& to);
  bool deletePreset(const juce::String& name);
  std::atomic<int> presetRevision{0};
  // What a preset or the plugin state holds besides the parameters.
  juce::ValueTree sceneTree() const;
  void applySceneTree(const juce::ValueTree& tree);

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

  struct SceneSlot {
    bool stored = false;
    std::vector<std::pair<juce::String, float>> values;  // id -> real value
  };
  SceneSlot captureScene() const;
  void applyScene(const SceneSlot& slot);
  void syncSceneParam();
  int sceneCount_ = 4, activeScene_ = 0;
  std::array<juce::String, kMaxScenes> sceneNames_;
  std::array<SceneSlot, kMaxScenes> scenes_;
  std::atomic<int> pendingScene_{-1}, pendingProgram_{-1};
  bool applyingScene_ = false;
  PresetStore presets_;

  Tight tight_;
  Focus focus_;
  bool focusMode_ = false;
  // Global Blend: dry DI (after Hush) against the whole board, the dry
  // delayed to match Wham's latency.
  std::vector<float> blendRing_, blendDry_;
  int blendPos_ = 0;
  juce::SmoothedValue<float> blend_;
  Grit grit_;
  Sub sub_;
  LoFi lofi_;
  Hold hold_;
  Sustainer sustainer_;
  bool sustainHoldMode_ = true;
  PitchShift whamShift_;  // CHORDS engine
  hushui::Params hushParams_;
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
