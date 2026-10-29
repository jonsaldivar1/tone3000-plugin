// dJONt checks: transparency with everything off, finite output at extreme
// settings, and that each module does what it says (Sub adds the octave
// below, Grit adds low harmonics, Tight lifts attacks and trims decays,
// Lo-Fi strips the lows). `--snapshot <png>` also renders the editor.
#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

#include "DjontEditor.h"
#include "DjontProcessor.h"
#include "WhamPresets.h"

using djont::DjontProcessor;

namespace {

constexpr double kFs = 48000.0;
constexpr int kBlock = 512;
int failures = 0;

void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++failures;
}

using Params = std::vector<std::pair<const char*, float>>;

// Everything off, unity gains, then `params`.
std::unique_ptr<DjontProcessor> make(const Params& params) {
  auto p = std::make_unique<DjontProcessor>();
  for (const auto& [id, v] : Params{{"tightOn", 0}, {"gritOn", 0}, {"subOn", 0}, {"lofiOn", 0}}) {
    auto* param = p->params.getParameter(id);
    param->setValueNotifyingHost(param->convertTo0to1(v));
  }
  for (const auto& [id, v] : params) {
    auto* param = p->params.getParameter(id);
    if (param == nullptr) {
      std::printf("unknown param %s\n", id);
      ++failures;
      continue;
    }
    param->setValueNotifyingHost(param->convertTo0to1(v));
  }
  p->setPlayConfigDetails(2, 2, kFs, kBlock);
  p->prepareToPlay(kFs, kBlock);
  return p;
}

std::vector<float> run(DjontProcessor& p, const std::vector<float>& in) {
  std::vector<float> out(in.size());
  juce::AudioBuffer<float> buffer(2, kBlock);
  juce::MidiBuffer midi;
  for (size_t off = 0; off < in.size(); off += kBlock) {
    const int n = static_cast<int>(std::min<size_t>(kBlock, in.size() - off));
    buffer.setSize(2, n, false, false, true);
    for (int ch = 0; ch < 2; ++ch) buffer.copyFrom(ch, 0, in.data() + off, n);
    p.processBlock(buffer, midi);
    std::copy(buffer.getReadPointer(0), buffer.getReadPointer(0) + n, out.begin() + static_cast<long>(off));
  }
  return out;
}

std::vector<float> sine(double hz, double seconds, float amp) {
  std::vector<float> v(static_cast<size_t>(seconds * kFs));
  for (size_t i = 0; i < v.size(); ++i)
    v[i] = amp * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * hz * static_cast<double>(i) / kFs));
  return v;
}

// Power of one frequency over [from, end) (Goertzel).
double tone(const std::vector<float>& x, double hz, size_t from = 24000) {
  const double w = juce::MathConstants<double>::twoPi * hz / kFs, c = 2 * std::cos(w);
  double s1 = 0, s2 = 0;
  for (size_t i = from; i < x.size(); ++i) {
    const double s = x[i] + c * s1 - s2;
    s2 = s1;
    s1 = s;
  }
  const double n = static_cast<double>(x.size() - from);
  return (s1 * s1 + s2 * s2 - c * s1 * s2) / (n * n);
}

bool finite(const std::vector<float>& x, float limit) {
  for (float v : x)
    if (!std::isfinite(v) || std::abs(v) > limit) return false;
  return true;
}

// Palm-mute-ish bursts: 110 Hz with a fast decay, every 250 ms.
std::vector<float> chugs() {
  std::vector<float> v(static_cast<size_t>(2.0 * kFs));
  for (size_t i = 0; i < v.size(); ++i) {
    const double t = std::fmod(static_cast<double>(i) / kFs, 0.25);
    v[i] = static_cast<float>(0.4 * std::exp(-t * 18.0) *
                              std::sin(juce::MathConstants<double>::twoPi * 110.0 * static_cast<double>(i) / kFs));
  }
  return v;
}

// Mean of |x| over each burst's first 15 ms / its last 100 ms.
std::pair<double, double> attackAndTail(const std::vector<float>& x) {
  double a = 0, t = 0;
  int na = 0, nt = 0;
  for (size_t i = static_cast<size_t>(0.5 * kFs); i < x.size(); ++i) {
    const double ph = std::fmod(static_cast<double>(i) / kFs, 0.25);
    if (ph < 0.015) a += std::abs(x[i]), ++na;
    if (ph > 0.15) t += std::abs(x[i]), ++nt;
  }
  return {a / na, t / nt};
}

}  // namespace

