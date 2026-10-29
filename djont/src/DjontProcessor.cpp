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
  layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"tightMode", 1}, "Tight Mode",
                                                          juce::StringArray{"Punch", "Focus"}, 0));
  layout.add(percent("blend", "Blend", 100.0f));
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
  // Scenes: which one is active (host automation / MIDI pick it by value).
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"scene", 1}, "Scene", juce::StringArray{"A", "B", "C", "D", "E", "F", "G", "H"}, 0));
  return layout;
}

DjontProcessor::DjontProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "dJONt", createLayout()),
      presets_([this](const juce::String& id) {
        auto* p = params.getParameter(id);
        return p != nullptr ? p->convertFrom0to1(p->getDefaultValue()) : 0.0f;
      }) {
  for (const char* id : {"whamOn", "whamAlgo", "whamInterval", "whamGrain", "whamBuffer", "scene"})
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
  for (const char* id : {"whamOn", "whamAlgo", "whamInterval", "whamGrain", "whamBuffer", "scene"})
    params.removeParameterListener(id, this);
  cancelPendingUpdate();
}

void DjontProcessor::parameterChanged(const juce::String& id, float value) {
  if (id == "scene") pendingScene_.store(juce::roundToInt(value));
  triggerAsyncUpdate();
}

void DjontProcessor::handleAsyncUpdate() {
  updateLatency();
  if (const int program = pendingProgram_.exchange(-1); program >= 0) setCurrentProgram(program);
  if (const int scene = pendingScene_.exchange(-1); scene >= 0 && scene != activeScene_ && !applyingScene_) {
    if (scene < sceneCount_) selectScene(scene);
    else syncSceneParam();  // past the last scene: snap back
  }
}

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
  focus_.prepare(sampleRate);
  grit_.prepare(sampleRate);
  blendRing_.assign(static_cast<size_t>(sampleRate) + 1, 0.0f);  // up to 1 s of Wham delay
  blendDry_.assign(static_cast<size_t>(juce::jmax(1, samplesPerBlock)), 0.0f);
  blendPos_ = 0;
  blend_.reset(sampleRate, 0.03);
  blend_.setCurrentAndTargetValue(value("blend") / 100.0f);
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
  handleMidi(midi);  // and leaves the MIDI in the buffer for whatever comes next
  const int n = buffer.getNumSamples();
  if (n <= 0 || buffer.getNumChannels() == 0) return;

  tight_.set(value("tightAmount") / 100.0f, value("tightFreq"));
  focus_.set(value("tightAmount") / 100.0f, value("tightFreq"));
  {
    const bool focus = value("tightMode") > 0.5f;
    if (focus != focusMode_) { focusMode_ = focus; tight_.reset(); focus_.reset(); }
  }
  blend_.setTargetValue(value("blend") / 100.0f);
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
    if (m.isProgramChange()) {
      pendingProgram_.store(m.getProgramChangeNumber());
      triggerAsyncUpdate();
      continue;
    }
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
    if (id == "scene") {
      // By value, like T3KJ: 0 = A, 1 = B, ... (past H: ignored).
      if (v >= kMaxScenes) continue;
      normalised = param->convertTo0to1(static_cast<float>(v));
    } else if (dynamic_cast<juce::AudioParameterBool*>(param) != nullptr) {
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
  // Global Blend: keep a copy of the (de-noised) DI, delayed by whatever Wham
  // adds so it lines up with the board's output.
  const bool blending = blend_.isSmoothing() || blend_.getCurrentValue() < 0.999f;
  if (blending) {
    const int lat = juce::jlimit(0, static_cast<int>(blendRing_.size()) - 1, getLatencySamples());
    const int size = static_cast<int>(blendRing_.size());
    for (int i = 0; i < n; ++i) {
      blendRing_[static_cast<size_t>(blendPos_)] = x[i];
      blendDry_[static_cast<size_t>(i)] = blendRing_[static_cast<size_t>((blendPos_ - lat + size) % size)];
      blendPos_ = (blendPos_ + 1) % size;
    }
  } else {
    for (int i = 0; i < n; ++i) {  // keep the ring current so turning Blend down is seamless
      blendRing_[static_cast<size_t>(blendPos_)] = x[i];
      blendPos_ = (blendPos_ + 1) % static_cast<int>(blendRing_.size());
    }
  }
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
        case kTight: tight_.reset(); focus_.reset(); break;
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
      case kTight: if (focusMode_) focus_.process(x, n); else tight_.process(x, n); break;
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
  if (blending)
    for (int i = 0; i < n; ++i) {
      const float b = blend_.getNextValue();
      x[i] = blendDry_[static_cast<size_t>(i)] + b * (x[i] - blendDry_[static_cast<size_t>(i)]);
    }
  outGain_.applyGain(x, n);
}

