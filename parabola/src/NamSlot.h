// One band's NAM capture: loads a .nam file on the message thread, hands
// it to the audio thread, and runs it in place on a mono buffer.
//
// Loading never blocks the audio thread: the new model is built, reset and
// prewarmed off the audio thread, then swapped in under a spin lock the
// audio thread only ever try-locks (a block that loses the race passes its
// band through unprocessed, once). The old model is destroyed on the
// message thread.
//
// Output level is normalised from the capture's own loudness metadata to
// kTargetLoudnessDb, so swapping captures doesn't jump the band's level.
// The file's metadata also says whether the capture includes a cab ("full
// rig"); the processor skips the cab IR for such a band.
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <atomic>
#include <memory>
#include <vector>

namespace nam { class DSP; }

namespace parabola {

class NamSlot {
public:
  static constexpr float kTargetLoudnessDb = -18.0f;

  struct Info {
    juce::String name;      // the capture's name (metadata, else the file name)
    juce::String path;      // full path of the loaded file
    juce::String gearType;  // metadata gear_type: amp, pedal, full-rig, ...
    juce::String modeledBy;
    bool fullRig = false;   // includes a cab (gear_type full-rig / amp_cab)
    bool hasLoudness = false;
    double loudnessDb = kTargetLoudnessDb;
    double expectedRate = 48000.0;
  };

  NamSlot();
  ~NamSlot();

  // Message thread. Returns an empty string on success, else a reason.
  juce::String load(const juce::File& file);
  void clear();
  // Re-prepares the loaded model (call from prepareToPlay).
  void prepare(double sampleRate, int maxBlockSize);

  bool isLoaded() const { return loaded_.load(); }
  bool isFullRig() const { return fullRig_.load(); }
  Info info() const;  // message thread

  // Audio thread: processes `n` samples in place. Pass-through if empty.
  void process(float* io, int n);

  // Reads a .nam file's metadata without building the model.
  static Info readInfo(const juce::File& file);

private:
  juce::SpinLock lock_;
  std::unique_ptr<nam::DSP> model_;  // guarded by lock_
  float normGain_ = 1.0f;            // guarded by lock_
  std::vector<double> in_, out_;     // audio thread scratch (NAM processes double)
  double sampleRate_ = 48000.0;
  int maxBlock_ = 512;
  std::atomic<bool> loaded_{false}, fullRig_{false};
  juce::CriticalSection infoLock_;
  Info info_;
};

}  // namespace parabola
