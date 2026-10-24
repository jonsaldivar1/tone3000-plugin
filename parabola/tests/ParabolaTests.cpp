// ParabolaTests: offline checks for Parabola Bass Z's DSP, state and UI.
//
//   ParabolaTests [--nam-dir <dir>] [--ir <wav>]          run the checks
//   ParabolaTests --snapshot <png> [--look id] [--settings] [--manual n] [--adv]
//                 [--nam low|mid|high=<file>]... [--ir <wav>] [--width N]
//
// With --nam-dir (a folder holding real .nam captures; none ship with the
// repo, their licenses don't allow it) the capture tests run on real files.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

#include "ParabolaEditor.h"
#include "ParabolaProcessor.h"

using parabola::ParabolaProcessor;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++failures;
}

void set(ParabolaProcessor& p, const juce::String& id, float value) {
  auto* param = p.params.getParameter(id);
  param->setValueNotifyingHost(param->convertTo0to1(value));
}

// Runs `seconds` of a sine through the processor (stereo in/out), returns
// the output RMS over the last half.
double sineRms(ParabolaProcessor& p, double fs, double hz, double amp, double seconds = 0.5, int block = 256) {
  const int total = static_cast<int>(fs * seconds);
  juce::AudioBuffer<float> buf(2, block);
  juce::MidiBuffer midi;
  double sum = 0.0;
  int count = 0;
  for (int start = 0; start < total; start += block) {
    for (int i = 0; i < block; ++i) {
      const float x = static_cast<float>(amp * std::sin(juce::MathConstants<double>::twoPi * hz * (start + i) / fs));
      buf.setSample(0, i, x);
      buf.setSample(1, i, x);
    }
    p.processBlock(buf, midi);
    if (start >= total / 2)
      for (int i = 0; i < block; ++i) {
        sum += buf.getSample(0, i) * buf.getSample(0, i);
        ++count;
      }
  }
  return std::sqrt(sum / juce::jmax(1, count));
}

double db(double ratio) { return 20.0 * std::log10(std::max(ratio, 1e-12)); }

void flatBands(ParabolaProcessor& p) {
  for (auto* b : {"low", "mid", "high"}) set(p, juce::String(b) + "Level", 0.0f);
  set(p, "lowComp", 0.0f);
  set(p, "midTone", 20000.0f);
  set(p, "highTone", 20000.0f);
}
}  // namespace