juce::AudioProcessorEditor* DjontProcessor::createEditor() { return new DjontEditor(*this); }

void DjontProcessor::getStateInformation(juce::MemoryBlock& dest) {
  writeMidiState();
  hushui::saveFloors(hush, params.state);
  auto state = params.copyState();
  state.removeChild(state.getChildWithName("Scenes"), nullptr);
  state.appendChild(sceneTree(), nullptr);
  if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}

void DjontProcessor::setStateInformation(const void* data, int size) {
  if (auto xml = getXmlFromBinary(data, size))
    if (xml->hasTagName(params.state.getType())) {
      auto state = juce::ValueTree::fromXml(*xml);
      const auto scenes = state.getChildWithName("Scenes").createCopy();
      state.removeChild(state.getChildWithName("Scenes"), nullptr);
      applyingScene_ = true;
      params.replaceState(state);
      applySceneTree(scenes);
      applyingScene_ = false;
      pendingScene_.store(-1);
      readMidiState();
      hushui::loadFloors(hush, params.state);
      presetRevision.fetch_add(1);
    }
}

// ---------------------------------------------------------------- scenes
const juce::StringArray& DjontProcessor::sceneParamIds() {
  // Everything but the scene pick itself and the performance controls
  // (HOLD, the Wham pedal's position): those follow your feet, not the scene.
  // (A test checks this covers every other parameter.)
  static const juce::StringArray ids{
      "input", "output", "blend",
      "whamOn", "whamMode", "whamHeel", "whamToe", "whamRate", "whamShape", "whamRetrig", "whamAlgo",
      "whamInterval", "whamMix", "whamTone", "whamGrain", "whamBuffer", "whamGlide",
      "sustainOn", "sustainMode", "holdType", "holdLatch", "holdFade", "holdLevel", "holdShimmer", "holdLowCut",
      "holdTone", "sustainerType", "sustainerAmount", "sustainerBloom", "sustainerInterval", "sustainerFloor",
      "sustainerResponse", "sustainerTone",
      "tightOn", "tightAmount", "tightFreq", "tightMode",
      "gritOn", "gritDrive", "gritFreq", "gritBlend",
      "subOn", "subMode", "subBlend", "subTone",
      "lofiOn", "lofiTone", "lofiBlend",
      "hushOn", "hushStrength", "hushSensitivity", "hushRelease"};
  return ids;
}

DjontProcessor::SceneSlot DjontProcessor::captureScene() const {
  SceneSlot slot;
  slot.stored = true;
  for (const auto& id : sceneParamIds())
    if (auto* p = params.getParameter(id)) slot.values.emplace_back(id, p->convertFrom0to1(p->getValue()));
  return slot;
}

void DjontProcessor::applyScene(const SceneSlot& slot) {
  if (!slot.stored) return;
  for (const auto& [id, value] : slot.values)
    if (auto* p = params.getParameter(id)) {
      const float norm = p->convertTo0to1(value);
      if (std::abs(norm - p->getValue()) > 1.0e-6f) p->setValueNotifyingHost(norm);
    }
}

void DjontProcessor::syncSceneParam() {
  if (auto* p = params.getParameter("scene")) {
    const float norm = p->convertTo0to1(static_cast<float>(activeScene_));
    if (std::abs(norm - p->getValue()) > 1.0e-6f) {
      const juce::ScopedValueSetter<bool> guard(applyingScene_, true);
      p->setValueNotifyingHost(norm);
    }
  }
}

juce::String DjontProcessor::sceneName(int index) const {
  return index >= 0 && index < kMaxScenes ? sceneNames_[static_cast<size_t>(index)] : juce::String();
}

