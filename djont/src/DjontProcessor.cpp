#include "DjontProcessor.h"

#include <algorithm>
#include <cmath>

#include "DjontEditor.h"

namespace djont {

namespace {

std::unique_ptr<juce::AudioParameterFloat> percent(const char* id, const char* name, float def) {
  return std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{id, 1}, name, juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), def,
      juce::AudioParameterFloatAttributes().withLabel("%"));
}

std::unique_ptr<juce::AudioParameterFloat> hertz(const char* id, const char* name, float lo, float hi, float def) {
  juce::NormalisableRange<float> range(lo, hi);
  range.setSkewForCentre(std::sqrt(lo * hi));
  return std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{id, 1}, name, range, def,
      juce::AudioParameterFloatAttributes().withLabel("Hz").withStringFromValueFunction(
          [](float v, int) { return juce::String(juce::roundToInt(v)); }));
}

std::unique_ptr<juce::AudioParameterFloat> decibels(const char* id, const char* name) {
  return std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{id, 1}, name, juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f,
      juce::AudioParameterFloatAttributes().withLabel("dB"));
}

std::unique_ptr<juce::AudioParameterBool> power(const char* id, const char* name, bool def) {
  return std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id, 1}, name, def);
}

std::unique_ptr<juce::AudioParameterFloat> semitones(const char* id, const char* name, float def) {
  return std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{id, 1}, name, juce::NormalisableRange<float>(-24.0f, 24.0f, 1.0f), def,
      juce::AudioParameterFloatAttributes().withLabel("st").withStringFromValueFunction([](float v, int) {
        const int st = juce::roundToInt(v);
        return (st > 0 ? "+" : "") + juce::String(st);
      }));
}

