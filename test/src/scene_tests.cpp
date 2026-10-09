// Fork (TK3J): scenes and footswitches. A scene recalls every tone block's
// power / knobs / EQ without touching the loaded models; tweaks made in a
// scene stick to it; scenes and footswitch letters survive the state round
// trip; footswitches flip their blocks together; MIDI picks scenes by value
// and fires footswitches.

#include "chain_test_helpers.h"

#include <gtest/gtest.h>

namespace {

void pump(int ms = 20) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); }

juce::MidiBuffer cc(int number, int value) {
  juce::MidiBuffer midi;
  midi.addEvent(juce::MidiMessage::controllerEvent(1, number, value), 0);
  return midi;
}

juce::ValueTree twoBlockRig() {
  juce::ValueTree snapshot("ChainSnapshot");
  juce::ValueTree left("ChainBlocks");
  left.appendChild(makeNamBlockTree("amp", 11, 111), nullptr);
  left.appendChild(makeIrBlockTree("cab", 22, 222), nullptr);
  snapshot.appendChild(left, nullptr);
  snapshot.appendChild(juce::ValueTree("RightChainBlocks"), nullptr);
  return snapshot;
}

juce::var block(TONE3000Processor& proc, const char* id) {
  const auto state = proc.getChainState(-1);
  for (const auto& item : *state["chain"].getArray())
    if (item["blockId"].toString() == id)
      return item;
  return {};
}

bool enabled(TONE3000Processor& proc, const char* id) {
  return static_cast<bool>(block(proc, id)["params"]["enabled"]);
}

double param(TONE3000Processor& proc, const char* id, const char* name) {
  return static_cast<double>(block(proc, id)["params"][name]);
}

}  // namespace

TEST(SceneTest, ScenesRecallPowerAndKnobsAndTweaksStick) {
  ChainTestProcessor proc;
  proc.restoreFromTree(twoBlockRig());
  ASSERT_TRUE(waitForChainLoaded(proc));

  // Scene A: both on, amp out at 0.5. Scene B: cab off, amp out at 0.8.
  EXPECT_EQ(static_cast<int>(proc.getChainState(-1)["scenes"]["active"]), 0);
  ASSERT_TRUE(proc.selectScene(1));
  proc.setBlockParam("cab", "enabled", 0.0);
  proc.setBlockParam("amp", "outputGain", 0.8);

  ASSERT_TRUE(proc.selectScene(0));
  EXPECT_TRUE(enabled(proc, "cab"));
  EXPECT_NEAR(param(proc, "amp", "outputGain"), 0.5, 1e-6);
  // The hover preview data: B has the cab off, A (live) has it on.
  const auto sceneOn = block(proc, "cab")["tk3j"]["sceneOn"];
  EXPECT_TRUE(static_cast<bool>(sceneOn[0]));
  EXPECT_FALSE(static_cast<bool>(sceneOn[1]));

  ASSERT_TRUE(proc.selectScene(1));
  EXPECT_FALSE(enabled(proc, "cab"));
  EXPECT_NEAR(param(proc, "amp", "outputGain"), 0.8, 1e-6);
  // Models stayed loaded: scenes never reload.
  EXPECT_TRUE(static_cast<bool>(block(proc, "amp")["loaded"]));
  EXPECT_FALSE(static_cast<bool>(block(proc, "amp")["modelLoading"]));
}

TEST(SceneTest, ScenesNamesAndFootswitchesSurviveStateRoundTrip) {
  ChainTestProcessor proc;
  proc.restoreFromTree(twoBlockRig());
  ASSERT_TRUE(waitForChainLoaded(proc));
  proc.renameScene(1, "Chorus");
  proc.setBlockFootswitch("cab", "E");
  proc.selectScene(1);
  proc.setBlockParam("cab", "enabled", 0.0);
  proc.selectScene(0);

  juce::MemoryBlock data;
  proc.getStateInformation(data);
  TONE3000Processor restored;
  restored.setStateInformation(data.getData(), static_cast<int>(data.getSize()));
  ASSERT_TRUE(waitForChainLoaded(restored));

  const auto scenes = restored.getChainState(-1)["scenes"];
  EXPECT_EQ(static_cast<int>(scenes["active"]), 0);
  EXPECT_EQ(scenes["names"][1].toString(), juce::String("Chorus"));
  EXPECT_EQ(block(restored, "cab")["tk3j"]["footswitch"].toString(), juce::String("E"));
  restored.selectScene(1);
  EXPECT_FALSE(enabled(restored, "cab"));
}

TEST(SceneTest, FootswitchFlipsItsBlocksTogether) {
  ChainTestProcessor proc;
  proc.restoreFromTree(twoBlockRig());
  ASSERT_TRUE(waitForChainLoaded(proc));
  proc.setBlockFootswitch("amp", "F");
  proc.setBlockFootswitch("cab", "F");
  proc.setBlockParam("cab", "enabled", 0.0);

  // Mixed -> all on, then all on -> all off.
  ASSERT_TRUE(proc.toggleFootswitch("F"));
  EXPECT_TRUE(enabled(proc, "amp"));
  EXPECT_TRUE(enabled(proc, "cab"));
  ASSERT_TRUE(proc.toggleFootswitch("F"));
  EXPECT_FALSE(enabled(proc, "amp"));
  EXPECT_FALSE(enabled(proc, "cab"));
  EXPECT_FALSE(proc.toggleFootswitch("G"));  // nothing assigned
  EXPECT_FALSE(proc.setBlockFootswitch("amp", "Z"));
}

TEST(SceneTest, MidiPicksScenesByValueAndFiresFootswitches) {
  ChainTestProcessor proc;
  proc.restoreFromTree(twoBlockRig());
  ASSERT_TRUE(waitForChainLoaded(proc));
  proc.setBlockFootswitch("cab", "E");
  ASSERT_TRUE(proc.midiMapper.setCcMapping("tk3jScene", 43));
  ASSERT_TRUE(proc.midiMapper.setCcMapping("tk3jFootswitchE", 44));
  pump();

  proc.midiMapper.processMidi(cc(43, 2));
  pump();
  EXPECT_EQ(static_cast<int>(proc.getChainState(-1)["scenes"]["active"]), 2);
  proc.midiMapper.processMidi(cc(43, 99));  // out of range: ignored
  pump();
  EXPECT_EQ(static_cast<int>(proc.getChainState(-1)["scenes"]["active"]), 2);

  proc.midiMapper.processMidi(cc(44, 127));
  pump();
  EXPECT_FALSE(enabled(proc, "cab"));
}
