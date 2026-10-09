// Fork (TK3J): scenes and footswitches.
//
// A scene is a per-preset snapshot of every tone block's power, In / Out /
// Mix and EQ (never the loaded model: switching captures reloads, which is a
// preset change's job). Scenes are stored on the blocks themselves
// (ChainBlock::scenes), so they ride presets, undo, DAW state and copy /
// paste with no id bookkeeping. The live block fields ARE the active scene;
// selectScene first folds them back into the scene being left, so whatever
// the player tweaks in a scene is what that scene recalls, with no explicit
// save step.
//
// Footswitches 1-8 group blocks: toggling one flips all of its blocks
// together (all on -> all off, else all on) as one undoable step; power
// changes glide through each block's wet fade like a power click.
#include "Processor.h"

namespace {

ChainBlock::SceneState captureScene(const ChainBlock& block) {
  ChainBlock::SceneState s;
  s.stored = true;
  s.enabled = block.enabled;
  s.inputGain = block.inputGainNormalized;
  s.outputGain = block.outputGainNormalized;
  s.mix = block.mixNormalized;
  s.eq = block.eq.toValueTree();
  return s;
}

// Message thread, under chainMutex. Power changes glide through the block's
// wet fade and the gains / mix through their smoothers on the audio thread;
// the EQ is only rebuilt when it actually differs.
void applyScene(ChainBlock& block, const ChainBlock::SceneState& s) {
  block.enabled = s.enabled;
  block.inputGainNormalized = juce::jlimit(0.0f, 1.0f, s.inputGain);
  block.outputGainNormalized = juce::jlimit(0.0f, 1.0f, s.outputGain);
  block.mixNormalized = juce::jlimit(0.0f, 1.0f, s.mix);
  if (s.eq.isValid() && !s.eq.isEquivalentTo(block.eq.toValueTree()))
    block.eq.restoreFromValueTree(s.eq);
}

// Footswitches are "1".."8". The first build named them E-H; those load as 1-4.
bool isFootswitchLetter(const juce::String& letter) {
  return letter.length() == 1 && letter[0] >= '1' && letter[0] <= '8';
}

juce::String migrateFootswitch(const juce::String& letter) {
  if (letter.length() == 1 && letter[0] >= 'E' && letter[0] <= 'H')
    return juce::String(letter[0] - 'E' + 1);
  return letter;
}

}  // namespace

juce::ValueTree TONE3000Processor::serializeBlockScenes(const ChainBlock& block) {
  bool any = false;
  for (const auto& s : block.scenes) any = any || s.stored;
  if (!any)
    return {};
  juce::ValueTree tree("TK3JScenes");
  for (int i = 0; i < ChainBlock::kNumScenes; ++i) {
    const auto& s = block.scenes[static_cast<size_t>(i)];
    if (!s.stored)
      continue;
    juce::ValueTree scene("Scene");
    scene.setProperty("index", i, nullptr);
    scene.setProperty("enabled", s.enabled, nullptr);
    scene.setProperty("inputGain", s.inputGain, nullptr);
    scene.setProperty("outputGain", s.outputGain, nullptr);
    scene.setProperty("mix", s.mix, nullptr);
    if (s.eq.isValid())
      scene.appendChild(s.eq.createCopy(), nullptr);
    tree.appendChild(scene, nullptr);
  }
  return tree;
}

void TONE3000Processor::applyBlockScenes(ChainBlock& block, const juce::ValueTree& blockState) {
  block.scenes = {};
  const auto tree = blockState.getChildWithName("TK3JScenes");
  for (const auto& scene : tree) {
    const int i = scene.getProperty("index", -1);
    if (i < 0 || i >= ChainBlock::kNumScenes)
      continue;
    auto& s = block.scenes[static_cast<size_t>(i)];
    s.stored = true;
    s.enabled = static_cast<bool>(scene.getProperty("enabled", true));
    s.inputGain = static_cast<float>(scene.getProperty("inputGain", 0.5f));
    s.outputGain = static_cast<float>(scene.getProperty("outputGain", 0.5f));
    s.mix = static_cast<float>(scene.getProperty("mix", 1.0f));
    const auto eq = scene.getChildWithName("Eq");
    s.eq = eq.isValid() ? eq.createCopy() : juce::ValueTree();
  }
  const auto letter = migrateFootswitch(blockState.getProperty("tk3jFootswitch").toString());
  block.footswitch = isFootswitchLetter(letter) ? letter : juce::String();
}

