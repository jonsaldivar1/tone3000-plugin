// Fork (T3KJ): split channel strip. With two lanes running and the split
// on, lane 2 (channel 1) has its own input gain, gate, tone stack and output
// level; lane 1 keeps the stock controls. The contracts:
//
//   - split on with lane 2 matching lane 1 sounds like split off (the tone
//     stack and output level move pre-matrix, which only reorders linear
//     stages),
//   - lane 2's controls change lane 2 only, and lane 1's change lane 1 only,
//   - each lane's gate is its own,
//   - the split is inert on a single-lane rig.
#include "Processor.h"
#include "chain_test_helpers.h"

#include <gtest/gtest.h>

#include <cmath>
#include <utility>
#include <vector>

namespace {

constexpr int kBlock = 512;
using InputMode = TONE3000Processor::InputMode;

const std::vector<float>& takeL() {
  static const auto v = makeNoise(240 * kBlock, 1001, 0.1f);
  return v;
}
const std::vector<float>& takeR() {
  static const auto v = makeNoise(240 * kBlock, 2002, 0.1f);
  return v;
}

juce::ValueTree monoAmpCab() {
  juce::ValueTree state("ChainSnapshot");
  juce::ValueTree lane("ChainBlocks");
  lane.appendChild(makeNamBlockTree("blk-amp", 1, 100), nullptr);
  lane.appendChild(makeIrBlockTree("blk-cab", 2, 200), nullptr);
  state.appendChild(lane, nullptr);
  return state;
}

void setParam(TONE3000Processor& proc, const char* id, float value) {
  auto* p = proc.parameters.getParameter(id);
  ASSERT_NE(p, nullptr) << id;
  p->setValueNotifyingHost(p->convertTo0to1(value));
}

using Params = std::vector<std::pair<const char*, float>>;

// Dual mono rig (two takes through the same chain: lane 1 = L, lane 2 = R).
// An empty chain: the strips' own effect, with no model in the way.
juce::ValueTree emptyChain() {
  juce::ValueTree state("ChainSnapshot");
  state.appendChild(juce::ValueTree("ChainBlocks"), nullptr);
  return state;
}

std::pair<std::vector<float>, std::vector<float>> run(const Params& params,
                                                      InputMode mode = InputMode::DualMono,
                                                      const juce::ValueTree& rig = monoAmpCab()) {
  auto proc = std::make_unique<ChainTestProcessor>();
  proc->setPlayConfigDetails(2, 2, kFs, kBlock);
  proc->prepareToPlay(kFs, kBlock);
  proc->restoreFromTree(rig);
  EXPECT_TRUE(waitForChainLoaded(*proc));
  proc->setInputMode(mode);
  EXPECT_TRUE(waitForChainLoaded(*proc));
  for (const auto& [id, value] : params) setParam(*proc, id, value);
  proc->prepareToPlay(kFs, kBlock);
  return processStereoLR(*proc, takeL(), takeR());
}

float settledDiff(const std::vector<float>& a, const std::vector<float>& b) {
  return settledMaxChannelDiff(a, b, 48000);
}

float settledRms(const std::vector<float>& x, size_t skip = 48000) {
  double sum = 0.0;
  for (size_t i = skip; i < x.size(); ++i) sum += static_cast<double>(x[i]) * x[i];
  return static_cast<float>(std::sqrt(sum / static_cast<double>(x.size() - skip)));
}

}  // namespace

TEST(SplitStripTest, MatchingLanesSoundLikeTheSharedStrip) {
  const Params lane1 = {{"inputLevel", 0.6f}, {"toneBass", 7.0f}, {"toneTreble", 3.0f}, {"outputLevel", 0.45f}};
  Params split = lane1;
  split.insert(split.end(), {{"tk3jSplitStrip", 1.0f}, {"inputLevel2", 0.6f}, {"toneBass2", 7.0f},
                             {"toneTreble2", 3.0f}, {"outputLevel2", 0.45f}});
  const auto [sl, sr] = run(lane1);
  const auto [pl, pr] = run(split);
  EXPECT_LT(settledDiff(sl, pl), 1e-4f);
  EXPECT_LT(settledDiff(sr, pr), 1e-4f);
}

TEST(SplitStripTest, Lane2ControlsOnlyMoveLane2) {
  const auto [bl, br] = run({{"tk3jSplitStrip", 1.0f}});
  const auto [l, r] = run({{"tk3jSplitStrip", 1.0f}, {"toneBass2", 9.0f}, {"outputLevel2", 0.3f}});
  EXPECT_LT(settledDiff(bl, l), 1e-6f) << "lane 1 moved with lane 2's knobs";
  EXPECT_GT(settledDiff(br, r), 1e-3f) << "lane 2's knobs did nothing";
  EXPECT_LT(settledRms(r), settledRms(br)) << "lane 2's output level should pull it down";
}

TEST(SplitStripTest, Lane1ControlsLeaveLane2AloneWhenSplit) {
  const auto [bl, br] = run({{"tk3jSplitStrip", 1.0f}});
  const auto [l, r] = run({{"tk3jSplitStrip", 1.0f}, {"toneMid", 1.0f}, {"outputLevel", 0.3f}, {"inputLevel", 0.7f}});
  EXPECT_GT(settledDiff(bl, l), 1e-3f);
  EXPECT_LT(settledDiff(br, r), 1e-6f) << "lane 2 followed lane 1's knobs while split";
}

TEST(SplitStripTest, EachLaneHasItsOwnGate) {
  // Lane 2's gate clamps shut at a 0 dB threshold; lane 1's stays open.
  const auto [bl, br] = run({{"tk3jSplitStrip", 1.0f}}, InputMode::DualMono, emptyChain());
  const auto [l, r] = run({{"tk3jSplitStrip", 1.0f}, {"gateEnabled2", 1.0f}, {"gateThreshold2", 0.0f}},
                          InputMode::DualMono, emptyChain());
  EXPECT_GT(settledRms(br), 0.01f) << "the empty chain should pass the take";
  EXPECT_LT(settledDiff(bl, l), 1e-6f);
  EXPECT_LT(settledRms(r), 0.05f * settledRms(br)) << "lane 2's gate should have closed";
}

TEST(SplitStripTest, InertOnASingleLaneRig) {
  // Stereo input summed into one mono chain: there is no lane 2.
  const auto [ol, orr] = run({}, InputMode::Stereo);
  const auto [l, r] = run({{"tk3jSplitStrip", 1.0f}, {"outputLevel2", 0.1f}}, InputMode::Stereo);
  EXPECT_EQ(settledDiff(ol, l), 0.0f);
  EXPECT_EQ(settledDiff(orr, r), 0.0f);
}
