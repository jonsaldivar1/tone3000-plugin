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

  // --snapshot <png> [--look chrome|redfoil] [--settings]
  juce::String snapshotPath, look = "chrome";
  bool settings = false;
  for (int i = 1; i < argc; ++i) {
    const juce::String arg(argv[i]);
    if (arg == "--snapshot" && i + 1 < argc) snapshotPath = argv[++i];
    else if (arg == "--look" && i + 1 < argc) look = argv[++i];
    else if (arg == "--settings") settings = true;
  }
  if (snapshotPath.isNotEmpty()) {
    DjontProcessor p;
    p.prepareToPlay(kFs, kBlock);
    for (const auto& [id, v] : Params{{"gritOn", 1}, {"subOn", 1}, {"whamOn", 1}})
      p.params.getParameter(id)->setValueNotifyingHost(v);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    if (auto* e = dynamic_cast<djont::DjontEditor*>(editor.get())) {
      e->setLook(look);
      e->showSettings(settings);
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