std::unique_ptr<juce::AudioParameterFloat> seconds(const char* id, const char* name, float lo, float hi, float centre,
                                                   float def) {
  juce::NormalisableRange<float> range(lo, hi);
  range.setSkewForCentre(centre);
  return std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{id, 1}, name, range, def,
      juce::AudioParameterFloatAttributes().withLabel("s").withStringFromValueFunction(
          [](float v, int) { return juce::String(v, v < 1.0f ? 2 : 1); }));
}

}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout DjontProcessor::createLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  layout.add(decibels("input", "Input"));
  layout.add(decibels("output", "Output"));
  // Wham (Wham.h): off by default; Auto, so it does something with no pedal.
  layout.add(power("whamOn", "Wham", false));
  layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"whamMode", 1}, "Wham Mode",
                                                          juce::StringArray{"Pedal", "Auto"}, 1));
  layout.add(percent("whamPedal", "Wham Pedal", 0.0f));
  layout.add(semitones("whamHeel", "Wham Heel", 0.0f));
  layout.add(semitones("whamToe", "Wham Toe", 12.0f));
  {
    juce::NormalisableRange<float> rate(Wham::kMinRateHz, Wham::kMaxRateHz);
    rate.setSkewForCentre(1.0f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"whamRate", 1}, "Wham Rate", rate, Wham::kDefaultRateHz,
        juce::AudioParameterFloatAttributes().withLabel("Hz").withStringFromValueFunction(
            [](float v, int) { return juce::String(v, v < 1.0f ? 2 : 1); })));
  }
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"whamShape", 1}, "Wham Shape", juce::StringArray{"Sine", "Triangle", "Rise", "Square"}, 0));
  layout.add(power("whamRetrig", "Wham Retrig", false));
  // The whammy-style controls: engine, the INTERVAL list (WhamModes.h), and
  // the ADV page's voicing.
  layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"whamAlgo", 1}, "Wham Engine",
                                                          juce::StringArray{"Classic", "Chords"}, 0));
  layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"whamInterval", 1}, "Wham Interval",
                                                          whamIntervalNames(), 0));
  layout.add(percent("whamMix", "Wham Mix", 100.0f));
  layout.add(hertz("whamTone", "Wham Tone", 1000.0f, 20000.0f, 20000.0f));
  {
    juce::NormalisableRange<float> grain(10.0f, ClassicShift::kMaxGrainMs);
    grain.setSkewForCentre(30.0f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"whamGrain", 1}, "Wham Grain", grain, 30.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms").withStringFromValueFunction(
            [](float v, int) { return juce::String(juce::roundToInt(v)); })));
  }
  layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"whamBuffer", 1}, "Wham Buffer",
                                                          juce::StringArray{"20 ms", "30 ms", "40 ms", "60 ms"}, 1));
  {
    juce::NormalisableRange<float> glide(0.0f, 2000.0f);
    glide.setSkewForCentre(250.0f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"whamGlide", 1}, "Wham Glide", glide, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms").withStringFromValueFunction(
            [](float v, int) { return juce::String(juce::roundToInt(v)); })));
  }
  // Sustain (Sustain.h): Hold (a freeze) or Sustainer (a sustainer pickup).
  layout.add(power("sustainOn", "Sustain", false));
  layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"sustainMode", 1}, "Sustain Mode",
                                                          juce::StringArray{"Hold", "Sustainer"}, 0));
  layout.add(power("holdOn", "Hold", false));
  layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"holdType", 1}, "Hold Type",
                                                          juce::StringArray{"Chord", "Pile-up"}, 0));
  layout.add(power("holdLatch", "Hold Latch", true));
  layout.add(seconds("holdFade", "Hold Fade", 0.02f, 4.0f, 0.5f, 0.3f));
  layout.add(percent("holdLevel", "Hold Level", 80.0f));
  layout.add(percent("holdShimmer", "Hold Shimmer", 0.0f));
  layout.add(hertz("holdLowCut", "Hold Low Cut", 20.0f, 500.0f, 20.0f));
  layout.add(hertz("holdTone", "Hold Tone", 1000.0f, 20000.0f, 20000.0f));
  layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"sustainerType", 1}, "Sustainer Type",
                                                          juce::StringArray{"Fundamental", "Harmonic", "Mix"}, 0));
  layout.add(percent("sustainerAmount", "Sustainer Amount", 60.0f));
  layout.add(seconds("sustainerBloom", "Sustainer Bloom", 0.2f, 6.0f, 1.5f, 1.5f));
  layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"sustainerInterval", 1},
                                                          "Sustainer Interval",
                                                          juce::StringArray{"Octave", "5th", "12th", "2 Octaves"}, 0));
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"sustainerFloor", 1}, "Sustainer Floor", juce::NormalisableRange<float>(-80.0f, -40.0f, 1.0f),
      -64.0f, juce::AudioParameterFloatAttributes().withLabel("dB")));
  {
    juce::NormalisableRange<float> response(2.0f, 100.0f);
    response.setSkewForCentre(15.0f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"sustainerResponse", 1}, "Sustainer Response", response, 15.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms").withStringFromValueFunction(
            [](float v, int) { return juce::String(juce::roundToInt(v)); })));
  }
  layout.add(hertz("sustainerTone", "Sustainer Tone", 1000.0f, 20000.0f, 20000.0f));
  layout.add(power("tightOn", "Tight", true));
  layout.add(percent("tightAmount", "Tight Amount", 50.0f));
  layout.add(hertz("tightFreq", "Tight Freq", 60.0f, 1000.0f, 180.0f));
  layout.add(power("gritOn", "Grit", false));
  layout.add(percent("gritDrive", "Grit Drive", 50.0f));
  layout.add(hertz("gritFreq", "Grit Freq", 60.0f, 400.0f, 140.0f));
  layout.add(percent("gritBlend", "Grit Blend", 40.0f));
  layout.add(power("subOn", "Sub", false));
  layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"subMode", 1}, "Sub Mode",
                                                          juce::StringArray{"Clean", "Synth"}, 0));
  layout.add(percent("subBlend", "Sub Blend", 40.0f));
  layout.add(hertz("subTone", "Sub Tone", 60.0f, 500.0f, 160.0f));
  layout.add(power("lofiOn", "Lo-Fi", false));
  layout.add(hertz("lofiTone", "Lo-Fi Tone", 300.0f, 3000.0f, 1000.0f));
  layout.add(percent("lofiBlend", "Lo-Fi Blend", 100.0f));
  hushui::addParameters(layout);
  return layout;
}

