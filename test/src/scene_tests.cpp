// Fork (T3KJ): scenes and footswitches. A scene recalls every tone block's
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
  proc.setBlockFootswitch("cab", "1");
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
  EXPECT_EQ(block(restored, "cab")["tk3j"]["footswitch"].toString(), juce::String("1"));
  restored.selectScene(1);
  EXPECT_FALSE(enabled(restored, "cab"));
}

TEST(SceneTest, FootswitchFlipsItsBlocksTogether) {
  ChainTestProcessor proc;
  proc.restoreFromTree(twoBlockRig());
  ASSERT_TRUE(waitForChainLoaded(proc));
  proc.setBlockFootswitch("amp", "2");
  proc.setBlockFootswitch("cab", "2");
  proc.setBlockParam("cab", "enabled", 0.0);

  // Mixed -> all on, then all on -> all off.
  ASSERT_TRUE(proc.toggleFootswitch("2"));
  EXPECT_TRUE(enabled(proc, "amp"));
  EXPECT_TRUE(enabled(proc, "cab"));
  ASSERT_TRUE(proc.toggleFootswitch("2"));
  EXPECT_FALSE(enabled(proc, "amp"));
  EXPECT_FALSE(enabled(proc, "cab"));
  EXPECT_FALSE(proc.toggleFootswitch("3"));  // nothing assigned
  EXPECT_FALSE(proc.setBlockFootswitch("amp", "9"));
}