bool DjontProcessor::selectScene(int index) {
  if (index < 0 || index >= sceneCount_) return false;
  if (index != activeScene_) {
    scenes_[static_cast<size_t>(activeScene_)] = captureScene();
    activeScene_ = index;
    const juce::ScopedValueSetter<bool> guard(applyingScene_, true);
    applyScene(scenes_[static_cast<size_t>(index)]);
  }
  syncSceneParam();
  sceneRevision.fetch_add(1);
  return true;
}

bool DjontProcessor::addScene() {
  if (sceneCount_ >= kMaxScenes) return false;
  const auto slot = static_cast<size_t>(sceneCount_);
  scenes_[slot] = {};
  sceneNames_[slot] = {};
  ++sceneCount_;
  return selectScene(sceneCount_ - 1);  // never visited: a copy of where you were
}

bool DjontProcessor::removeScene(int index) {
  if (index < 0 || index >= sceneCount_ || sceneCount_ <= 1) return false;
  const int last = sceneCount_ - 1;
  const SceneSlot* recall = nullptr;
  SceneSlot target;
  if (index == activeScene_) {
    // The live settings are the scene being removed: land on the one after
    // it (or before, when it was the last) and recall that scene.
    target = scenes_[static_cast<size_t>(index < last ? index + 1 : index - 1)];
    recall = &target;
    activeScene_ = index < last ? index : index - 1;
  } else if (index < activeScene_) {
    --activeScene_;
  }
  for (int i = index; i < last; ++i) {
    scenes_[static_cast<size_t>(i)] = scenes_[static_cast<size_t>(i + 1)];
    sceneNames_[static_cast<size_t>(i)] = sceneNames_[static_cast<size_t>(i + 1)];
  }
  scenes_[static_cast<size_t>(last)] = {};
  sceneNames_[static_cast<size_t>(last)] = {};
  sceneCount_ = last;
  if (recall != nullptr) {
    const juce::ScopedValueSetter<bool> guard(applyingScene_, true);
    applyScene(*recall);
  }
  syncSceneParam();
  sceneRevision.fetch_add(1);
  return true;
}

bool DjontProcessor::renameScene(int index, const juce::String& name) {
  if (index < 0 || index >= sceneCount_) return false;
  sceneNames_[static_cast<size_t>(index)] = name.trim().substring(0, 40);
  sceneRevision.fetch_add(1);
  return true;
}

// <Scenes count active><Scene index name stored><P id v/>...</Scene></Scenes>.
// The active scene's slot is written from the live settings.
juce::ValueTree DjontProcessor::sceneTree() const {
  juce::ValueTree tree("Scenes");
  tree.setProperty("count", sceneCount_, nullptr);
  tree.setProperty("active", activeScene_, nullptr);
  for (int i = 0; i < sceneCount_; ++i) {
    const auto slot = i == activeScene_ ? captureScene() : scenes_[static_cast<size_t>(i)];
    juce::ValueTree scene("Scene");
    scene.setProperty("index", i, nullptr);
    scene.setProperty("name", sceneNames_[static_cast<size_t>(i)], nullptr);
    scene.setProperty("stored", slot.stored, nullptr);
    for (const auto& [id, value] : slot.values) {
      juce::ValueTree p("P");
      p.setProperty("id", id, nullptr);
      p.setProperty("v", value, nullptr);
      scene.appendChild(p, nullptr);
    }
    tree.appendChild(scene, nullptr);
  }
  return tree;
}

// Sets the scene list only; the live parameters are the caller's (they are
// the active scene's). A missing tree = four scenes, all copies.
void DjontProcessor::applySceneTree(const juce::ValueTree& tree) {
  sceneCount_ = juce::jlimit(1, kMaxScenes, static_cast<int>(tree.getProperty("count", 4)));
  activeScene_ = juce::jlimit(0, sceneCount_ - 1, static_cast<int>(tree.getProperty("active", 0)));
  for (auto& s : scenes_) s = {};
  for (auto& n : sceneNames_) n = {};
  for (const auto& scene : tree) {
    const int i = scene.getProperty("index", -1);
    if (i < 0 || i >= sceneCount_) continue;
    auto& slot = scenes_[static_cast<size_t>(i)];
    sceneNames_[static_cast<size_t>(i)] = scene.getProperty("name").toString();
    slot.stored = static_cast<bool>(scene.getProperty("stored", false));
    for (const auto& p : scene)
      slot.values.emplace_back(p.getProperty("id").toString(), static_cast<float>(static_cast<double>(p.getProperty("v"))));
  }
  syncSceneParam();
  sceneRevision.fetch_add(1);
}