DjontProcessor::DjontProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "dJONt", createLayout()) {
  for (const char* id : {"whamOn", "whamAlgo", "whamInterval", "whamGrain", "whamBuffer"})
    params.addParameterListener(id, this);
  for (auto* p : getParameters())
    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p)) midiParams_.push_back(ranged);
  for (auto& cc : ccMap_) cc.store(-1);
  ccLast_.fill(0);
  PitchShift::Params p;
  p.window = kWhamWindow;
  whamShift_.setParams(p);
  hushParams_.attach(params);
}

DjontProcessor::~DjontProcessor() {
  for (const char* id : {"whamOn", "whamAlgo", "whamInterval", "whamGrain", "whamBuffer"})
    params.removeParameterListener(id, this);
  cancelPendingUpdate();
}

void DjontProcessor::parameterChanged(const juce::String&, float) { triggerAsyncUpdate(); }
void DjontProcessor::handleAsyncUpdate() { updateLatency(); }

// Wham's delay while it replaces the dry (Custom / Whammy). Harmony and
// Detune keep the dry on time and add a voice, so they report none.
void DjontProcessor::updateLatency() {
  const int index = juce::jlimit(0, kNumWhamIntervals - 1, juce::roundToInt(value("whamInterval")));
  const auto kind = kWhamIntervals[index].kind;
  int samples = 0;
  if (value("whamOn") > 0.5f && (kind == WhamInterval::Kind::custom || kind == WhamInterval::Kind::whammy))
    samples = value("whamAlgo") < 0.5f
                  ? ClassicShift::latencySamples(value("whamGrain"), sampleRate_)
                  : PitchShift::latencySamples(PitchShift::windowFromIndex(juce::roundToInt(value("whamBuffer"))),
                                               sampleRate_);
  setLatencySamples(samples);
}

bool DjontProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  const auto in = layouts.getMainInputChannelSet();
  const auto out = layouts.getMainOutputChannelSet();
  const auto ok = [](const juce::AudioChannelSet& s) {
    return s == juce::AudioChannelSet::mono() || s == juce::AudioChannelSet::stereo();
  };
  return ok(in) && ok(out);
}

void DjontProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  hush.prepare(sampleRate);
  tight_.prepare(sampleRate);
  grit_.prepare(sampleRate);
  sub_.prepare(sampleRate, samplesPerBlock);
  lofi_.prepare(sampleRate);
  hold_.prepare(sampleRate);
  sustainer_.prepare(sampleRate, juce::jmax(1, samplesPerBlock));
  sampleRate_ = sampleRate;
  whamShift_.prepare(sampleRate, juce::jmax(1, samplesPerBlock));
  wham_.prepare(sampleRate);
  classic_.prepare(sampleRate);
  whamDry_.setSize(1, juce::jmax(1, samplesPerBlock));
  whamScratch_.setSize(1, juce::jmax(1, samplesPerBlock));
  whamFade_.reset(sampleRate, 0.02);
  whamFade_.setCurrentAndTargetValue(value("whamOn") > 0.5f ? 1.0f : 0.0f);
  updateLatency();
  dry_.setSize(1, juce::jmax(1, samplesPerBlock));
  const char* powers[kNumModules] = {"sustainOn", "tightOn", "gritOn", "subOn", "lofiOn"};
  for (int m = 0; m < kNumModules; ++m) {
    power_[static_cast<size_t>(m)].reset(sampleRate, 0.02);
    power_[static_cast<size_t>(m)].setCurrentAndTargetValue(value(powers[m]) > 0.5f ? 1.0f : 0.0f);
  }
  inGain_.reset(sampleRate, 0.02);
  outGain_.reset(sampleRate, 0.02);
  inGain_.setCurrentAndTargetValue(dbToGain(value("input")));
  outGain_.setCurrentAndTargetValue(dbToGain(value("output")));
}

void DjontProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
  juce::ScopedNoDenormals noDenormals;
  handleMidi(midi);
  midi.clear();
  const int n = buffer.getNumSamples();
  if (n <= 0 || buffer.getNumChannels() == 0) return;

  tight_.set(value("tightAmount") / 100.0f, value("tightFreq"));
  grit_.set(value("gritDrive") / 100.0f, value("gritFreq"), value("gritBlend") / 100.0f);
  sub_.set(value("subMode") > 0.5f ? Sub::Mode::synth : Sub::Mode::clean, value("subBlend") / 100.0f,
           value("subTone"), value("subOn") > 0.5f);
  lofi_.set(value("lofiTone"), value("lofiBlend") / 100.0f);
  whamSettings_.mode = value("whamMode") > 0.5f ? Wham::Mode::autoSweep : Wham::Mode::pedal;
  whamSettings_.shape = static_cast<Wham::Shape>(juce::jlimit(0, 3, juce::roundToInt(value("whamShape"))));
  whamSettings_.pedal = value("whamPedal") / 100.0f;
  whamSettings_.rateHz = value("whamRate");
  whamSettings_.retrig = value("whamRetrig") > 0.5f;
  whamSettings_.glideMs = value("whamGlide");
  {
    const bool on = value("sustainOn") > 0.5f;
    sustainHoldMode_ = value("sustainMode") < 0.5f;
    hold_.set(on && sustainHoldMode_ && value("holdOn") > 0.5f,
              value("holdType") > 0.5f ? Hold::Type::pileUp : Hold::Type::chord, value("holdFade"),
              value("holdLevel") / 100.0f);
    sustainer_.set(value("sustainerAmount") / 100.0f, value("sustainerBloom"),
                   static_cast<Sustainer::Type>(juce::jlimit(0, 2, juce::roundToInt(value("sustainerType")))),
                   on && !sustainHoldMode_);
  }
  whamHeel_ = value("whamHeel");
  whamToe_ = value("whamToe");
  {
    const bool on = value("whamOn") > 0.5f;
    whamClassic_ = value("whamAlgo") < 0.5f;
    whamInterval_ = juce::jlimit(0, kNumWhamIntervals - 1, juce::roundToInt(value("whamInterval")));
    whamMix_ = value("whamMix") / 100.0f;
    const float tone = value("whamTone");
    whamToneOn_ = tone < 19000.0f;
    whamTone_.set(Biquad::Type::lowPass, sampleRate_, juce::jmin(tone, 19000.0f), 0.7071);
    classic_.setGrainMs(value("whamGrain"));
    whamShift_.setEnabled(on && !whamClassic_);
    whamFade_.setTargetValue(on ? 1.0f : 0.0f);
    hold_.setVoicing(value("holdShimmer") / 100.0f, value("holdLowCut"), value("holdTone"));
    const float intervals[] = {12.0f, 7.0f, 19.0f, 24.0f};
    sustainer_.setVoicing(intervals[juce::jlimit(0, 3, juce::roundToInt(value("sustainerInterval")))],
                          value("sustainerFloor"), value("sustainerResponse"), value("sustainerTone"));
  }
  inGain_.setTargetValue(dbToGain(value("input")));
  outGain_.setTargetValue(dbToGain(value("output")));
  const char* powers[kNumModules] = {"sustainOn", "tightOn", "gritOn", "subOn", "lofiOn"};
  for (int m = 0; m < kNumModules; ++m)
    power_[static_cast<size_t>(m)].setTargetValue(value(powers[m]) > 0.5f ? 1.0f : 0.0f);

  // Hosts may hand over a bigger block than promised: work in slices the
  // scratch buffers fit.
  float* x = buffer.getWritePointer(0);
  const int slice = dry_.getNumSamples();
  for (int offset = 0; offset < n; offset += slice) processMono(x + offset, juce::jmin(slice, n - offset));

  float peak = 0.0f;
  for (int i = 0; i < n; ++i) peak = std::max(peak, std::abs(x[i]));
  const float db = peak > 0.0f ? juce::jmax(-60.0f, juce::Decibels::gainToDecibels(peak)) : -60.0f;
  outputPeakDb.store(juce::jmax(db, outputPeakDb.load() - 1.5f));

  for (int ch = 1; ch < buffer.getNumChannels(); ++ch) buffer.copyFrom(ch, 0, buffer, 0, 0, n);
}

