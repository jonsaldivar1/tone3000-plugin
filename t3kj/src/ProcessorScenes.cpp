// Fork (T3KJ): scenes and footswitches.
//
// A scene is a per-preset snapshot of every tone block's power, In / Out /
// Mix and EQ (never the loaded model: switching captures reloads, which is a
// preset change's job), plus the faceplate controls (Input, Gate, the 3-band
// EQ, Spread, Output; not Pitch). Block scenes are stored on the blocks
// themselves (ChainBlock::scenes), so they ride presets, undo, DAW state and
// copy / paste with no id bookkeeping; the faceplate values and names ride
// the scene set (serializeSceneSet). The live block fields and parameters
// ARE the active scene; selectScene first folds them back into the scene
// being left, so whatever the player tweaks in a scene is what that scene
// recalls, with no explicit save step. A preset has 1-8 scenes (A-H).
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
  for (int i = 0; i < ChainBlock::kMaxScenes; ++i) {
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
    if (i < 0 || i >= ChainBlock::kMaxScenes)
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

const std::vector<juce::String>& TONE3000Processor::sceneParameterIds() {
  static const std::vector<juce::String> ids = [] {
    std::vector<juce::String> out;
    for (const auto& id : presetParameterIds())
      if (!id.startsWith("pitch") && !id.startsWith("align") && !id.startsWith("chainPan") &&
          !id.startsWith("chainInvert") && id != "tk3jSplitStrip")
        out.push_back(id);
    return out;
  }();
  return ids;
}

TONE3000Processor::SceneParams TONE3000Processor::captureSceneParams() const {
  SceneParams out;
  out.stored = true;
  for (const auto& id : sceneParameterIds())
    if (auto* p = parameters.getParameter(id))
      out.values.emplace_back(id, p->convertFrom0to1(p->getValue()));
  return out;
}

// Message thread, outside chainMutex (parameter listeners run inline).
void TONE3000Processor::applySceneParams(const SceneParams& params) {
  if (!params.stored)
    return;
  for (const auto& [id, value] : params.values)
    if (auto* p = parameters.getParameter(id)) {
      const float norm = p->convertTo0to1(value);
      if (std::abs(norm - p->getValue()) > 1.0e-6f)
        p->setValueNotifyingHost(norm);
    }
}

juce::ValueTree TONE3000Processor::serializeSceneSet() const {
  juce::ValueTree tree("TK3JSceneSet");
  tree.setProperty("active", activeScene, nullptr);
  tree.setProperty("count", sceneCount, nullptr);
  for (int i = 0; i < sceneCount; ++i) {
    if (sceneNames[static_cast<size_t>(i)].isNotEmpty())
      tree.setProperty("name" + juce::String(i), sceneNames[static_cast<size_t>(i)], nullptr);
    const auto& params = sceneParams[static_cast<size_t>(i)];
    if (!params.stored)
      continue;
    juce::ValueTree scene("SceneParams");
    scene.setProperty("index", i, nullptr);
    for (const auto& [id, value] : params.values) {
      juce::ValueTree param("Param");
      param.setProperty("id", id, nullptr);
      param.setProperty("value", static_cast<double>(value), nullptr);
      scene.appendChild(param, nullptr);
    }
    tree.appendChild(scene, nullptr);
  }
  return tree;
}

void TONE3000Processor::applySceneSet(const juce::ValueTree& snapshot) {
  // Snapshots without scenes (stock TONE3000 presets, older states) start
  // at scene A of four, default names, every scene sharing the faceplate.
  const auto tree = snapshot.getChildWithName("TK3JSceneSet");
  sceneCount = juce::jlimit(1, ChainBlock::kMaxScenes, static_cast<int>(tree.getProperty("count", 4)));
  activeScene = juce::jlimit(0, sceneCount - 1, static_cast<int>(tree.getProperty("active", 0)));
  for (int i = 0; i < ChainBlock::kMaxScenes; ++i) {
    sceneNames[static_cast<size_t>(i)] = i < sceneCount ? tree.getProperty("name" + juce::String(i)).toString() : juce::String();
    sceneParams[static_cast<size_t>(i)] = {};
  }
  for (const auto& scene : tree) {
    if (!scene.hasType("SceneParams"))
      continue;
    const int i = scene.getProperty("index", -1);
    if (i < 0 || i >= sceneCount)
      continue;
    auto& params = sceneParams[static_cast<size_t>(i)];
    params.stored = true;
    for (const auto& param : scene)
      params.values.emplace_back(param.getProperty("id").toString(), static_cast<float>(static_cast<double>(param.getProperty("value"))));
  }
  // Restores run under the chain lock: bring the Scene parameter along later.
  sceneParamStale.store(true);
  triggerAsyncUpdate();
}

int TONE3000Processor::getSceneCount() const {
  juce::ScopedLock lock(chainMutex);
  return sceneCount;
}

int TONE3000Processor::getActiveScene() const {
  juce::ScopedLock lock(chainMutex);
  return activeScene;
}

// Message thread, outside chainMutex: the "scene" host parameter follows the
// active scene (its listener sees the same index and does nothing).
void TONE3000Processor::syncSceneParameter() {
  auto* p = parameters.getParameter("scene");
  if (p == nullptr)
    return;
  const float norm = p->convertTo0to1(static_cast<float>(getActiveScene()));
  if (std::abs(norm - p->getValue()) > 1.0e-6f)
    p->setValueNotifyingHost(norm);
}

bool TONE3000Processor::selectScene(int index) {
  SceneParams next;
  {
    juce::ScopedLock lock(chainMutex);
    if (index < 0 || index >= sceneCount)
      return false;
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
    sceneParams[from] = captureSceneParams();
    next = sceneParams[to];
    activeScene = index;
    bumpChainRevision();
  }
  applySceneParams(next);
  syncSceneParameter();
  juce::Logger::writeToLog("[Scenes] Scene " + juce::String::charToString(static_cast<juce::juce_wchar>('A' + index)));
  return true;
}

bool TONE3000Processor::addScene() {
  int added = -1;
  {
    juce::ScopedLock lock(chainMutex);
    if (sceneCount >= ChainBlock::kMaxScenes)
      return false;
    pushChainHistory();
    added = sceneCount++;
    const auto slot = static_cast<size_t>(added);
    for (auto& l : lanes)
      for (auto& block : l) block->scenes[slot] = {};
    sceneNames[slot] = {};
    sceneParams[slot] = {};
    bumpChainRevision();
  }
  // Never visited, so entering it keeps everything as it is: a copy of the
  // scene you were in, ready to change.
  return selectScene(added);
}

bool TONE3000Processor::removeScene(int index) {
  SceneParams next;
  {
    juce::ScopedLock lock(chainMutex);
    if (index < 0 || index >= sceneCount || sceneCount <= 1)
      return false;
    pushChainHistory();
    const int last = sceneCount - 1;
    if (index == activeScene) {
      // The live state is the scene being removed: land on the one after
      // it (or before, when it was the last) and recall that scene.
      const auto target = static_cast<size_t>(index < last ? index + 1 : index - 1);
      for (auto& l : lanes)
        for (auto& block : l)
          if (block->type != ChainBlockType::INSERT && block->scenes[target].stored)
            applyScene(*block, block->scenes[target]);
      next = sceneParams[target];
      activeScene = index < last ? index : index - 1;
    } else if (index < activeScene) {
      --activeScene;
    }
    for (int i = index; i < last; ++i) {
      const auto a = static_cast<size_t>(i), b = static_cast<size_t>(i + 1);
      for (auto& l : lanes)
        for (auto& block : l) block->scenes[a] = block->scenes[b];
      sceneNames[a] = sceneNames[b];
      sceneParams[a] = sceneParams[b];
    }
    const auto tail = static_cast<size_t>(last);
    for (auto& l : lanes)
      for (auto& block : l) block->scenes[tail] = {};
    sceneNames[tail] = {};
    sceneParams[tail] = {};
    sceneCount = last;
    bumpChainRevision();
  }
  applySceneParams(next);
  syncSceneParameter();
  return true;
}

bool TONE3000Processor::renameScene(int index, const juce::String& name) {
  juce::ScopedLock lock(chainMutex);
  if (index < 0 || index >= sceneCount)
    return false;
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