int main(int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI gui;

  {
    const auto in = sine(110.0, 1.0, 0.3f);
    auto p = make({});
    const auto out = run(*p, in);
    float diff = 0;
    for (size_t i = 0; i < in.size(); ++i) diff = std::max(diff, std::abs(out[i] - in[i]));
    check(diff == 0.0f, "everything off at unity gain passes the input through untouched");
  }
  {
    auto p = make({{"tightOn", 1}, {"tightAmount", 100}, {"gritOn", 1}, {"gritDrive", 100}, {"gritBlend", 100},
                   {"subOn", 1}, {"subBlend", 100}, {"lofiOn", 1}, {"input", 24}, {"output", 0}});
    check(finite(run(*p, chugs()), 20.0f), "all modules maxed (+24 dB in) stays finite and bounded");
    auto q = make({{"subOn", 1}, {"subMode", 1}, {"subBlend", 100}, {"gritOn", 1}, {"gritDrive", 100}});
    check(finite(run(*q, std::vector<float>(48000, 0.0f)), 1e-3f), "silence in, (near) silence out");
  }
  {
    const auto in = sine(110.0, 1.5, 0.3f);
    const auto off = run(*make({}), in);
    const auto synth = run(*make({{"subOn", 1}, {"subMode", 1}, {"subBlend", 100}}), in);
    const auto clean = run(*make({{"subOn", 1}, {"subMode", 0}, {"subBlend", 100}}), in);
    check(tone(synth, 55.0) > 100.0 * (tone(off, 55.0) + 1e-12), "Sub SYNTH adds the octave below (55 Hz from 110)");
    check(tone(clean, 55.0) > 100.0 * (tone(off, 55.0) + 1e-12), "Sub CLEAN adds the octave below (55 Hz from 110)");
  }
  {
    const auto in = sine(80.0, 1.0, 0.3f);
    const auto off = run(*make({}), in);
    const auto grit = run(*make({{"gritOn", 1}, {"gritDrive", 70}, {"gritBlend", 100}}), in);
    check(tone(grit, 240.0) > 100.0 * (tone(off, 240.0) + 1e-12), "Grit adds harmonics to the lows (240 Hz from 80)");
  }
  {
    const auto in = chugs();
    const auto [a0, t0] = attackAndTail(run(*make({}), in));
    const auto [a1, t1] = attackAndTail(run(*make({{"tightOn", 1}, {"tightAmount", 100}, {"tightFreq", 110}}), in));
    check(a1 > a0 * 1.15, "Tight lifts the attack of each chug");
    check(t1 / a1 < t0 / a0, "Tight shortens the decay relative to the attack");
  }
  {
    const auto in = sine(80.0, 1.0, 0.3f);
    const auto lofi = run(*make({{"lofiOn", 1}, {"lofiTone", 1000}, {"lofiBlend", 100}}), in);
    check(tone(lofi, 80.0) < 0.01 * tone(in, 80.0), "Lo-Fi strips the lows (80 Hz down > 20 dB)");
  }
  {
    auto p = make({{"output", -12}});
    const auto in = sine(440.0, 0.5, 0.5f);
    const auto out = run(*p, in);
    double ei = 0, eo = 0;
    for (size_t i = in.size() / 2; i < in.size(); ++i) ei += in[i] * in[i], eo += out[i] * out[i];
    check(std::abs(std::sqrt(eo / ei) - 0.2512) < 0.01, "Output -12 dB scales by 0.251");
  }

  {
    // Wham: the pedal at the toe plays the Toe pitch (+12: 110 -> 220 Hz), at
    // the heel the Heel pitch (0: 110 Hz), and Wham reports its latency.
    const auto in = sine(110.0, 1.5, 0.3f);
    auto toe = make({{"whamOn", 1}, {"whamMode", 0}, {"whamPedal", 100}, {"whamHeel", 0}, {"whamToe", 12}});
    const auto up = run(*toe, in);
    check(tone(up, 220.0) > 10.0 * tone(up, 110.0), "Wham pedal at the toe shifts up by Toe (+12 st)");
    check(toe->getLatencySamples() > 0, "Wham on reports its latency");
    auto heel = make({{"whamOn", 1}, {"whamMode", 0}, {"whamPedal", 0}, {"whamHeel", 0}, {"whamToe", 12}});
    const auto down = run(*heel, in);
    check(tone(down, 110.0) > 10.0 * tone(down, 220.0), "Wham pedal at the heel plays Heel (0 st)");
    check(make({})->getLatencySamples() == 0, "Wham off adds no latency");
  }
  {
    // Whammy modes. Both engines shift; the Whammy list replaces the dry,
    // Harmony keeps it and adds the voice (and reports no latency).
    const auto in = sine(110.0, 1.5, 0.3f);
    for (float algo : {0.0f, 1.0f}) {
      auto w = make({{"whamOn", 1}, {"whamMode", 0}, {"whamPedal", 100}, {"whamInterval", 2}, {"whamAlgo", algo}});
      const auto out = run(*w, in);
      check(tone(out, 220.0) > 10.0 * tone(out, 110.0),
            algo < 0.5f ? "Classic engine: Whammy Oct Up at the toe plays the octave"
                        : "Chords engine: Whammy Oct Up at the toe plays the octave");
    }
    auto h = make({{"whamOn", 1}, {"whamMode", 0}, {"whamPedal", 0}, {"whamInterval", 19}});  // Oct Dn > Oct Up
    const auto harm = run(*h, in);
    check(tone(harm, 110.0) > 1e-4 && tone(harm, 55.0) > 1e-4, "Harmony keeps your note and adds the voice (heel: octave down)");
    check(h->getLatencySamples() == 0, "Harmony reports no latency (the dry stays on time)");
    auto c = make({{"whamOn", 1}, {"whamAlgo", 0}, {"whamGrain", 60}});
    check(c->getLatencySamples() == djont::ClassicShift::latencySamples(60.0f, kFs), "Classic reports its grain delay");
    auto d = make({{"whamOn", 1}, {"whamInterval", 21}});
    check(finite(run(*d, in), 2.0f), "Detune stays bounded");
  }
  {
    // Voicing: Hold's Low Cut thins the held lows; the Sustainer's Interval.
    const auto set = [](DjontProcessor& proc, const char* id, float v) {
      auto* param = proc.params.getParameter(id);
      param->setValueNotifyingHost(param->convertTo0to1(v));
    };
    auto hp = make({{"sustainOn", 1}, {"sustainMode", 0}, {"holdFade", 0.05f}, {"holdLevel", 100}, {"holdLowCut", 500}});
    run(*hp, sine(80.0, 0.5, 0.3f));
    set(*hp, "holdOn", 1);
    const auto thin = run(*hp, std::vector<float>(static_cast<size_t>(1.0 * kFs), 0.0f));
    auto full = make({{"sustainOn", 1}, {"sustainMode", 0}, {"holdFade", 0.05f}, {"holdLevel", 100}});
    run(*full, sine(80.0, 0.5, 0.3f));
    set(*full, "holdOn", 1);
    const auto thick = run(*full, std::vector<float>(static_cast<size_t>(1.0 * kFs), 0.0f));
    check(tone(thin, 80.0, thin.size() / 2) < 0.1 * tone(thick, 80.0, thick.size() / 2), "Hold Low Cut thins the held lows");
    std::vector<float> pluck(static_cast<size_t>(2.0 * kFs));
    for (size_t i = 0; i < pluck.size(); ++i) {
      const double t = static_cast<double>(i) / kFs;
      pluck[i] = static_cast<float>(0.3 * std::exp(-t * 3.0) * std::sin(juce::MathConstants<double>::twoPi * 110.0 * t));
    }
    const auto fifth = run(*make({{"sustainOn", 1}, {"sustainMode", 1}, {"sustainerType", 1}, {"sustainerAmount", 100},
                                  {"sustainerBloom", 0.3f}, {"sustainerInterval", 1}}),
                           pluck);
    const double f5 = 110.0 * std::pow(2.0, 7.0 / 12.0);
    check(tone(fifth, f5, static_cast<size_t>(1.2 * kFs)) > 3.0 * tone(fifth, 110.0, static_cast<size_t>(1.2 * kFs)),
          "Sustainer Interval 5th blooms into the fifth");
  }
  {
    // Wham presets: save, reload from disk, apply, delete; factory names are
    // protected.
    const auto file = juce::File::createTempFile(".json");
    auto p = make({{"whamHeel", -3}, {"whamToe", 7}, {"whamRate", 2.5f}, {"whamShape", 3}, {"whamRetrig", 1}});
    {
      djont::WhamPresets store(file);
      check(store.save("My Trill", p->params), "a preset saves");
      check(!store.save("Octave Scoop", p->params), "a factory preset can't be overwritten");
    }
    djont::WhamPresets reloaded(file);
    const auto* mine = reloaded.find("my trill");
    check(mine != nullptr && !mine->factory, "a saved preset is there after a reload");
    auto q = make({});
    if (mine != nullptr) djont::WhamPresets::apply(*mine, q->params);
    const auto v = [&](const char* id) { return q->params.getRawParameterValue(id)->load(); };
    check(std::abs(v("whamHeel") + 3) < 0.01f && std::abs(v("whamToe") - 7) < 0.01f &&
              std::abs(v("whamRate") - 2.5f) < 0.02f && std::abs(v("whamShape") - 3) < 0.01f && v("whamRetrig") > 0.5f,
          "loading a preset sets Heel, Toe, Rate, Shape and Retrig");
    check(reloaded.matching(q->params) == "My Trill", "the loaded preset is recognised as current");
    check(reloaded.remove("My Trill") && djont::WhamPresets(file).find("My Trill") == nullptr, "a preset deletes");
    file.deleteFile();
  }
  {
    // Sustain / Hold: Chord keeps a picked note sounding after the input
    // stops, and lets it go on release; Pile-up keeps adding notes.
    const auto set = [](DjontProcessor& proc, const char* id, float v) {
      auto* param = proc.params.getParameter(id);
      param->setValueNotifyingHost(param->convertTo0to1(v));
    };
    const auto rms = [](const std::vector<float>& x, size_t from) {
      double e = 0;
      for (size_t i = from; i < x.size(); ++i) e += static_cast<double>(x[i]) * x[i];
      return std::sqrt(e / static_cast<double>(x.size() - from));
    };
    auto p = make({{"sustainOn", 1}, {"sustainMode", 0}, {"holdType", 0}, {"holdFade", 0.05f}, {"holdLevel", 100}});
    run(*p, sine(220.0, 0.5, 0.3f));
    set(*p, "holdOn", 1);
    const auto held = run(*p, std::vector<float>(static_cast<size_t>(1.5 * kFs), 0.0f));
    const double heldRms = rms(held, held.size() / 2);
    std::printf("      hold rms %.3f (input sine rms 0.212)\n", heldRms);
    check(heldRms > 0.08 && heldRms < 0.5, "Hold (Chord) keeps the note sounding after you stop playing");
    check(tone(held, 220.0, held.size() / 2) > 50.0 * tone(held, 330.0, held.size() / 2), "Hold keeps the pitch (220 Hz)");
    set(*p, "holdOn", 0);
    const auto released = run(*p, std::vector<float>(static_cast<size_t>(0.5 * kFs), 0.0f));
    check(rms(released, released.size() / 2) < 0.002, "letting go of Hold fades it out");

    auto q = make({{"sustainOn", 1}, {"sustainMode", 0}, {"holdType", 1}, {"holdFade", 0.05f}, {"holdLevel", 100}});
    run(*q, sine(220.0, 0.5, 0.3f));
    set(*q, "holdOn", 1);
    run(*q, sine(330.0, 0.5, 0.3f));
    const auto piled = run(*q, std::vector<float>(static_cast<size_t>(1.0 * kFs), 0.0f));
    check(tone(piled, 220.0, piled.size() / 2) > 1e-5 && tone(piled, 330.0, piled.size() / 2) > 1e-5,
          "Hold (Pile-up) keeps both the first note and one played while held");
  }
  {
    // Sustainer: a plucked note that decays keeps much more of its level.
    std::vector<float> pluck(static_cast<size_t>(2.0 * kFs));
    for (size_t i = 0; i < pluck.size(); ++i) {
      const double t = static_cast<double>(i) / kFs;
      pluck[i] = static_cast<float>(0.3 * std::exp(-t * 3.0) * std::sin(juce::MathConstants<double>::twoPi * 110.0 * t));
    }
    const auto tail = [](const std::vector<float>& x) {
      double e = 0;
      for (size_t i = static_cast<size_t>(1.5 * kFs); i < x.size(); ++i) e += static_cast<double>(x[i]) * x[i];
      return std::sqrt(e);
    };
    const auto dry = run(*make({}), pluck);
    const auto fund = run(*make({{"sustainOn", 1}, {"sustainMode", 1}, {"sustainerType", 0}, {"sustainerAmount", 100}}), pluck);
    check(tail(fund) > 5.0 * tail(dry), "Sustainer holds a decaying note up (Fundamental)");
    const auto harm = run(*make({{"sustainOn", 1}, {"sustainMode", 1}, {"sustainerType", 1}, {"sustainerAmount", 100},
                                 {"sustainerBloom", 0.3f}}),
                          pluck);
    check(tone(harm, 220.0, static_cast<size_t>(1.2 * kFs)) > 3.0 * tone(harm, 110.0, static_cast<size_t>(1.2 * kFs)),
          "Sustainer Harmonic blooms into the octave");
    check(finite(harm, 4.0f) && finite(fund, 4.0f), "Sustainer stays bounded");
  }
  {
    // Wham Glide: a footswitch jump of the pedal becomes a timed sweep.
    djont::Wham w;
    w.prepare(kFs);
    djont::Wham::Settings st;
    st.mode = djont::Wham::Mode::pedal;
    st.pedal = 1.0f;
    st.glideMs = 500.0f;
    std::vector<float> quiet(32, 0.0f);
    float pos = 0;
    for (int i = 0; i < static_cast<int>(0.25 * kFs / 32); ++i) pos = w.advance(st, quiet.data(), 32);
    check(pos > 0.4f && pos < 0.6f, "Wham Glide 500 ms: halfway after 250 ms");
  }
  {
    // MIDI learn: the next CC maps; HOLD with LATCH flips on each press.
    auto p = make({{"holdLatch", 1}});
    const auto send = [&](int cc, int v) {
      juce::MidiBuffer m;
      m.addEvent(juce::MidiMessage::controllerEvent(1, cc, v), 0);
      juce::AudioBuffer<float> b(2, 64);
      b.clear();
      p->processBlock(b, m);
    };
    const auto hold = [&] { return p->params.getRawParameterValue("holdOn")->load() > 0.5f; };
    p->startLearn("holdOn");
    send(20, 127);
    send(20, 0);
    check(p->ccFor("holdOn") == 20 && p->learning().isEmpty(), "MIDI learn maps the next CC");
    check(!hold(), "the press that teaches doesn't also act");
    send(20, 127);
    send(20, 0);
    check(hold(), "LATCH: a momentary footswitch press holds (and its release doesn't let go)");
    send(20, 127);
    send(20, 0);
    check(!hold(), "LATCH: the next press lets go");
    auto* latch = p->params.getParameter("holdLatch");
    latch->setValueNotifyingHost(0.0f);
    send(20, 127);
    check(hold(), "no LATCH: holds while pressed");
    send(20, 0);
    check(!hold(), "no LATCH: lets go on release");
    p->startLearn("whamPedal");
    send(11, 0);
    send(11, 64);
    check(std::abs(p->params.getRawParameterValue("whamPedal")->load() - 50.4f) < 0.5f, "a CC moves a knob");
    juce::MemoryBlock state;
    p->getStateInformation(state);
    auto r = make({});
    r->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    check(r->ccFor("holdOn") == 20 && r->ccFor("whamPedal") == 11, "MIDI mappings save with the state");
  }
  {
    // The editor: the look round-trips through the state, and the hint bar
    // starts on its default line.
    DjontProcessor p;
    p.prepareToPlay(kFs, kBlock);
    {
      std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
      auto* e = dynamic_cast<djont::DjontEditor*>(editor.get());
      check(e != nullptr && e->currentHint().startsWith("Hover"), "editor opens with the default hint line");
      if (e != nullptr) e->setLook("redfoil");
    }
    juce::MemoryBlock state;
    p.getStateInformation(state);
    DjontProcessor q;
    q.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    check(q.params.state.getProperty("look").toString() == "redfoil", "the look is saved with the plugin state");
  }

  {
    // HUSH on the DI: learns the hiss, then turns it down; floors save.
    DjontProcessor p;
    p.prepareToPlay(kFs, kBlock);
    juce::Random rng(3);
    const auto hissRms = [&](int blocks) {
      juce::AudioBuffer<float> buf(2, kBlock);
      juce::MidiBuffer midi;
      double sum = 0.0;
      int count = 0;
      for (int k = 0; k < blocks; ++k) {
        for (int i = 0; i < kBlock; ++i) {
          const float x = 0.0005f * 1.732f * (2.0f * rng.nextFloat() - 1.0f);
          buf.setSample(0, i, x);
          buf.setSample(1, i, x);
        }
        p.processBlock(buf, midi);
        if (k >= blocks / 2)
          for (int i = 0; i < kBlock; ++i) {
            sum += buf.getSample(0, i) * buf.getSample(0, i);
            ++count;
          }
      }
      return std::sqrt(sum / juce::jmax(1, count));
    };
    for (const auto& [id, v] : Params{{"tightOn", 0}}) p.params.getParameter(id)->setValueNotifyingHost(v);
    const int second = static_cast<int>(kFs / kBlock);
    const double before = hissRms(second);
    p.hush.startLearn();
    hissRms(second * 5 / 2);
    p.params.getParameter("hushOn")->setValueNotifyingHost(1.0f);
    p.params.getParameter("hushStrength")->setValueNotifyingHost(1.0f);
    const double after = hissRms(second);
    std::printf("   hush: hiss %.1f dB -> %.1f dB\n", 20.0 * std::log10(before), 20.0 * std::log10(after));
    check(p.hush.learnedCount.load() == 1 && 20.0 * std::log10(after / before) < -12.0, "Hush learns the hiss and cuts it 12+ dB");
    juce::MemoryBlock state;
    p.getStateInformation(state);
    DjontProcessor q;
    q.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    check(std::abs(q.hush.floorDb[4].load() - p.hush.floorDb[4].load()) < 0.2f, "Hush's learned floors save with the state");
  }
  {
    // Tight FOCUS: a tight boost. Cuts the lows under Freq, pushes the mids,
    // clips a little when Amount is up.
    const auto lowIn = sine(82.0, 1.0, 0.2f), midIn = sine(720.0, 1.0, 0.05f);
    const Params focus{{"tightOn", 1}, {"tightMode", 1}, {"tightFreq", 250}, {"tightAmount", 0}};
    const double low = tone(run(*make(focus), lowIn), 82.0) / tone(lowIn, 82.0);
    const double mid = tone(run(*make(focus), midIn), 720.0) / tone(midIn, 720.0);
    std::printf("   focus: 82 Hz %.1f dB, 720 Hz %+.1f dB\n", 10.0 * std::log10(low), 10.0 * std::log10(mid));
    check(10.0 * std::log10(low) < -12.0, "FOCUS cuts the low E's fundamental (Freq 250 Hz)");
    check(10.0 * std::log10(mid) > 2.0, "and pushes the mids");
    Params hot = focus;
    hot.push_back({"tightAmount", 100});
    const auto hotOut = run(*make(hot), sine(720.0, 1.0, 0.2f));
    const double h3 = 10.0 * std::log10(tone(hotOut, 2160.0) / tone(hotOut, 720.0));
    std::printf("   focus at full Amount: 3rd harmonic %.0f dB\n", h3);
    check(h3 > -30.0 && finite(hotOut, 1.0f), "full Amount clips (harmonics), and stays bounded");
    const auto punch = run(*make({{"tightOn", 1}, {"tightMode", 0}}), chugs());
    const auto punch2 = run(*make({{"tightOn", 1}}), chugs());
    float diff = 0;
    for (size_t i = 0; i < punch.size(); ++i) diff = std::max(diff, std::abs(punch[i] - punch2[i]));
    check(diff == 0.0f, "PUNCH is still the default and unchanged");
  }
  {
    // Global Blend: 0% is the dry DI, lined up with Wham's delay; 100% is the board.
    const auto in = sine(220.0, 1.0, 0.2f);
    auto dryOnly = make({{"lofiOn", 1}, {"lofiBlend", 100}, {"gritOn", 1}, {"gritBlend", 100}, {"blend", 0}});
    const auto out = run(*dryOnly, in);
    float diff = 0;
    for (size_t i = 4800; i < in.size(); ++i) diff = std::max(diff, std::abs(out[i] - in[i]));
    check(diff < 1.0e-5f, "Blend 0% passes the dry DI untouched, whatever the modules do");
    auto whammed = make({{"whamOn", 1}, {"blend", 0}});
    const auto wout = run(*whammed, in);
    const int lat = whammed->getLatencySamples();
    float wdiff = 0;
    for (size_t i = 9600; i < in.size(); ++i) wdiff = std::max(wdiff, std::abs(wout[i] - in[i - static_cast<size_t>(lat)]));
    std::printf("   blend with Wham on: latency %d samples, max error %.2g\n", lat, wdiff);
    check(lat > 0 && wdiff < 1.0e-5f, "with Wham on, the dry is delayed to line up with the board");
    const auto full = run(*make({{"lofiOn", 1}, {"lofiTone", 1000}, {"lofiBlend", 100}}), in);
    const auto half = run(*make({{"lofiOn", 1}, {"lofiTone", 1000}, {"lofiBlend", 100}, {"blend", 50}}), in);
    float mdiff = 0;
    for (size_t i = 4800; i < in.size(); ++i) mdiff = std::max(mdiff, std::abs(half[i] - 0.5f * (full[i] + in[i])));
    check(mdiff < 1.0e-4f, "Blend 50% is halfway between the dry DI and the board");
  }
  {
    // Scenes: everything but HOLD and the Wham pedal follows the scene.
    const auto pump = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil(60); };
    auto p = make({});
    const auto set = [&](const char* id, float v) {
      auto* param = p->params.getParameter(id);
      param->setValueNotifyingHost(param->convertTo0to1(v));
    };
    const auto get = [&](const char* id) {
      auto* param = p->params.getParameter(id);
      return param->convertFrom0to1(param->getValue());
    };
    bool covers = true;
    for (auto* param : p->getParameters())
      if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(param)) {
        const auto id = r->getParameterID();
        const bool skip = id == "scene" || id == "holdOn" || id == "whamPedal";
        if (skip == DjontProcessor::sceneParamIds().contains(id)) {
          std::printf("  scene list: %s\n", id.toRawUTF8());
          covers = false;
        }
      }
    check(covers, "scenes keep every setting except the scene pick, HOLD and the Wham pedal");

    set("lofiOn", 1);
    set("output", 3);
    set("whamPedal", 40);
    check(p->selectScene(2) && p->activeScene() == 2, "switching to scene C");
    check(get("lofiOn") > 0.5f && std::abs(get("output") - 3.0f) < 0.05f, "a scene never visited starts as a copy");
    set("lofiOn", 0);
    set("sustainOn", 1);
    set("output", -6);
    set("hushOn", 1);
    set("whamPedal", 80);
    p->selectScene(0);
    check(get("lofiOn") > 0.5f && get("sustainOn") < 0.5f && std::abs(get("output") - 3.0f) < 0.05f && get("hushOn") < 0.5f,
          "scene A recalls its own modules, Output and Hush");
    check(std::abs(get("whamPedal") - 80.0f) < 0.5f, "the Wham pedal stays where your foot left it");
    p->selectScene(2);
    check(get("lofiOn") < 0.5f && get("sustainOn") > 0.5f && std::abs(get("output") + 6.0f) < 0.05f,
          "scene C recalls its tweaks");

    check(p->addScene() && p->sceneCount() == 5 && p->activeScene() == 4, "+ adds scene E and switches to it");
    while (p->addScene()) {}
    check(p->sceneCount() == 8, "up to eight scenes");
    p->renameScene(2, "Chorus");
    p->selectScene(7);
    check(p->removeScene(1) && p->sceneCount() == 7 && p->sceneName(1) == "Chorus" && p->activeScene() == 6,
          "deleting B moves the later scenes up a letter");

    // MIDI: learn CC 30 on the scene switch, then values pick scenes.
    p->startLearn("scene");
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf(2, 64);
    buf.clear();
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 30, 127), 0);
    p->processBlock(buf, midi);
    check(midi.getNumEvents() == 1, "MIDI passes through dJONt to whatever comes next");
    midi.clear();
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 30, 1), 0);
    p->processBlock(buf, midi);
    pump();
    check(p->activeScene() == 1 && get("lofiOn") < 0.5f && get("sustainOn") > 0.5f, "MIDI CC value 1 picks scene B (the old C)");
    // Host automation of the Scene parameter.
    set("scene", 0);
    pump();
    check(p->activeScene() == 0 && get("lofiOn") > 0.5f, "automating the Scene parameter switches scenes");
    while (p->sceneCount() > 3) p->removeScene(p->sceneCount() - 1);
    set("scene", 6);
    pump();
    check(p->activeScene() == 0 && juce::roundToInt(get("scene")) == 0, "a scene past the last one is ignored");

    // Saved with the plugin state.
    p->selectScene(1);
    juce::MemoryBlock state;
    p->getStateInformation(state);
    DjontProcessor q;
    q.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    pump();
    const auto qget = [&](const char* id) {
      auto* param = q.params.getParameter(id);
      return param->convertFrom0to1(param->getValue());
    };
    check(q.sceneCount() == 3 && q.activeScene() == 1 && q.sceneName(1) == "Chorus" && qget("sustainOn") > 0.5f,
          "scenes survive the DAW project / standalone state");
    q.selectScene(0);
    check(qget("lofiOn") > 0.5f && qget("sustainOn") < 0.5f, "and each restored scene recalls its settings");
  }
  {
    // Presets: yours first (PC 0, 1, ...), then starters; save, edit, rename,
    // reorder, delete; Program Change loads by list position.
    const auto pump = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil(60); };
    const auto file = djont::PresetStore::defaultFile();
    const bool had = file.existsAsFile();
    const auto backup = file.loadFileAsString();
    file.deleteFile();
    {
      auto p = make({});
      const auto set = [&](const char* id, float v) {
        auto* param = p->params.getParameter(id);
        param->setValueNotifyingHost(param->convertTo0to1(v));
      };
      const auto get = [&](const char* id) {
        auto* param = p->params.getParameter(id);
        return param->convertFrom0to1(param->getValue());
      };
      set("gritOn", 1);
      p->selectScene(1);
      set("gritOn", 0);
      set("lofiOn", 1);
      p->renameScene(1, "Bridge");
      check(p->savePreset("My Song") && p->presets().indexOf("My Song") == 0, "save a preset: yours come first (PC 0)");
      set("lofiOn", 0);
      p->selectScene(0);
      set("output", -3);
      check(p->savePreset("My Song"), "SAVE again edits it in place");
      check(p->savePreset("Other") && p->presets().indexOf("Other") == 1, "a second one is PC 1");
      check(!p->savePreset("Starter: Mine"), "starter names are reserved");
      set("gritOn", 0);
      set("output", 0);
      check(p->loadPreset("My Song") && get("gritOn") > 0.5f && std::abs(get("output") + 3.0f) < 0.05f,
            "loading brings back the settings");
      p->selectScene(1);
      check(get("lofiOn") < 0.5f && p->sceneName(1) == "Bridge", "and its scenes, as last saved");
      check(p->presets().move("Other", -1) && p->presets().indexOf("Other") == 0, "move up changes its PC number");
      check(p->renamePreset("Other", "Intro") && p->currentPreset() == "My Song", "rename");
      // Program Change: VST3 (host program) and raw MIDI.
      const int songs = p->presets().indexOf("Starter: Song Sections");
      p->setCurrentProgram(songs);
      check(p->currentPreset() == "Starter: Song Sections" && p->sceneCount() == 4 && p->sceneName(2) == "Lo-Fi Intro",
            "a host program change loads that preset, scenes and all");
      p->selectScene(2);
      check(get("lofiOn") > 0.5f && get("tightOn") < 0.5f, "the starter's Lo-Fi Intro scene");
      juce::MidiBuffer midi;
      juce::AudioBuffer<float> buf(2, 64);
      buf.clear();
      midi.addEvent(juce::MidiMessage::programChange(1, p->presets().indexOf("My Song")), 0);
      p->processBlock(buf, midi);
      pump();
      check(p->currentPreset() == "My Song", "a MIDI Program Change loads the preset at that number");
      check(p->deletePreset("Intro") && p->presets().indexOf("Intro") < 0, "delete one of yours");
      check(!p->deletePreset("Starter: Tight Boost"), "starters can't be deleted");
    }
    file.deleteFile();
    if (had) file.replaceWithText(backup);
  }
  {
    // The manual: every topic parses, with the parts the panel shows.
    const auto sections = djont::ManualPanel::load();
    check(sections.size() >= 13, "the manual has all its topics");
    bool complete = true;
    for (const auto& s : sections)
      complete = complete && s.title.isNotEmpty() && s.what.isNotEmpty() && s.how.isNotEmpty() && !s.tries.isEmpty();
    check(complete, "every manual topic has a title, what it does, how it works and things to try");
  }

  // --snapshot <png> [--look chrome|redfoil] [--settings] [--manual n]
  juce::String snapshotPath, look = "chrome";
  bool settings = false, adv = false;
  int width = 0, manual = -1;
  bool hushPanel = false;
  juce::StringArray sets;  // --set id=value (real units), any number
  juce::StringArray sceneNames;  // --scenes "Verse,Chorus,..." (adds scenes as needed)
  for (int i = 1; i < argc; ++i) {
    const juce::String arg(argv[i]);
    if (arg == "--snapshot" && i + 1 < argc) snapshotPath = argv[++i];
    else if (arg == "--look" && i + 1 < argc) look = argv[++i];
    else if (arg == "--settings") settings = true;
    else if (arg == "--adv") adv = true;
    else if (arg == "--manual" && i + 1 < argc) manual = juce::String(argv[++i]).getIntValue();
    else if (arg == "--hush") hushPanel = true;
    else if (arg == "--width" && i + 1 < argc) width = juce::String(argv[++i]).getIntValue();
    else if (arg == "--set" && i + 1 < argc) sets.add(argv[++i]);
    else if (arg == "--scenes" && i + 1 < argc) sceneNames = juce::StringArray::fromTokens(argv[++i], ",", "");
  }
  if (snapshotPath.isNotEmpty()) {
    DjontProcessor p;
    p.prepareToPlay(kFs, kBlock);
    for (const auto& [id, v] : Params{{"gritOn", 1}, {"subOn", 1}, {"whamOn", 1}, {"sustainOn", 1}})
      p.params.getParameter(id)->setValueNotifyingHost(v);
    for (const auto& kv : sets)
      if (auto* param = p.params.getParameter(kv.upToFirstOccurrenceOf("=", false, false)))
        param->setValueNotifyingHost(param->convertTo0to1(kv.fromFirstOccurrenceOf("=", false, false).getFloatValue()));
    for (int i = 0; i < sceneNames.size(); ++i) {
      while (p.sceneCount() <= i) p.addScene();
      p.renameScene(i, sceneNames[i]);
    }
    if (!sceneNames.isEmpty()) p.selectScene(0);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    if (auto* e = dynamic_cast<djont::DjontEditor*>(editor.get())) {
      e->setLook(look);
      e->showSettings(settings);
      e->showAdvanced(adv);
      if (manual >= 0) e->showManual(true, manual);
      if (hushPanel) e->showHush(true);
      if (width > 0) e->setSize(width, width * djont::DjontView::kHeight / djont::DjontView::kWidth);
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