// Wham: the pedal position (Wham.h) picks the shift between heel and toe
// (or the INTERVAL's), per slice, through the CLASSIC or CHORDS engine; then
// the mix by the interval's kind (WhamModes.h). Power fades over 20 ms; off
// and settled, it's skipped (a bit-exact, zero-delay passthrough).
void DjontProcessor::processWham(float* x, int n) {
  // CHORDS still fading out after a switch to CLASSIC: let it finish on a
  // scratch copy so it can stop.
  if (whamClassic_ && whamShift_.isRunning()) {
    float* scratch = whamScratch_.getWritePointer(0);
    std::copy(x, x + n, scratch);
    float* channels[1] = {scratch};
    juce::AudioBuffer<float> view(channels, 1, n);
    whamShift_.process(view);
  }
  if (!whamFade_.isSmoothing() && whamFade_.getCurrentValue() <= 0.0f) {
    whamPosition.store(-1.0f);
    wham_.reset();
    classic_.reset();
    return;
  }
  const auto& mode = kWhamIntervals[static_cast<size_t>(whamInterval_)];
  const bool custom = mode.kind == WhamInterval::Kind::custom;
  const float heel = custom ? whamHeel_ : mode.heel, toe = custom ? whamToe_ : mode.toe;
  float* dry = whamDry_.getWritePointer(0);
  std::copy(x, x + n, dry);
  auto p = PitchShift::Params{};
  p.window = PitchShift::windowFromIndex(juce::roundToInt(value("whamBuffer")));
  float position = 0.0f;
  for (int start = 0; start < n; start += Wham::kSliceSamples) {
    const int len = juce::jmin(Wham::kSliceSamples, n - start);
    position = wham_.advance(whamSettings_, dry + start, len);
    const float semitones = mode.kind == WhamInterval::Kind::detune ? mode.heel : heel + (toe - heel) * position;
    if (whamClassic_) {
      classic_.process(x + start, len, semitones);
    } else {
      p.semitones = semitones;
      whamShift_.setParams(p);
      float* channels[1] = {x + start};
      juce::AudioBuffer<float> view(channels, 1, len);
      whamShift_.process(view);
    }
  }
  const bool replaces = custom || mode.kind == WhamInterval::Kind::whammy;
  for (int i = 0; i < n; ++i) {
    float wet = x[i];
    if (whamToneOn_) wet = whamTone_.process(wet);
    const float out = replaces ? dry[i] + whamMix_ * (wet - dry[i]) : dry[i] + whamMix_ * wet;
    x[i] = dry[i] + whamFade_.getNextValue() * (out - dry[i]);
  }
  whamPosition.store(mode.kind == WhamInterval::Kind::detune ? 0.0f : position);
}

void DjontProcessor::processSustain(float* x, int n) {
  if (sustainHoldMode_) {
    sustainer_.reset();
    hold_.process(x, n);
  } else {
    if (hold_.sounding()) hold_.process(x, n);  // let a held layer fade out
    sustainer_.process(x, n);
  }
}

// ---------------------------------------------------------------- MIDI
int DjontProcessor::indexOf(const juce::String& id) const {
  for (size_t i = 0; i < midiParams_.size(); ++i)
    if (midiParams_[i]->getParameterID() == id) return static_cast<int>(i);
  return -1;
}

void DjontProcessor::startLearn(const juce::String& id) { learnIndex_.store(indexOf(id)); }

juce::String DjontProcessor::learning() const {
  const int i = learnIndex_.load();
  return i >= 0 && i < static_cast<int>(midiParams_.size()) ? midiParams_[static_cast<size_t>(i)]->getParameterID()
                                                              : juce::String();
}

int DjontProcessor::ccFor(const juce::String& id) const {
  const int index = indexOf(id);
  if (index < 0) return -1;
  for (int cc = 0; cc < 128; ++cc)
    if (ccMap_[static_cast<size_t>(cc)].load() == index) return cc;
  return -1;
}

void DjontProcessor::forget(const juce::String& id) {
  const int index = indexOf(id);
  for (auto& cc : ccMap_)
    if (cc.load() == index) cc.store(-1);
  writeMidiState();
}

