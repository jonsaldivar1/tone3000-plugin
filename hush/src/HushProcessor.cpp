#include "HushProcessor.h"

#include "HushEditor.h"

namespace {
juce::AudioProcessorValueTreeState::ParameterLayout layout() {
  juce::AudioProcessorValueTreeState::ParameterLayout l;
  hushui::addParameters(l);
  return l;
}

float peakDb(const float* x, int n) {
  float p = 0.0f;
  for (int i = 0; i < n; ++i) p = std::max(p, std::abs(x[i]));
  return 20.0f * std::log10(std::max(p, 1e-6f));
}
}  // namespace

HushProcessor::HushProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "Hush", layout()) {
  hushParams_.attach(params);
  // In its own plugin Hush is the whole point: start it on.
  params.getParameter("hushOn")->setValueNotifyingHost(1.0f);
}

bool HushProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  const auto out = layouts.getMainOutputChannelSet(), in = layouts.getMainInputChannelSet();
  return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) &&
         (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void HushProcessor::prepareToPlay(double sampleRate, int) { hush.prepare(sampleRate); }

void HushProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
  juce::ScopedNoDenormals noDenormals;
  const int n = buffer.getNumSamples();
  if (getTotalNumInputChannels() == 0 || n == 0) return;
  float* x = buffer.getWritePointer(0);
  inputPeakDb.store(peakDb(x, n));
  hushParams_.apply(hush);
  hush.process(x, n);
  for (int c = 1; c < getTotalNumOutputChannels(); ++c) buffer.copyFrom(c, 0, x, n);
  outputPeakDb.store(peakDb(x, n));
}

juce::AudioProcessorEditor* HushProcessor::createEditor() { return new HushEditor(*this); }

void HushProcessor::getStateInformation(juce::MemoryBlock& dest) {
  hushui::saveFloors(hush, params.state);
  if (auto xml = params.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void HushProcessor::setStateInformation(const void* data, int size) {
  if (auto xml = getXmlFromBinary(data, size))
    if (xml->hasTagName(params.state.getType())) {
      params.replaceState(juce::ValueTree::fromXml(*xml));
      hushui::loadFloors(hush, params.state);
    }
}

#if !defined(HUSH_NO_PLUGIN_ENTRY)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HushProcessor(); }
#endif