int main(int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI gui;
  juce::File namDir, irFile;
  juce::String snapshotPath, look = "foil";
  bool settings = false, adv = false;
  int manual = -1, width = 0;
  juce::StringArray namArgs;
  for (int i = 1; i < argc; ++i) {
    const juce::String arg(argv[i]);
    if (arg == "--nam-dir" && i + 1 < argc) namDir = juce::File(juce::String(argv[++i]));
    else if (arg == "--ir" && i + 1 < argc) irFile = juce::File(juce::String(argv[++i]));
    else if (arg == "--snapshot" && i + 1 < argc) snapshotPath = argv[++i];
    else if (arg == "--look" && i + 1 < argc) look = argv[++i];
    else if (arg == "--settings") settings = true;
    else if (arg == "--adv") adv = true;
    else if (arg == "--manual" && i + 1 < argc) manual = juce::String(argv[++i]).getIntValue();
    else if (arg == "--width" && i + 1 < argc) width = juce::String(argv[++i]).getIntValue();
    else if (arg == "--nam" && i + 1 < argc) namArgs.add(argv[++i]);
  }

  if (snapshotPath.isEmpty()) {
    {
      // With nothing loaded and every band at 0 dB, the three bands sum back
      // to the input at every frequency (the crossover is phase-coherent).
      ParabolaProcessor p;
      p.prepareToPlay(48000.0, 256);
      flatBands(p);
      bool flat = true;
      for (double hz : {40.0, 100.0, 180.0, 400.0, 1000.0, 1600.0, 3000.0, 8000.0}) {
        const double out = sineRms(p, 48000.0, hz, 0.25);
        const double err = db(out / (0.25 / std::sqrt(2.0)));
        if (std::abs(err) > 0.25) {
          flat = false;
          std::printf("   %.0f Hz: %.2f dB\n", hz, err);
        }
      }
      check(flat, "empty bands at 0 dB sum back flat (within 0.25 dB, 40 Hz to 8 kHz)");
    }
    {
      ParabolaProcessor p;
      p.prepareToPlay(48000.0, 256);
      flatBands(p);
      set(p, "midOn", 0.0f);
      set(p, "highOn", 0.0f);
      const double low = sineRms(p, 48000.0, 60.0, 0.25), high = sineRms(p, 48000.0, 4000.0, 0.25);
      check(db(low / 0.1768) > -0.5 && db(high / 0.1768) < -30.0, "low band alone passes 60 Hz and blocks 4 kHz");
      set(p, "xoverLow", 400.0f);
      check(db(sineRms(p, 48000.0, 250.0, 0.25) / 0.1768) > -1.5, "moving the low split up lets 250 Hz into the low band");
    }
    {
      ParabolaProcessor p;
      p.prepareToPlay(48000.0, 256);
      flatBands(p);
      set(p, "gate", -40.0f);
      check(sineRms(p, 48000.0, 100.0, 0.001) < 1e-4, "the gate silences a signal under its threshold");
      check(db(sineRms(p, 48000.0, 100.0, 0.25) / 0.1768) > -0.5, "the gate passes a signal over its threshold");
    }
    {
      ParabolaProcessor p;
      p.prepareToPlay(48000.0, 256);
      flatBands(p);
      set(p, "geq0", 12.0f);
      check(db(sineRms(p, 48000.0, 80.0, 0.1) / 0.0707) > 10.0, "graphic EQ 80 Hz boosts 80 Hz");
      set(p, "geq0", 0.0f);
      set(p, "lowBass", -12.0f);
      check(db(sineRms(p, 48000.0, 60.0, 0.1) / 0.0707) < -6.0, "the low band's Bass cuts the lows");
    }
    {
      ParabolaProcessor p;
      p.prepareToPlay(48000.0, 256);
      flatBands(p);
      set(p, "lowComp", 100.0f);
      const double loud = sineRms(p, 48000.0, 60.0, 0.5), quiet = sineRms(p, 48000.0, 60.0, 0.05);
      check(db(loud / quiet) < 14.0, "low Comp squeezes a 20 dB swing to under 14 dB");
      check(p.compReductionDb.load() >= 0.0f, "comp reports gain reduction");
    }
    {
      // A non-48k host: the chain runs behind the resampler and reports it.
      ParabolaProcessor p;
      p.prepareToPlay(44100.0, 512);
      flatBands(p);
      const int latency = p.getLatencySamples();
      std::printf("   44.1 kHz latency: %d samples (%.2f ms)\n", latency, 1000.0 * latency / 44100.0);
      check(latency > 0 && latency < 441, "at 44.1 kHz the resampler's delay is reported (under 10 ms)");
      check(std::abs(db(sineRms(p, 44100.0, 200.0, 0.25) / 0.1768)) < 0.5, "at 44.1 kHz a 200 Hz sine comes through at level");
      ParabolaProcessor q;
      q.prepareToPlay(48000.0, 512);
      check(q.getLatencySamples() == 0, "at 48 kHz there's no added latency");
    }
    {
      const auto sections = parabola::ManualPanel::load();
      check(sections.size() >= 9, "the manual has all its topics");
      bool complete = true;
      for (const auto& s : sections) complete = complete && s.title.isNotEmpty() && s.what.isNotEmpty() && !s.tries.isEmpty();
      check(complete, "every manual topic has a title, what it does and things to try");
    }

    // Real captures (optional).
    if (namDir.isDirectory()) {
      const auto find = [&](const juce::String& name) {
        for (const auto& f : namDir.findChildFiles(juce::File::findFiles, true, "*.nam"))
          if (f.getFileName().containsIgnoreCase(name)) return f;
        return juce::File();
      };
      const auto djent = find("B7K Djent"), svt = find("SVT CLEAN.nam"), omega = find("OMEGA");
      ParabolaProcessor p;
      p.prepareToPlay(48000.0, 256);
      flatBands(p);
      check(p.loadNam(parabola::kMid, djent).isEmpty() && p.slot(parabola::kMid).isLoaded(), "loads an A2 capture (B7K Djent)");
      const auto info = p.slot(parabola::kMid).info();
      check(info.name == "B7K Djent" && info.gearType == "pedal" && !info.fullRig, "reads its name and gear type");
      check(p.loadNam(parabola::kLow, svt).isEmpty(), "loads the SVT-CL clean capture into low");
      if (omega.existsAsFile()) {
        p.loadNam(parabola::kHigh, omega);
        check(p.slot(parabola::kHigh).isFullRig(), "flags the Alpha Omega capture as a full rig (has a cab)");
      }
      const double out = sineRms(p, 48000.0, 110.0, 0.3, 1.0);
      std::printf("   three captures on a 110 Hz sine: %.1f dBFS RMS\n", db(out));
      check(std::isfinite(out) && out > 1e-3, "captures produce sound");
      check(p.loadNam(parabola::kMid, juce::File("/nonexistent.nam")).isNotEmpty(), "a missing file reports an error");

      // CPU: three captures in real time.
      const auto t0 = juce::Time::getMillisecondCounterHiRes();
      sineRms(p, 48000.0, 110.0, 0.3, 2.0, 128);
      const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
      std::printf("   2 s of audio with three captures took %.0f ms (%.0f%% of real time)\n", ms, ms / 20.0);

      if (irFile.existsAsFile()) {
        check(p.loadIr(irFile).isEmpty() && p.irLoaded(), "loads a cab IR");
        juce::Thread::sleep(300);  // the convolver swaps the IR in on a background thread
        check(p.bandUsesCab(parabola::kMid) && !p.bandUsesCab(parabola::kLow), "mid goes through the cab, low doesn't");
        if (omega.existsAsFile()) check(!p.bandUsesCab(parabola::kHigh), "a full-rig capture skips the cab");
        set(p, "cabOn", 1.0f);
        const double withCab = sineRms(p, 48000.0, 220.0, 0.3, 1.0);
        set(p, "cabOn", 0.0f);
        const double without = sineRms(p, 48000.0, 220.0, 0.3, 1.0);
        std::printf("   cab on vs off at 220 Hz: %.1f dB\n", db(withCab / without));
        check(std::isfinite(withCab) && withCab > 1e-4, "the cab path produces sound");
      }

      // State: files come back.
      juce::MemoryBlock state;
      p.getStateInformation(state);
      ParabolaProcessor q;
      q.prepareToPlay(48000.0, 256);
      q.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
      check(q.slot(parabola::kMid).isLoaded() && q.slot(parabola::kMid).info().name == "B7K Djent",
            "a saved state reloads each band's capture");
      if (irFile.existsAsFile()) check(q.irLoaded(), "a saved state reloads the IR");
    } else {
      std::printf("SKIP  capture tests (no --nam-dir)\n");
    }
  } else {
    ParabolaProcessor p;
    p.prepareToPlay(48000.0, 256);
    for (const auto& kv : namArgs) {
      const auto band = kv.upToFirstOccurrenceOf("=", false, false);
      const int b = band == "low" ? 0 : band == "mid" ? 1 : 2;
      p.loadNam(b, juce::File(kv.fromFirstOccurrenceOf("=", false, false)));
    }
    if (irFile.existsAsFile()) p.loadIr(irFile);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    if (auto* e = dynamic_cast<parabola::ParabolaEditor*>(editor.get())) {
      e->setLook(look);
      e->showSettings(settings);
      e->showAdvanced(adv);
      if (manual >= 0) e->showManual(true, manual);
      if (width > 0) e->setSize(width, width * parabola::ParabolaView::kHeight / parabola::ParabolaView::kWidth);
    }
    // Let the view's timer pick up the loaded files.
    for (int i = 0; i < 3; ++i) {
      juce::Thread::sleep(40);
      juce::Timer::callPendingTimersSynchronously();
    }
    const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 2.0f);
    const juce::File file{snapshotPath};
    file.deleteFile();
    juce::FileOutputStream stream(file);
    juce::PNGImageFormat().writeImageToStream(image, stream);
    std::printf("snapshot: %s\n", file.getFullPathName().toRawUTF8());
  }

  std::printf("%s (%d failed)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures);
  return failures == 0 ? 0 : 1;
}