void DjontProcessor::handleMidi(const juce::MidiBuffer& midi) {
  for (const auto meta : midi) {
    const auto m = meta.getMessage();
    if (!m.isController()) continue;
    const int cc = m.getControllerNumber(), v = m.getControllerValue();
    const auto ucc = static_cast<size_t>(juce::jlimit(0, 127, cc));
    const int prev = ccLast_[ucc];
    ccLast_[ucc] = v;
    if (const int learn = learnIndex_.exchange(-1); learn >= 0) {
      for (auto& other : ccMap_)
        if (other.load() == learn) other.store(-1);
      ccMap_[ucc].store(learn);
      learnedCount.fetch_add(1);
      continue;  // the press that taught it doesn't also act
    }
    const int index = ccMap_[ucc].load();
    if (index < 0) continue;
    auto* param = midiParams_[static_cast<size_t>(index)];
    const auto id = param->getParameterID();
    float normalised = static_cast<float>(v) / 127.0f;
    if (dynamic_cast<juce::AudioParameterBool*>(param) != nullptr) {
      if (id == "holdOn" && value("holdLatch") > 0.5f) {
        if (!(prev < 64 && v >= 64)) continue;  // flip on the press only
        normalised = param->getValue() > 0.5f ? 0.0f : 1.0f;
      } else {
        normalised = v >= 64 ? 1.0f : 0.0f;
      }
    }
    if (!juce::approximatelyEqual(param->getValue(), normalised)) param->setValueNotifyingHost(normalised);
  }
}

// Mappings live in the state as <MIDI><Map cc="20" param="holdOn"/></MIDI>.
void DjontProcessor::writeMidiState() {
  auto tree = params.state.getOrCreateChildWithName("MIDI", nullptr);
  tree.removeAllChildren(nullptr);
  for (int cc = 0; cc < 128; ++cc)
    if (const int index = ccMap_[static_cast<size_t>(cc)].load(); index >= 0) {
      juce::ValueTree map("Map");
      map.setProperty("cc", cc, nullptr);
      map.setProperty("param", midiParams_[static_cast<size_t>(index)]->getParameterID(), nullptr);
      tree.appendChild(map, nullptr);
    }
}

void DjontProcessor::readMidiState() {
  for (auto& cc : ccMap_) cc.store(-1);
  const auto tree = params.state.getChildWithName("MIDI");
  for (const auto& map : tree) {
    const int cc = map.getProperty("cc", -1);
    const int index = indexOf(map.getProperty("param").toString());
    if (cc >= 0 && cc < 128 && index >= 0) ccMap_[static_cast<size_t>(cc)].store(index);
  }
}

void DjontProcessor::processMono(float* x, int n) {
  inGain_.applyGain(x, n);
  hushParams_.apply(hush);
  hush.process(x, n);
  processWham(x, n);
  for (int m = 0; m < kNumModules; ++m) {
    auto& p = power_[static_cast<size_t>(m)];
    // Off and settled: skip the module, and clear its state so it comes
    // back clean.
    if (!p.isSmoothing() && p.getCurrentValue() <= 0.0f) {
      switch (m) {
        case kSustain:
          // A held layer still fading out keeps sounding through power-off.
          if (hold_.sounding()) {
            hold_.process(x, n);
          } else {
            hold_.reset();
            sustainer_.reset();
          }
          break;
        case kTight: tight_.reset(); break;
        case kGrit: grit_.reset(); break;
        case kSub: sub_.reset(); break;
        case kLoFi: lofi_.reset(); break;
        default: break;
      }
      continue;
    }
    float* dry = dry_.getWritePointer(0);
    std::copy(x, x + n, dry);
    switch (m) {
      case kSustain: processSustain(x, n); break;
      case kTight: tight_.process(x, n); break;
      case kGrit: grit_.process(x, n); break;
      case kSub: sub_.process(x, n); break;
      case kLoFi: lofi_.process(x, n); break;
      default: break;
    }
    if (p.isSmoothing() || p.getCurrentValue() < 1.0f) {
      for (int i = 0; i < n; ++i) {
        const float w = p.getNextValue();
        x[i] = dry[i] + w * (x[i] - dry[i]);
      }
    }
  }
  outGain_.applyGain(x, n);
}

juce::AudioProcessorEditor* DjontProcessor::createEditor() { return new DjontEditor(*this); }

void DjontProcessor::getStateInformation(juce::MemoryBlock& dest) {
  writeMidiState();
  hushui::saveFloors(hush, params.state);
  if (auto xml = params.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void DjontProcessor::setStateInformation(const void* data, int size) {
  if (auto xml = getXmlFromBinary(data, size))
    if (xml->hasTagName(params.state.getType())) {
      params.replaceState(juce::ValueTree::fromXml(*xml));
      readMidiState();
      hushui::loadFloors(hush, params.state);
    }
}

}  // namespace djont

#ifndef DJONT_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new djont::DjontProcessor(); }
#endif
