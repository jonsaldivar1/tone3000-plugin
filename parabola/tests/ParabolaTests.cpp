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
#include "Presets.h"

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

// Share of the output's energy (1.6 - 12 kHz) that sits on the note's
// harmonics rather than between them (hiss): 1.0 = all note.
std::vector<float> lastOut;
double harmonicShare(double hz) {
  constexpr int order = 15, size = 1 << order;
  if (static_cast<int>(lastOut.size()) < size) return 0.0;
  std::vector<float> data(2 * size, 0.0f);
  for (int i = 0; i < size; ++i) {
    const float w = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * static_cast<float>(i) / size);
    data[static_cast<size_t>(i)] = lastOut[lastOut.size() - size + static_cast<size_t>(i)] * w;
  }
  juce::dsp::FFT fft(order);
  fft.performFrequencyOnlyForwardTransform(data.data());
  const double binHz = 48000.0 / size;
  double on = 0.0, all = 0.0;
  for (int b = static_cast<int>(1600.0 / binHz); b < static_cast<int>(12000.0 / binHz); ++b) {
    const double f = b * binHz, pw = static_cast<double>(data[static_cast<size_t>(b)]) * data[static_cast<size_t>(b)];
    all += pw;
    const double k = std::round(f / hz);
    if (std::abs(f - k * hz) < 4.0) on += pw;
  }
  return all > 0.0 ? on / all : 0.0;
}