// ---------------------------------------------------------------- presets
bool DjontProcessor::loadPreset(const juce::String& name) {
  presets_.reload();
  const auto* preset = presets_.find(name);
  if (preset == nullptr) return false;
  const auto data = preset->data;
  const auto scenes = data.getChildWithName("Scenes");
  const int active = juce::jlimit(0, kMaxScenes - 1, static_cast<int>(scenes.getProperty("active", 0)));
  // The live settings = the preset's active scene (else its base settings).
  juce::ValueTree live = data.getChildWithName("Params");
  for (const auto& scene : scenes)
    if (static_cast<int>(scene.getProperty("index", -1)) == active && static_cast<bool>(scene.getProperty("stored", false)))
      live = scene;
  {
    const juce::ScopedValueSetter<bool> guard(applyingScene_, true);
    for (const auto& id : sceneParamIds())
      if (auto* p = params.getParameter(id)) {
        float v = p->convertFrom0to1(p->getDefaultValue());
        for (const auto& item : live)
          if (item.getProperty("id").toString() == id) v = static_cast<float>(static_cast<double>(item.getProperty("v")));
        p->setValueNotifyingHost(p->convertTo0to1(v));
      }
    applySceneTree(scenes);
  }
  pendingScene_.store(-1);
  params.state.setProperty("presetName", name, nullptr);
  presetRevision.fetch_add(1);
  updateHostDisplay(ChangeDetails{}.withProgramChanged(true));
  return true;
}

bool DjontProcessor::savePreset(const juce::String& name) {
  juce::ValueTree data("Preset");
  juce::ValueTree values("Params");
  for (const auto& [id, value] : captureScene().values) {
    juce::ValueTree p("P");
    p.setProperty("id", id, nullptr);
    p.setProperty("v", value, nullptr);
    values.appendChild(p, nullptr);
  }
  data.appendChild(values, nullptr);
  data.appendChild(sceneTree(), nullptr);
  if (!presets_.put(name, data)) return false;
  params.state.setProperty("presetName", name.trim().substring(0, 60), nullptr);
  presetRevision.fetch_add(1);
  updateHostDisplay(ChangeDetails{}.withProgramChanged(true));
  return true;
}

bool DjontProcessor::renamePreset(const juce::String& from, const juce::String& to) {
  if (!presets_.rename(from, to)) return false;
  if (currentPreset() == from) params.state.setProperty("presetName", to.trim().substring(0, 60), nullptr);
  presetRevision.fetch_add(1);
  updateHostDisplay(ChangeDetails{}.withProgramChanged(true));
  return true;
}

bool DjontProcessor::deletePreset(const juce::String& name) {
  if (!presets_.remove(name)) return false;
  if (currentPreset() == name) params.state.setProperty("presetName", "", nullptr);
  presetRevision.fetch_add(1);
  updateHostDisplay(ChangeDetails{}.withProgramChanged(true));
  return true;
}

int DjontProcessor::getCurrentProgram() {
  return juce::jmax(0, presets_.indexOf(currentPreset()));
}

void DjontProcessor::setCurrentProgram(int index) {
  if (!juce::MessageManager::existsAndIsCurrentThread()) {
    pendingProgram_.store(index);
    triggerAsyncUpdate();
    return;
  }
  presets_.reload();
  const auto& all = presets_.all();
  if (index < 0 || index >= static_cast<int>(all.size())) return;  // empty slot: ignore
  if (all[static_cast<size_t>(index)].name == currentPreset()) return;  // host echo
  loadPreset(all[static_cast<size_t>(index)].name);
}

const juce::String DjontProcessor::getProgramName(int index) {
  const auto& all = presets_.all();
  return index >= 0 && index < static_cast<int>(all.size()) ? all[static_cast<size_t>(index)].name : juce::String();
}

}  // namespace djont

#ifndef DJONT_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new djont::DjontProcessor(); }
#endif