TEST(SceneTest, MidiPicksScenesByValueAndFiresFootswitches) {
  ChainTestProcessor proc;
  proc.restoreFromTree(twoBlockRig());
  ASSERT_TRUE(waitForChainLoaded(proc));
  proc.setBlockFootswitch("cab", "1");
  ASSERT_TRUE(proc.midiMapper.setCcMapping("tk3jScene", 43));
  ASSERT_TRUE(proc.midiMapper.setCcMapping("tk3jFootswitch1", 44));
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

TEST(SceneTest, FirstBuildFootswitchLettersLoadAsNumbers) {
  // A block saved by the first scenes build with footswitch "F" loads as 2,
  // and an old MIDI map target E..H maps onto 1..4.
  auto rig = twoBlockRig();
  rig.getChildWithName("ChainBlocks").getChild(1).setProperty("tk3jFootswitch", "F", nullptr);
  ChainTestProcessor proc;
  proc.restoreFromTree(rig);
  ASSERT_TRUE(waitForChainLoaded(proc));
  EXPECT_EQ(block(proc, "cab")["tk3j"]["footswitch"].toString(), juce::String("2"));

  juce::ValueTree map("MidiMappings");
  juce::ValueTree m("Mapping");
  m.setProperty("targetId", "tk3jFootswitchG", nullptr);
  m.setProperty("source", "cc", nullptr);
  m.setProperty("number", 50, nullptr);
  map.appendChild(m, nullptr);
  proc.midiMapper.restoreFromValueTree(map);
  pump();
  EXPECT_EQ(proc.midiMapper.getState()["mappings"][0]["targetId"].toString(), juce::String("tk3jFootswitch3"));
}

namespace {
void setParam(TONE3000Processor& proc, const char* id, float value) {
  auto* p = proc.parameters.getParameter(id);
  p->setValueNotifyingHost(p->convertTo0to1(value));
}
float getParam(TONE3000Processor& proc, const char* id) {
  auto* p = proc.parameters.getParameter(id);
  return p->convertFrom0to1(p->getValue());
}
int sceneCount(TONE3000Processor& proc) { return static_cast<int>(proc.getChainState(-1)["scenes"]["count"]); }
int activeScene(TONE3000Processor& proc) { return static_cast<int>(proc.getChainState(-1)["scenes"]["active"]); }
}  // namespace

TEST(SceneTest, FaceplateKnobsFollowTheSceneButPitchStaysPut) {
  ChainTestProcessor proc;
  proc.restoreFromTree(twoBlockRig());
  ASSERT_TRUE(waitForChainLoaded(proc));
  setParam(proc, "outputLevel", 0.3f);
  setParam(proc, "toneMid", 7.0f);
  setParam(proc, "pitchSemitones", -2.0f);

  ASSERT_TRUE(proc.selectScene(3));  // never visited: a copy of A
  EXPECT_NEAR(getParam(proc, "outputLevel"), 0.3f, 1e-4);
  setParam(proc, "outputLevel", 0.7f);
  setParam(proc, "toneMid", 2.0f);
  setParam(proc, "gateThreshold", -40.0f);
  setParam(proc, "pitchSemitones", -5.0f);

  ASSERT_TRUE(proc.selectScene(0));
  EXPECT_NEAR(getParam(proc, "outputLevel"), 0.3f, 1e-4);
  EXPECT_NEAR(getParam(proc, "toneMid"), 7.0f, 1e-3);
  EXPECT_NEAR(getParam(proc, "gateThreshold"), -80.0f, 1e-3);
  EXPECT_NEAR(getParam(proc, "pitchSemitones"), -5.0f, 1e-3);  // shared
  ASSERT_TRUE(proc.selectScene(3));
  EXPECT_NEAR(getParam(proc, "outputLevel"), 0.7f, 1e-4);
  EXPECT_NEAR(getParam(proc, "toneMid"), 2.0f, 1e-3);
  EXPECT_NEAR(getParam(proc, "gateThreshold"), -40.0f, 1e-3);

  // Saved with the state: back in A after a restore, D still has its knobs.
  proc.selectScene(0);
  juce::MemoryBlock data;
  proc.getStateInformation(data);
  TONE3000Processor restored;
  restored.setStateInformation(data.getData(), static_cast<int>(data.getSize()));
  ASSERT_TRUE(waitForChainLoaded(restored));
  EXPECT_NEAR(getParam(restored, "outputLevel"), 0.3f, 1e-4);
  ASSERT_TRUE(restored.selectScene(3));
  EXPECT_NEAR(getParam(restored, "outputLevel"), 0.7f, 1e-4);
  EXPECT_NEAR(getParam(restored, "toneMid"), 2.0f, 1e-3);
}

TEST(SceneTest, AddAndRemoveScenesUpToEight) {
  ChainTestProcessor proc;
  proc.restoreFromTree(twoBlockRig());
  ASSERT_TRUE(waitForChainLoaded(proc));
  EXPECT_EQ(sceneCount(proc), 4);
  EXPECT_FALSE(proc.selectScene(4));

  // E starts as a copy of the scene you were in, and you land in it.
  proc.selectScene(1);
  proc.setBlockParam("cab", "enabled", 0.0);
  setParam(proc, "outputLevel", 0.65f);
  ASSERT_TRUE(proc.addScene());
  EXPECT_EQ(sceneCount(proc), 5);
  EXPECT_EQ(activeScene(proc), 4);
  EXPECT_FALSE(enabled(proc, "cab"));
  EXPECT_NEAR(getParam(proc, "outputLevel"), 0.65f, 1e-4);
  while (proc.addScene()) {}
  EXPECT_EQ(sceneCount(proc), 8);

  // MIDI value 7 picks H now that there are eight.
  ASSERT_TRUE(proc.midiMapper.setCcMapping("tk3jScene", 43));
  pump();
  proc.midiMapper.processMidi(cc(43, 7));
  pump();
  EXPECT_EQ(activeScene(proc), 7);

  // Removing B moves C..H up a letter; names go with them.
  proc.renameScene(2, "Lead");
  ASSERT_TRUE(proc.removeScene(1));
  EXPECT_EQ(sceneCount(proc), 7);
  EXPECT_EQ(activeScene(proc), 6);
  EXPECT_EQ(proc.getChainState(-1)["scenes"]["names"][1].toString(), juce::String("Lead"));

  // Removing the active scene lands on the one after it (or before, at the
  // end) and recalls it.
  proc.selectScene(0);
  EXPECT_TRUE(enabled(proc, "cab"));
  ASSERT_TRUE(proc.removeScene(0));
  EXPECT_EQ(activeScene(proc), 0);  // the old C ("Lead"), never visited: kept live
  EXPECT_EQ(proc.getChainState(-1)["scenes"]["names"][0].toString(), juce::String("Lead"));
  while (sceneCount(proc) > 1) ASSERT_TRUE(proc.removeScene(sceneCount(proc) - 1));
  EXPECT_FALSE(proc.removeScene(0));  // one always stays

  // Undo brings the scene back.
  ASSERT_TRUE(proc.undoChain());
  EXPECT_EQ(sceneCount(proc), 2);
}