juce::ValueTree TONE3000Processor::serializeSceneSet() const {
  juce::ValueTree tree("TK3JSceneSet");
  tree.setProperty("active", activeScene, nullptr);
  for (int i = 0; i < ChainBlock::kNumScenes; ++i)
    if (sceneNames[static_cast<size_t>(i)].isNotEmpty())
      tree.setProperty("name" + juce::String(i), sceneNames[static_cast<size_t>(i)], nullptr);
  return tree;
}

void TONE3000Processor::applySceneSet(const juce::ValueTree& snapshot) {
  // Snapshots without scenes (stock TONE3000 presets, older states) start
  // at scene A with default names.
  const auto tree = snapshot.getChildWithName("TK3JSceneSet");
  activeScene = juce::jlimit(0, ChainBlock::kNumScenes - 1, static_cast<int>(tree.getProperty("active", 0)));
  for (int i = 0; i < ChainBlock::kNumScenes; ++i)
    sceneNames[static_cast<size_t>(i)] = tree.getProperty("name" + juce::String(i)).toString();
}

bool TONE3000Processor::selectScene(int index) {
  if (index < 0 || index >= ChainBlock::kNumScenes)
    return false;
  juce::ScopedLock lock(chainMutex);
  if (index == activeScene)
    return true;
  const auto from = static_cast<size_t>(activeScene);
  const auto to = static_cast<size_t>(index);
  for (auto& l : lanes) {
    for (auto& block : l) {
      if (block->type == ChainBlockType::INSERT)
        continue;
      block->scenes[from] = captureScene(*block);
      if (block->scenes[to].stored)
        applyScene(*block, block->scenes[to]);
      else
        block->scenes[to] = captureScene(*block);
    }
  }
  activeScene = index;
  bumpChainRevision();
  juce::Logger::writeToLog("[Scenes] Scene " + juce::String::charToString(static_cast<juce::juce_wchar>('A' + index)));
  return true;
}

bool TONE3000Processor::renameScene(int index, const juce::String& name) {
  if (index < 0 || index >= ChainBlock::kNumScenes)
    return false;
  juce::ScopedLock lock(chainMutex);
  const auto trimmed = name.trim().substring(0, 40);
  if (sceneNames[static_cast<size_t>(index)] == trimmed)
    return true;
  pushChainHistory();
  sceneNames[static_cast<size_t>(index)] = trimmed;
  bumpChainRevision();
  return true;
}

bool TONE3000Processor::setBlockFootswitch(const std::string& blockId, const juce::String& letter) {
  if (letter.isNotEmpty() && !isFootswitchLetter(letter))
    return false;
  juce::ScopedLock lock(chainMutex);
  ChainBlock* block = findBlockById(blockId);
  if (block == nullptr || block->type == ChainBlockType::INSERT)
    return false;
  if (block->footswitch == letter)
    return true;
  pushChainHistory();
  block->footswitch = letter;
  bumpChainRevision();
  return true;
}

bool TONE3000Processor::toggleFootswitch(const juce::String& letter) {
  if (!isFootswitchLetter(letter))
    return false;
  juce::ScopedLock lock(chainMutex);
  std::vector<ChainBlock*> members;
  bool allOn = true;
  for (auto& l : lanes)
    for (auto& block : l)
      if (block->type != ChainBlockType::INSERT && block->footswitch == letter) {
        members.push_back(block.get());
        allOn = allOn && block->enabled;
      }
  if (members.empty())
    return false;
  // One undo step for the whole switch, like one stomp.
  pushChainHistory();
  for (auto* block : members) block->enabled = !allOn;
  bumpChainRevision();
  return true;
}
