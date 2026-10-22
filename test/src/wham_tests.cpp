// Fork (TK3J): wham, the pitch shifter's virtual expression pedal. The
// contracts:
//   - the LFO shapes start at the heel and reach the toe,
//   - RETRIG restarts the sweep on a pick, and Rise+RETRIG holds the toe,
//   - in the processor, wham off leaves the Pitch knob as the shift, and
//     PEDAL at the toe shifts by the knob (an octave up from 220 Hz here)
//     while PEDAL at the heel plays the heel (0 st: the dry pitch).
#include "Processor.h"
#include "Wham.h"
#include "chain_test_helpers.h"

#include <gtest/gtest.h>

#include <cmath>
#include <utility>
#include <vector>

namespace {

constexpr int kBlock = 512;

juce::AudioBuffer<float> silence(int n) {
  juce::AudioBuffer<float> b(2, n);
  b.clear();
  return b;
}

std::vector<float> sine(double hz, double seconds) {
  std::vector<float> v(static_cast<size_t>(seconds * kFs));
  for (size_t i = 0; i < v.size(); ++i)
    v[i] = 0.3f * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * hz * static_cast<double>(i) / kFs));
  return v;
}

// Power at one frequency over the last half (Goertzel).
double power(const std::vector<float>& x, double hz) {
  const double w = juce::MathConstants<double>::twoPi * hz / kFs, c = 2 * std::cos(w);
  double s1 = 0, s2 = 0;
  for (size_t i = x.size() / 2; i < x.size(); ++i) {
    const double s = x[i] + c * s1 - s2;
    s2 = s1;
    s1 = s;
  }
  const double n = static_cast<double>(x.size() / 2);
  return (s1 * s1 + s2 * s2 - c * s1 * s2) / (n * n);
}

using Params = std::vector<std::pair<const char*, float>>;

std::vector<float> runPitch(const Params& params) {
  auto proc = std::make_unique<ChainTestProcessor>();
  proc->setPlayConfigDetails(2, 2, kFs, kBlock);
  proc->prepareToPlay(kFs, kBlock);
  juce::ValueTree state("ChainSnapshot");
  state.appendChild(juce::ValueTree("ChainBlocks"), nullptr);
  proc->restoreFromTree(state);
  EXPECT_TRUE(waitForChainLoaded(*proc));
  for (const auto& [id, value] : params) {
    auto* p = proc->parameters.getParameter(id);
    EXPECT_NE(p, nullptr) << id;
    if (p != nullptr) p->setValueNotifyingHost(p->convertTo0to1(value));
  }
  proc->prepareToPlay(kFs, kBlock);
  const auto in = sine(220.0, 2.0);
  return processStereoLR(*proc, in, in).first;
}

}  // namespace

TEST(WhamTest, ShapesStartAtTheHeelAndReachTheToe) {
  using S = t3k::Wham::Shape;
  for (auto shape : {S::sine, S::triangle, S::rise}) {
    EXPECT_NEAR(t3k::Wham::shapeAt(shape, 0.0), 0.0f, 1e-6f);
    float peak = 0.0f;
    for (int i = 0; i < 100; ++i) peak = std::max(peak, t3k::Wham::shapeAt(shape, i / 100.0));
    EXPECT_GT(peak, 0.97f);
  }
  EXPECT_FLOAT_EQ(t3k::Wham::shapeAt(S::square, 0.1), 1.0f);
  EXPECT_FLOAT_EQ(t3k::Wham::shapeAt(S::square, 0.6), 0.0f);
}

TEST(WhamTest, PedalModeFollowsThePedal) {
  t3k::Wham w;
  w.prepare(kFs);
  t3k::Wham::Settings s;
  s.mode = t3k::Wham::Mode::pedal;
  s.pedal = 0.75f;
  const auto b = silence(32);
  float p = 0;
  for (int i = 0; i < 200; ++i) p = w.advance(s, b, 0, 32);
  EXPECT_NEAR(p, 0.75f, 1e-3f);
}

TEST(WhamTest, RiseWithRetrigScoopsOncePerPickThenHolds) {
  t3k::Wham w;
  w.prepare(kFs);
  t3k::Wham::Settings s;
  s.mode = t3k::Wham::Mode::autoSweep;
  s.shape = t3k::Wham::Shape::rise;
  s.rateHz = 4.0f;  // a 250 ms scoop
  s.retrig = true;
  const auto quiet = silence(32);
  float p = 0;
  for (int i = 0; i < 1500; ++i) p = w.advance(s, quiet, 0, 32);  // 1 s: past one cycle
  EXPECT_GT(p, 0.99f) << "Rise + RETRIG holds the toe after the scoop";
  // A pick: the sweep restarts from the heel.
  juce::AudioBuffer<float> pick(2, 32);
  for (int ch = 0; ch < 2; ++ch)
    for (int i = 0; i < 32; ++i) pick.setSample(ch, i, 0.5f * std::sin(0.3f * static_cast<float>(i)));
  w.advance(s, pick, 0, 32);
  for (int i = 0; i < 24; ++i) p = w.advance(s, quiet, 0, 32);  // ~16 ms: past the glide
  EXPECT_LT(p, 0.25f) << "a pick restarts the scoop at the heel";
}

TEST(WhamTest, ProcessorPedalSweepsFromHeelToToe) {
  const Params base = {{"pitchEnabled", 1.0f}, {"pitchSemitones", 12.0f}, {"whamHeel", 0.0f}};
  Params toe = base, heel = base;
  toe.insert(toe.end(), {{"whamMode", 1.0f}, {"whamPedal", 1.0f}});
  heel.insert(heel.end(), {{"whamMode", 1.0f}, {"whamPedal", 0.0f}});
  const auto atToe = runPitch(toe);
  const auto atHeel = runPitch(heel);
  const auto knobOnly = runPitch(base);
  EXPECT_GT(power(atToe, 440.0), 10.0 * power(atToe, 220.0)) << "toe: the knob's +12 st";
  EXPECT_GT(power(atHeel, 220.0), 10.0 * power(atHeel, 440.0)) << "heel: 0 st";
  EXPECT_GT(power(knobOnly, 440.0), 10.0 * power(knobOnly, 220.0)) << "wham off: the knob is the shift";
}
