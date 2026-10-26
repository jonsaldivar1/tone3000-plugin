// HushTests: offline checks for the Hush plugin (and the shared engine).
//
//   HushTests                    run the checks
//   HushTests --snapshot <png>   render the window
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>
#include <cstdio>

#include "HushEditor.h"
#include "HushProcessor.h"

namespace {
int failures = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++failures;
}
constexpr double kFs = 48000.0;
constexpr int kBlock = 256;

// Runs `seconds` of hiss (at hissDb) plus an optional 110 Hz bass-like note;
// returns the RMS of the last half.
double run(HushProcessor& p, double seconds, float hissDb, float noteAmp, juce::Random& rng) {
  juce::AudioBuffer<float> buf(2, kBlock);
  juce::MidiBuffer midi;
  const int total = static_cast<int>(kFs * seconds);
  const float hiss = std::pow(10.0f, hissDb / 20.0f) * 1.732f;
  double sum = 0.0;
  int count = 0;
  for (int start = 0; start < total; start += kBlock) {
    for (int i = 0; i < kBlock; ++i) {
      double note = 0.0;
      for (int k = 1; k < 30; ++k) note += std::sin(juce::MathConstants<double>::twoPi * 110.0 * k * (start + i) / kFs) / (k * k);
      const float x = noteAmp * static_cast<float>(note) + hiss * (2.0f * rng.nextFloat() - 1.0f);
      buf.setSample(0, i, x);
      buf.setSample(1, i, x);
    }
    p.processBlock(buf, midi);
    if (start >= total / 2)
      for (int i = 0; i < kBlock; ++i) {
        sum += buf.getSample(0, i) * buf.getSample(0, i);
        ++count;
      }
  }
  return std::sqrt(sum / juce::jmax(1, count));
}
double db(double v) { return 20.0 * std::log10(std::max(v, 1e-12)); }
}  // namespace

int main(int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI gui;
  juce::String snapshot;
  for (int i = 1; i < argc; ++i)
    if (juce::String(argv[i]) == "--snapshot" && i + 1 < argc) snapshot = argv[++i];

  if (snapshot.isEmpty()) {
    juce::Random rng(11);
    HushProcessor p;
    p.prepareToPlay(kFs, kBlock);
    check(p.params.getRawParameterValue("hushOn")->load() > 0.5f, "the plugin starts with Hush on");
    p.params.getParameter("hushOn")->setValueNotifyingHost(0.0f);
    const double off = run(p, 0.5, -66.0f, 0.0f, rng);
    p.hush.startLearn();
    run(p, 2.5, -66.0f, 0.0f, rng);
    check(p.hush.learnedCount.load() == 1, "LEARN finishes in about two seconds");
    p.params.getParameter("hushOn")->setValueNotifyingHost(1.0f);
    p.params.getParameter("hushStrength")->setValueNotifyingHost(1.0f);
    const double on = run(p, 1.0, -66.0f, 0.0f, rng);
    std::printf("   hiss alone: %.1f dB -> %.1f dB\n", db(off), db(on));
    check(db(on / off) < -20.0, "hiss alone drops 20+ dB at full Strength");
    const double note = run(p, 1.0, -120.0f, 0.15f, rng);
    p.params.getParameter("hushOn")->setValueNotifyingHost(0.0f);
    const double dry = run(p, 1.0, -120.0f, 0.15f, rng);
    std::printf("   a clean note: %.2f dB with Hush, %.2f dB without\n", db(note), db(dry));
    check(std::abs(db(note / dry)) < 0.3, "a note with no hiss comes through at its level");
    check(p.getLatencySamples() == 0, "zero latency");
    juce::MemoryBlock state;
    p.getStateInformation(state);
    HushProcessor q;
    q.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    check(std::abs(q.hush.floorDb[2].load() - p.hush.floorDb[2].load()) < 0.2f, "learned floors save with the state");
  } else {
    HushProcessor p;
    p.prepareToPlay(kFs, kBlock);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 2.0f);
    const juce::File file{snapshot};
    file.deleteFile();
    juce::FileOutputStream stream(file);
    juce::PNGImageFormat().writeImageToStream(image, stream);
    std::printf("snapshot: %s\n", file.getFullPathName().toRawUTF8());
  }
  std::printf("%s (%d failed)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures);
  return failures == 0 ? 0 : 1;
}