double toneSlope = 1.5;  // harmonic n at 1/n^slope
// Like sineRms, with a bass-like tone: harmonics at 1/n^slope up to 6 kHz.
double toneRms(ParabolaProcessor& p, double fs, double hz, double amp, double seconds = 1.0, int block = 256,
               double noiseRms = 0.0) {
  juce::Random rng(1234);
  const int total = static_cast<int>(fs * seconds);
  juce::AudioBuffer<float> buf(2, block);
  juce::MidiBuffer midi;
  double sum = 0.0;
  int count = 0;
  const int harmonics = static_cast<int>(6000.0 / hz);
  for (int start = 0; start < total; start += block) {
    for (int i = 0; i < block; ++i) {
      double x = 0.0;
      for (int k = 1; k <= harmonics; ++k)
        x += std::sin(juce::MathConstants<double>::twoPi * hz * k * (start + i) / fs) / std::pow(k, toneSlope);
      const double v = amp * x * 0.5 + noiseRms * 1.732 * (2.0 * rng.nextDouble() - 1.0);
      buf.setSample(0, i, static_cast<float>(v));
      buf.setSample(1, i, static_cast<float>(v));
    }
    p.processBlock(buf, midi);
    if (start >= total / 2)
      for (int i = 0; i < block; ++i) {
        sum += buf.getSample(0, i) * buf.getSample(0, i);
        lastOut.push_back(buf.getSample(0, i));
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
  juce::String overlay;
  for (int i = 1; i < argc; ++i) {
    const juce::String arg(argv[i]);
    if (arg == "--nam-dir" && i + 1 < argc) namDir = juce::File(juce::String(argv[++i]));
    else if (arg == "--ir" && i + 1 < argc) irFile = juce::File(juce::String(argv[++i]));
    else if (arg == "--snapshot" && i + 1 < argc) snapshotPath = argv[++i];
    else if (arg == "--look" && i + 1 < argc) look = argv[++i];
    else if (arg == "--settings") settings = true;
    else if (arg == "--adv") adv = true;
    else if (arg == "--manual" && i + 1 < argc) manual = juce::String(argv[++i]).getIntValue();
    else if (arg == "--overlay" && i + 1 < argc) overlay = argv[++i];
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
      // The drive bands' gate shuts mid / high under its threshold; the low band rings on.
      ParabolaProcessor p;
      p.prepareToPlay(48000.0, 256);
      flatBands(p);
      set(p, "driveGate", -40.0f);
      check(sineRms(p, 48000.0, 2000.0, 0.003) < 1e-4, "Noise gate shuts a quiet 2 kHz signal in the drive bands");
      check(sineRms(p, 48000.0, 60.0, 0.003) > 0.0015, "and leaves the low band alone");
      check(sineRms(p, 48000.0, 2000.0, 0.3) > 0.15, "and opens for a played note");
    }
    {
      // HUSH: learns the noise, then cuts it; off is a bypass; floors save.
      ParabolaProcessor p;
      p.prepareToPlay(48000.0, 256);
      flatBands(p);
      juce::Random rng(7);
      const auto noiseRms = [&](double seconds) {
        juce::AudioBuffer<float> buf(2, 256);
        juce::MidiBuffer midi;
        double sum = 0.0;
        int count = 0;
        const int total = static_cast<int>(48000.0 * seconds);
        for (int start = 0; start < total; start += 256) {
          for (int i = 0; i < 256; ++i) {
            const float x = 0.0005f * 1.732f * (2.0f * rng.nextFloat() - 1.0f);  // -66 dBFS
            buf.setSample(0, i, x);
            buf.setSample(1, i, x);
          }
          p.processBlock(buf, midi);
          if (start >= total / 2)
            for (int i = 0; i < 256; ++i) {
              sum += buf.getSample(0, i) * buf.getSample(0, i);
              ++count;
            }
        }
        return std::sqrt(sum / juce::jmax(1, count));
      };
      const double before = noiseRms(1.0);
      p.hush.startLearn();
      noiseRms(2.5);
      check(p.hush.learnedCount.load() == 1, "Hush LEARN finishes after about two seconds");
      set(p, "hushOn", 1.0f);
      set(p, "hushStrength", 100.0f);
      const double after = noiseRms(1.0);
      std::printf("   hush on hiss alone: %.1f dB -> %.1f dB\n", db(before), db(after));
      check(db(after / before) < -12.0, "Hush turns hiss alone down by 12+ dB");
      check(std::abs(db(sineRms(p, 48000.0, 110.0, 0.3) / 0.2121)) < 0.5, "and leaves a played note at its level");
      juce::MemoryBlock state;
      p.getStateInformation(state);
      ParabolaProcessor q;
      q.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
      check(std::abs(q.hush.floorDb[3].load() - p.hush.floorDb[3].load()) < 0.2f && q.hush.floorDb[3].load() > -77.0f,
            "the learned noise floors save with the state");
    }
    {
      // RACK: split drive leaves the lows clean and drives the highs; sub comp
      // squeezes only the lows; DI shaper boosts its band; SOLO is the rack
      // alone; the reported latency doesn't move.
      ParabolaProcessor p;
      p.prepareToPlay(48000.0, 256);
      flatBands(p);
      // tone at `hz`; returns {rms, level of harmonic `k` relative to the fundamental in dB}
      const auto probe = [&](double hz, double amp, int k) {
        const int total = 48000, block = 256;
        juce::AudioBuffer<float> buf(2, block);
        juce::MidiBuffer midi;
        std::vector<float> out;
        for (int start = 0; start < total; start += block) {
          for (int i = 0; i < block; ++i) {
            const float x = static_cast<float>(amp * std::sin(juce::MathConstants<double>::twoPi * hz * (start + i) / 48000.0));
            buf.setSample(0, i, x);
            buf.setSample(1, i, x);
          }
          p.processBlock(buf, midi);
          if (start >= total / 2)
            for (int i = 0; i < block; ++i) out.push_back(buf.getSample(0, i));
        }
        const auto goertzel = [&](double f) {
          double re = 0, im = 0;
          for (size_t i = 0; i < out.size(); ++i) {
            const double w = juce::MathConstants<double>::twoPi * f * static_cast<double>(i) / 48000.0;
            re += out[i] * std::cos(w);
            im -= out[i] * std::sin(w);
          }
          return std::sqrt(re * re + im * im);
        };
        double sum = 0;
        for (float v : out) sum += static_cast<double>(v) * v;
        return std::pair<double, double>{std::sqrt(sum / static_cast<double>(out.size())), db(goertzel(hz * k) / goertzel(hz))};
      };
      const int latencyOff = p.getLatencySamples();
      const auto low0 = probe(70.0, 0.25, 3), high0 = probe(1000.0, 0.25, 3);
      set(p, "rackDriveOn", 1.0f);
      set(p, "rackDrive", 100.0f);
      set(p, "rackDriveBlend", 100.0f);
      set(p, "rackDriveTone", 8000.0f);
      const auto low1 = probe(70.0, 0.25, 3), high1 = probe(1000.0, 0.25, 3);
      std::printf("   split drive: 70 Hz %.2f -> %.2f dB, 3rd harmonic at 70 Hz %.0f dB; 1 kHz 3rd harmonic %.0f -> %.0f dB\n",
                  db(low0.first), db(low1.first), low1.second, high0.second, high1.second);
      check(std::abs(db(low1.first / low0.first)) < 1.0 && low1.second < -40.0, "split drive: a 70 Hz note stays clean");
      check(high1.second > high0.second + 30.0 && high1.second > -30.0, "split drive: a 1 kHz note above the split gets driven");
      check(p.getLatencySamples() == latencyOff, "the rack never changes the reported latency");
      set(p, "rackDriveOn", 0.0f);
      const auto lowOff = probe(70.0, 0.25, 3);
      check(std::abs(db(lowOff.first / low0.first)) < 0.3, "split drive off: back to the same level");
      set(p, "rackSubOn", 1.0f);
      set(p, "rackSubAmount", 100.0f);
      const auto subLow = probe(60.0, 0.5, 3), subHigh = probe(2000.0, 0.2, 3);
      const auto ref2k = 0.2 / std::sqrt(2.0);
      std::printf("   sub comp: 60 Hz at -6 dBFS %.1f dB, 2 kHz %.2f dB\n", db(subLow.first / (0.5 / std::sqrt(2.0))), db(subHigh.first / ref2k));
      check(db(subLow.first / (0.5 / std::sqrt(2.0))) < -6.0, "sub comp squeezes a loud low note");
      check(std::abs(db(subHigh.first / ref2k)) < 0.6, "and leaves a 2 kHz note alone");
      set(p, "rackSubOn", 0.0f);
      set(p, "rackDiOn", 1.0f);
      set(p, "rackDi3", 12.0f);  // 400 Hz
      const auto di = probe(400.0, 0.05, 3);
      std::printf("   DI shaper +12 dB at 400 Hz: %.1f dB\n", db(di.first / (0.05 / std::sqrt(2.0))));
      check(std::abs(db(di.first / (0.05 / std::sqrt(2.0))) - 12.0) < 1.5, "DI shaper: +12 dB at 400 Hz comes out about +12 dB");
      set(p, "rackDi3", 0.0f);
      set(p, "lowLevel", -36.0f);
      set(p, "midLevel", -36.0f);
      set(p, "highLevel", -36.0f);
      set(p, "rackSolo", 1.0f);
      const auto solo = probe(1000.0, 0.2, 3);
      check(std::abs(db(solo.first / (0.2 / std::sqrt(2.0)))) < 0.5, "SOLO plays the rack alone (bands muted, level unchanged)");
      check(p.rackActive(), "the RACK chip knows the rack is in use");
    }
    {
      // Pitch: +12 doubles the frequency; Parabola reports the shifter's delay.
      ParabolaProcessor p;
      set(p, "pitch", 12.0f);
      p.prepareToPlay(48000.0, 256);
      flatBands(p);
      check(p.getLatencySamples() > 0, "Pitch on reports the shifter's delay");
      const int total = 48000;
      juce::AudioBuffer<float> buf(2, 256);
      juce::MidiBuffer midi;
      int crossings = 0;
      float last = 0.0f;
      for (int start = 0; start < total; start += 256) {
        for (int i = 0; i < 256; ++i) {
          const float x = 0.3f * std::sin(juce::MathConstants<float>::twoPi * 110.0f * static_cast<float>(start + i) / 48000.0f);
          buf.setSample(0, i, x);
          buf.setSample(1, i, x);
        }
        p.processBlock(buf, midi);
        if (start >= total / 2)
          for (int i = 0; i < 256; ++i) {
            const float y = buf.getSample(0, i);
            if (last < 0.0f && y >= 0.0f) ++crossings;
            last = y;
          }
      }
      std::printf("   pitch +12 on 110 Hz: %d Hz\n", crossings * 2);
      check(std::abs(crossings * 2 - 220) < 12, "Pitch +12 shifts 110 Hz to 220 Hz");
      ParabolaProcessor q;
      q.prepareToPlay(48000.0, 256);
      check(q.getLatencySamples() == 0, "Pitch off adds no delay");
    }
    {
      // MIDI learn: the next CC maps to the parameter, then moves it.
      ParabolaProcessor p;
      p.prepareToPlay(48000.0, 256);
      juce::AudioBuffer<float> buf(2, 256);
      buf.clear();
      p.startLearn("midLevel");
      juce::MidiBuffer midi;
      midi.addEvent(juce::MidiMessage::controllerEvent(1, 20, 127), 0);
      p.processBlock(buf, midi);
      check(p.ccFor("midLevel") == 20, "MIDI learn maps CC 20 to Mid Level");
      midi.clear();
      midi.addEvent(juce::MidiMessage::controllerEvent(1, 20, 0), 0);
      p.processBlock(buf, midi);
      check(p.params.getRawParameterValue("midLevel")->load() < -35.0f, "CC 20 at 0 turns Mid Level all the way down");
      p.startLearn("highOn");
      midi.clear();
      midi.addEvent(juce::MidiMessage::controllerEvent(1, 21, 127), 0);
      p.processBlock(buf, midi);
      midi.clear();
      midi.addEvent(juce::MidiMessage::controllerEvent(1, 21, 0), 0);
      p.processBlock(buf, midi);
      check(p.params.getRawParameterValue("highOn")->load() < 0.5f, "a footswitch CC mutes the high band");
      juce::MemoryBlock state;
      p.getStateInformation(state);
      ParabolaProcessor q;
      q.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
      check(q.ccFor("midLevel") == 20 && q.ccFor("highOn") == 21, "MIDI mappings save with the state");
    }
    {
      // Tuner: hears the DI and mutes the output.
      ParabolaProcessor p;
      p.prepareToPlay(48000.0, 256);
      p.setTunerActive(true);
      const double out = sineRms(p, 48000.0, 55.0, 0.3, 1.0);
      const double hz = p.tuner.getReading()["frequency"];
      std::printf("   tuner on a 55 Hz sine: %.2f Hz\n", hz);
      check(out < 1e-4, "the output is muted while the tuner is open");
      check(std::abs(hz - 55.0) < 0.5, "the tuner reads a low A (55 Hz)");
      p.setTunerActive(false);
      check(sineRms(p, 48000.0, 55.0, 0.3, 0.5) > 0.05, "closing the tuner unmutes");
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

      {
        // A folder added in My Gear is searched too (like C:\Users\you\NAM Tones (Bass)).
        const auto extra = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("NAM Tones (Bass)");
        extra.getChildFile("Pack").createDirectory();
        djent.copyFileTo(extra.getChildFile("Pack").getChildFile("Extra Test Capture.nam"));
        parabola::Library::addRoot(extra);
        check(parabola::Library::find("Extra Test Capture.nam").existsAsFile(), "an added My Gear folder is searched");
        parabola::Library::removeRoot(extra);
        check(!parabola::Library::find("Extra Test Capture.nam").existsAsFile(), "and removing it stops that");
        extra.deleteRecursively();
      }
      // Starter presets find their captures by file name in the tones folders.
      if (!parabola::Library::roots().empty()) {
        const auto lib = parabola::Library::scan();
        std::printf("   My Gear sees %d files\n", static_cast<int>(lib.size()));
        check(!lib.empty(), "My Gear lists the tones folders' captures and IRs");
        parabola::PresetStore store;
        const auto* djentPreset = store.byName("Djent Low End");
        ParabolaProcessor r;
        r.prepareToPlay(48000.0, 256);
        const auto missing = parabola::PresetStore::apply(*djentPreset, r);
        std::printf("   Djent Low End missing: '%s'\n", missing.toRawUTF8());
        check(missing.isEmpty() && r.slot(parabola::kMid).info().name == "B7K Djent" && r.irLoaded(),
              "the Djent Low End starter preset loads its captures and IR by file name");
        check(std::abs(r.params.getRawParameterValue("lowComp")->load() - 55.0f) < 0.5f, "and its settings");
        set(r, "midDrive", 7.0f);
        store.save("Test Preset", r);
        parabola::PresetStore reloaded;
        const auto* mine = reloaded.byName("Test Preset");
        check(mine != nullptr && !mine->factory && mine->nam[1] == "B7K Djent.nam", "a saved preset comes back from disk");
        ParabolaProcessor t;
        t.prepareToPlay(48000.0, 256);
        parabola::PresetStore::apply(*mine, t);
        check(std::abs(t.params.getRawParameterValue("midDrive")->load() - 7.0f) < 0.1f && t.slot(parabola::kMid).isLoaded(),
              "and applies its settings and captures");
        check(reloaded.remove("Test Preset") && reloaded.byName("Test Preset") == nullptr, "deleting it works");
      } else {
        std::printf("SKIP  preset/My Gear tests (no tones folder)\n");
      }

      {
        // POST: the high band's capture grinds on a low note; PRE gets nothing above the split.
        ParabolaProcessor h;
        h.prepareToPlay(48000.0, 256);
        flatBands(h);
        set(h, "lowOn", 0.0f);
        set(h, "midOn", 0.0f);
        h.loadNam(parabola::kHigh, find("B7K Heavy"));
        // A played note over a passive single-coil's hiss (about -60 dBFS): how
        // much of the high band's output is the note's harmonics vs noise.
        const auto share = [&](float split) {
          set(h, "highSplit", split);
          lastOut.clear();
          toneRms(h, 48000.0, 82.0, 0.3, 1.5, 256, 0.001);
          const double sh = harmonicShare(82.0);
          std::printf("   high band, %s: %.0f%% of its output is the note's harmonics\n", split > 0.5f ? "POST" : "PRE", 100.0 * sh);
          return sh;
        };
        toneSlope = 2.5;  // a passive J bass is dark up there
        set(h, "highDrive", -6.0f);
        const double pre = share(0.0f), postShare = share(1.0f);
        check(postShare > pre, "POST (Drive -6): more of the high band is the note and less is hiss than PRE");
        set(h, "highDrive", 0.0f);
        toneSlope = 1.5;
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
      if (overlay == "mygear") e->showMyGear(true);
      if (overlay == "tuner") e->showTuner(true);
      if (overlay == "hush") e->showHush(true);
      if (overlay == "rack") e->showRack(true);
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
