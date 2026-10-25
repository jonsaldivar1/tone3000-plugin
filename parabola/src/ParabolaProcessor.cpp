#include "ParabolaProcessor.h"

// AudioDSPTools compatibility defines (it originally came from iPlug2).
#ifndef DEFAULT_BLOCK_SIZE
#define DEFAULT_BLOCK_SIZE 1024
#endif
namespace iplug {
constexpr double PI = 3.14159265358979323846;
}
#include "dsp/ResamplingContainer/ResamplingContainer.h"

#include "ParabolaEditor.h"

namespace parabola {

namespace {
const juce::Identifier kFilesNode{"Files"};
const juce::Identifier kIrProp{"ir"};

juce::String bandId(int band, const char* suffix) { return juce::String(kBandIds[band]) + suffix; }

std::unique_ptr<juce::AudioParameterFloat> decibels(const juce::String& id, const juce::String& name, float min,
                                                    float max, float def) {
  return std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{id, 1}, name, juce::NormalisableRange<float>(min, max, 0.1f), def,
      juce::AudioParameterFloatAttributes().withLabel("dB").withStringFromValueFunction([](float v, int) {
        return (v > 0.05f ? "+" : "") + juce::String(v, 1);
      }));
}

std::unique_ptr<juce::AudioParameterFloat> hertz(const juce::String& id, const juce::String& name, float min,
                                                 float max, float def, float centre) {
  juce::NormalisableRange<float> range(min, max);
  range.setSkewForCentre(centre);
  return std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{id, 1}, name, range, def,
      juce::AudioParameterFloatAttributes().withLabel("Hz").withStringFromValueFunction([](float v, int) {
        return v >= 1000.0f ? juce::String(v / 1000.0f, v >= 10000.0f ? 1 : 2) + "k" : juce::String(juce::roundToInt(v));
      }));
}

std::unique_ptr<juce::AudioParameterFloat> percent(const juce::String& id, const juce::String& name, float def) {
  return std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{id, 1}, name, juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), def,
      juce::AudioParameterFloatAttributes().withLabel("%"));
}

std::unique_ptr<juce::AudioParameterBool> toggle(const juce::String& id, const juce::String& name, bool def) {
  return std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id, 1}, name, def);
}

// The gain that puts an IR's average response over 100 Hz - 3 kHz (where a
// driven bass lives) at 0 dB, so switching the cab on doesn't jump the level.
float irLevelMatch(const juce::AudioBuffer<float>& ir, double fs) {
  const float* x = ir.getReadPointer(0);
  const int n = ir.getNumSamples();
  double sumDb = 0.0;
  int count = 0;
  for (int k = 0; k < 24; ++k) {
    const double hz = 100.0 * std::pow(30.0, k / 23.0);
    const double w = juce::MathConstants<double>::twoPi * hz / fs;
    double re = 0.0, im = 0.0;
    for (int i = 0; i < n; ++i) {
      re += x[i] * std::cos(w * i);
      im -= x[i] * std::sin(w * i);
    }
    sumDb += 10.0 * std::log10(std::max(re * re + im * im, 1e-20));
    ++count;
  }
  const double avgDb = sumDb / count;
  return static_cast<float>(std::pow(10.0, juce::jlimit(-40.0, 40.0, -avgDb) / 20.0));
}

float peakDb(const float* x, int n) {
  float p = 0.0f;
  for (int i = 0; i < n; ++i) p = std::max(p, std::abs(x[i]));
  return gainToDb(p);
}
}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout ParabolaProcessor::createLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  layout.add(decibels("input", "Input", -24.0f, 24.0f, 0.0f));
  layout.add(decibels("output", "Output", -24.0f, 24.0f, 0.0f));
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"gate", 1}, "Gate", juce::NormalisableRange<float>(-90.0f, -20.0f, 0.5f), -90.0f,
      juce::AudioParameterFloatAttributes().withLabel("dB").withStringFromValueFunction([](float v, int) {
        return v <= -89.5f ? juce::String("Off") : juce::String(v, 1);
      })));
  layout.add(std::make_unique<juce::AudioParameterInt>(
      juce::ParameterID{"pitch", 1}, "Pitch", -12, 12, 0,
      juce::AudioParameterIntAttributes().withLabel("st").withStringFromValueFunction([](int v, int) {
        return v == 0 ? juce::String("Off") : (v > 0 ? "+" : "") + juce::String(v);
      })));
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"driveGate", 1}, "Noise Gate (drive bands)", juce::NormalisableRange<float>(-90.0f, -20.0f, 0.5f),
      -90.0f, juce::AudioParameterFloatAttributes().withLabel("dB").withStringFromValueFunction([](float v, int) {
        return v <= -89.5f ? juce::String("Off") : juce::String(v, 1);
      })));
  layout.add(hertz("xoverLow", "Crossover Low", 60.0f, 400.0f, 180.0f, 160.0f));
  layout.add(hertz("xoverHigh", "Crossover High", 600.0f, 5000.0f, 1600.0f, 1600.0f));

  const float levels[kNumBands] = {0.0f, -2.0f, -6.0f};
  const float tones[kNumBands] = {0.0f, 5000.0f, 12000.0f};
  for (int b = 0; b < kNumBands; ++b) {
    const juce::String n = kBandNames[b];
    layout.add(toggle(bandId(b, "On"), n + " On", true));
    layout.add(decibels(bandId(b, "Drive"), n + " Drive", -12.0f, 24.0f, 0.0f));
    layout.add(decibels(bandId(b, "Level"), n + " Level", -36.0f, 12.0f, levels[b]));
    layout.add(decibels(bandId(b, "Bass"), n + " Bass", -12.0f, 12.0f, 0.0f));
    layout.add(decibels(bandId(b, "Mid"), n + " Mid", -12.0f, 12.0f, 0.0f));
    layout.add(decibels(bandId(b, "Treble"), n + " Treble", -12.0f, 12.0f, 0.0f));
    layout.add(toggle(bandId(b, "Cab"), n + " Cab", b != kLow));
    if (b == kLow) {
      layout.add(percent("lowComp", "Low Comp", 40.0f));
    } else {
      layout.add(hertz(bandId(b, "Tone"), n + " Tone", 1000.0f, 20000.0f, tones[b], 6000.0f));
      layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{bandId(b, "Split"), 1}, n + " Split",
                                                              juce::StringArray{"Pre", "Post"}, 0));
    }
  }
  layout.add(toggle("cabOn", "Cab On", true));
  const char* geqNames[6] = {"80", "250", "500", "1k", "2.5k", "6k"};
  for (int i = 0; i < 6; ++i)
    layout.add(decibels("geq" + juce::String(i), juce::String("EQ ") + geqNames[i], -12.0f, 12.0f, 0.0f));
  return layout;
}

ParabolaProcessor::ParabolaProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "Parabola", createLayout()) {
  for (int b = 0; b < kNumBands; ++b) {
    const auto get = [&](const char* s) { return params.getRawParameterValue(bandId(b, s)); };
    on_[static_cast<size_t>(b)] = get("On");
    drive_[static_cast<size_t>(b)] = get("Drive");
    level_[static_cast<size_t>(b)] = get("Level");
    bass_[static_cast<size_t>(b)] = get("Bass");
    mid_[static_cast<size_t>(b)] = get("Mid");
    treble_[static_cast<size_t>(b)] = get("Treble");
    cab_[static_cast<size_t>(b)] = get("Cab");
    toneP_[static_cast<size_t>(b)] = b == kLow ? nullptr : get("Tone");
    split_[static_cast<size_t>(b)] = b == kLow ? nullptr : get("Split");
  }
  input_ = params.getRawParameterValue("input");
  output_ = params.getRawParameterValue("output");
  gate_p_ = params.getRawParameterValue("gate");
  xoverLow_ = params.getRawParameterValue("xoverLow");
  xoverHigh_ = params.getRawParameterValue("xoverHigh");
  cabOn_ = params.getRawParameterValue("cabOn");
  lowComp_ = params.getRawParameterValue("lowComp");
  for (int i = 0; i < 6; ++i) geqP_[static_cast<size_t>(i)] = params.getRawParameterValue("geq" + juce::String(i));
  pitchP_ = params.getRawParameterValue("pitch");
  driveGateP_ = params.getRawParameterValue("driveGate");
  for (auto* p : getParameters())
    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p)) midiParams_.push_back(ranged);
  for (auto& cc : ccMap_) cc.store(-1);
  ccLast_.fill(0);
  for (auto& p : bandPeakDb) p.store(-100.0f);
  internalFn_ = [this](float** in, float** out, int n) { processInternal(in[0], out[0], n); };
  prepareInternal(512);
}

ParabolaProcessor::~ParabolaProcessor() { cancelPendingUpdate(); }

bool ParabolaProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  const auto out = layouts.getMainOutputChannelSet();
  if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
  const auto in = layouts.getMainInputChannelSet();
  return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void ParabolaProcessor::prepareInternal(int maxInternalBlock) {
  internalMax_ = juce::jmax(16, maxInternalBlock);
  const auto n = static_cast<size_t>(internalMax_);
  for (auto& b : bandBuf_) b.assign(n, 0.0f);
  cabBuf_.assign(n, 0.0f);
  restBuf_.assign(n, 0.0f);
  driveGain_buf_.assign(n, 1.0f);
  driveGate_.prepare(kInternalRate);
  for (auto& f : postHp_) f.reset();
  for (auto& f : postLp_) f.reset();
  directBuf_.assign(n, 0.0f);
  gate_.prepare(kInternalRate);
  pitch_.prepare(kInternalRate, internalMax_);
  {
    PitchShift::Params pp;
    pp.window = kPitchWindow;
    pitch_.setParams(pp);
  }
  crossover_.prepare(kInternalRate);
  comp_.prepare(kInternalRate);
  for (auto& t : tone_) t.reset();
  toneHz_.fill(-1.0f);
  for (auto& e : eq_) e.prepare(kInternalRate);
  geq_.prepare(kInternalRate);
  for (auto& slot : slots_) slot.prepare(kInternalRate, internalMax_);
  convolver_.prepare({kInternalRate, static_cast<juce::uint32>(internalMax_), 1});
  convolver_.reset();
}

void ParabolaProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  hostRate_ = sampleRate > 0 ? sampleRate : kInternalRate;
  hostMaxBlock_ = juce::jmax(16, samplesPerBlock);
  monoIn_.assign(static_cast<size_t>(hostMaxBlock_), 0.0f);
  monoOut_.assign(static_cast<size_t>(hostMaxBlock_), 0.0f);
  if (std::abs(hostRate_ - kInternalRate) < 0.5) {
    resampler_.reset();
    prepareInternal(hostMaxBlock_);
  } else {
    if (resampler_ == nullptr) resampler_ = std::make_unique<Resampler>(kInternalRate);
    resampler_->Reset(hostRate_, hostMaxBlock_);
    prepareInternal(static_cast<int>(std::ceil(hostMaxBlock_ * kInternalRate / hostRate_)) + 8);
  }
  tuner.prepare(hostRate_);
  updateLatency();
}

bool ParabolaProcessor::bandUsesCab(int band) const {
  return cabOn_->load() > 0.5f && irLoaded_.load() && cab_[static_cast<size_t>(band)]->load() > 0.5f &&
         !slots_[static_cast<size_t>(band)].isFullRig();
}

void ParabolaProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
  juce::ScopedNoDenormals noDenormals;
  handleMidi(midi);
  const bool pitchOn = std::abs(pitchP_->load()) > 0.5f;
  if (pitchOn != pitchWasOn_) {
    pitchWasOn_ = pitchOn;
    triggerAsyncUpdate();  // the latency changes with it
  }
  const int total = buffer.getNumSamples();
  const int outs = getTotalNumOutputChannels();
  const int ins = getTotalNumInputChannels();
  for (int start = 0; start < total; start += hostMaxBlock_) {
    const int n = juce::jmin(hostMaxBlock_, total - start);
    if (ins > 0) juce::FloatVectorOperations::copy(monoIn_.data(), buffer.getReadPointer(0, start), n);
    else juce::FloatVectorOperations::clear(monoIn_.data(), n);
    if (tunerOn_.load()) tuner.pushSamples(monoIn_.data(), n);
    if (resampler_ != nullptr) {
      float* in = monoIn_.data();
      float* out = monoOut_.data();
      resampler_->ProcessBlock(&in, &out, n, internalFn_);
    } else {
      processInternal(monoIn_.data(), monoOut_.data(), n);
    }
    muteGain_.process(monoOut_.data(), n, tunerOn_.load() ? 0.0f : 1.0f);
    for (int c = 0; c < outs; ++c) juce::FloatVectorOperations::copy(buffer.getWritePointer(c, start), monoOut_.data(), n);
  }
  outputPeakDb.store(peakDb(buffer.getReadPointer(0), total));
}

void ParabolaProcessor::processInternal(float* in, float* out, int n) {
  for (int start = 0; start < n; start += internalMax_) {
    const int m = juce::jmin(internalMax_, n - start);
    processInternalChunk(in + start, out + start, m);
  }
}

void ParabolaProcessor::processInternalChunk(float* in, float* out, int n) {
  // Input, gate, split.
  inputGain_.process(in, n, dbToGain(input_->load()));
  gate_.setThresholdDb(gate_p_->load());
  crossover_.setFrequencies(xoverLow_->load(), xoverHigh_->load());
  float* low = bandBuf_[kLow].data();
  float* mid = bandBuf_[kMid].data();
  float* high = bandBuf_[kHigh].data();
  for (int i = 0; i < n; ++i) in[i] = gate_.process(in[i]);
  const float semitones = std::round(pitchP_->load());
  pitch_.setEnabled(std::abs(semitones) > 0.5f);
  if (pitch_.isRunning()) {
    PitchShift::Params pp;
    pp.semitones = semitones;
    pp.window = kPitchWindow;
    pitch_.setParams(pp);
    float* chans[1] = {in};
    juce::AudioBuffer<float> view(chans, 1, n);
    pitch_.process(view);
  }
  float* rest = restBuf_.data();
  for (int i = 0; i < n; ++i) crossover_.process(in[i], low[i], mid[i], high[i], rest[i]);
  gateOpen.store(gate_.isOpen());

  // The drive bands' gate follows the DI (after the first gate and Pitch).
  driveGate_.setThresholdDb(driveGateP_->load());
  const bool driveGateOn = driveGate_.enabled();
  if (driveGateOn)
    for (int i = 0; i < n; ++i) driveGain_buf_[static_cast<size_t>(i)] = driveGate_.gain(in[i]);
  const double f1 = xoverLow_->load(), f2 = std::max(static_cast<double>(xoverHigh_->load()), f1 * 1.5);

  // Bands.
  const bool cabReady = irLoaded_.load() && cabOn_->load() > 0.5f;
  juce::FloatVectorOperations::clear(cabBuf_.data(), n);
  juce::FloatVectorOperations::clear(directBuf_.data(), n);
  bool anyCab = false;
  for (int b = 0; b < kNumBands; ++b) {
    const auto bi = static_cast<size_t>(b);
    float* x = bandBuf_[bi].data();
    const bool on = on_[bi]->load() > 0.5f;
    // POST: the capture hears everything above the low split; its own slice
    // is cut out after it (drive first, then split).
    const bool post = b != kLow && split_[bi]->load() > 0.5f;
    if (post) juce::FloatVectorOperations::copy(x, rest, n);
    driveGain_[bi].process(x, n, dbToGain(drive_[bi]->load()));
    slots_[bi].process(x, n);
    if (post) {
      if (b == kMid) {
        postHp_[bi].set(kInternalRate, f1);
        postLp_[bi].set(kInternalRate, f2);
        for (int i = 0; i < n; ++i) {
          float lo, hi, lo2, hi2;
          postHp_[bi].process(x[i], lo, hi);
          postLp_[bi].process(hi, lo2, hi2);
          x[i] = lo2;
        }
      } else {
        postHp_[bi].set(kInternalRate, f2);
        for (int i = 0; i < n; ++i) {
          float lo, hi;
          postHp_[bi].process(x[i], lo, hi);
          x[i] = hi;
        }
      }
    }
    if (b != kLow && driveGateOn) juce::FloatVectorOperations::multiply(x, driveGain_buf_.data(), n);
    if (b == kLow) {
      comp_.setAmount(lowComp_->load() / 100.0f);
      for (int i = 0; i < n; ++i) x[i] = comp_.process(x[i]);
    } else {
      const float hz = toneP_[bi]->load();
      if (hz != toneHz_[bi]) {
        toneHz_[bi] = hz;
        tone_[bi].set(Biquad::Type::lowPass, kInternalRate, hz, 0.7071);
      }
      if (hz < 19900.0f)
        for (int i = 0; i < n; ++i) x[i] = tone_[bi].process(x[i]);
    }
    eq_[bi].set(bass_[bi]->load(), mid_[bi]->load(), treble_[bi]->load());
    for (int i = 0; i < n; ++i) x[i] = eq_[bi].process(x[i]);
    levelGain_[bi].process(x, n, on ? dbToGain(level_[bi]->load()) : 0.0f);
    bandPeakDb[bi].store(peakDb(x, n));

    const bool toCab = cabReady && cab_[bi]->load() > 0.5f && !slots_[bi].isFullRig();
    juce::FloatVectorOperations::add(toCab ? cabBuf_.data() : directBuf_.data(), x, n);
    anyCab = anyCab || toCab;
  }
  compReductionDb.store(comp_.reductionDb());

  if (anyCab) {
    float* chans[1] = {cabBuf_.data()};
    juce::dsp::AudioBlock<float> block(chans, 1, static_cast<size_t>(n));
    convolver_.process(juce::dsp::ProcessContextReplacing<float>(block));
    juce::FloatVectorOperations::add(directBuf_.data(), cabBuf_.data(), n);
  }

  // Graphic EQ and output.
  for (int i = 0; i < 6; ++i) geq_.set(i, geqP_[static_cast<size_t>(i)]->load());
  for (int i = 0; i < n; ++i) out[i] = geq_.process(directBuf_[static_cast<size_t>(i)]);
  outputGain_.process(out, n, dbToGain(output_->load()));
}

juce::AudioProcessorEditor* ParabolaProcessor::createEditor() { return new ParabolaEditor(*this); }

void ParabolaProcessor::setTunerActive(bool on) {
  tuner.setEnabled(on);
  tunerOn_.store(on);
}

// The resampler's delay (non-48k hosts) plus the shifter's while Pitch is on,
// in host samples. Reported from the message thread.
void ParabolaProcessor::updateLatency() {
  double samples = resampler_ != nullptr ? resampler_->GetLatency() : 0.0;
  if (std::abs(pitchP_->load()) > 0.5f)
    samples += PitchShift::latencySamples(kPitchWindow, kInternalRate) * hostRate_ / kInternalRate;
  setLatencySamples(juce::roundToInt(samples));
}

// ---------------------------------------------------------------- MIDI
int ParabolaProcessor::indexOf(const juce::String& id) const {
  for (size_t i = 0; i < midiParams_.size(); ++i)
    if (midiParams_[i]->getParameterID() == id) return static_cast<int>(i);
  return -1;
}

void ParabolaProcessor::startLearn(const juce::String& id) { learnIndex_.store(indexOf(id)); }

juce::String ParabolaProcessor::learning() const {
  const int i = learnIndex_.load();
  return i >= 0 && i < static_cast<int>(midiParams_.size()) ? midiParams_[static_cast<size_t>(i)]->getParameterID()
                                                              : juce::String();
}

int ParabolaProcessor::ccFor(const juce::String& id) const {
  const int index = indexOf(id);
  if (index < 0) return -1;
  for (int cc = 0; cc < 128; ++cc)
    if (ccMap_[static_cast<size_t>(cc)].load() == index) return cc;
  return -1;
}

void ParabolaProcessor::forget(const juce::String& id) {
  const int index = indexOf(id);
  for (auto& cc : ccMap_)
    if (cc.load() == index) cc.store(-1);
  writeMidiState();
}

void ParabolaProcessor::handleMidi(const juce::MidiBuffer& midi) {
  for (const auto meta : midi) {
    const auto m = meta.getMessage();
    if (!m.isController()) continue;
    const int cc = m.getControllerNumber(), v = m.getControllerValue();
    const auto ucc = static_cast<size_t>(juce::jlimit(0, 127, cc));
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
    float normalised = static_cast<float>(v) / 127.0f;
    if (dynamic_cast<juce::AudioParameterBool*>(param) != nullptr) normalised = v >= 64 ? 1.0f : 0.0f;
    if (!juce::approximatelyEqual(param->getValue(), normalised)) param->setValueNotifyingHost(normalised);
  }
}

// Mappings live in the state as <MIDI><Map cc="20" param="midOn"/></MIDI>.
void ParabolaProcessor::writeMidiState() {
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

void ParabolaProcessor::readMidiState() {
  for (auto& cc : ccMap_) cc.store(-1);
  const auto tree = params.state.getChildWithName("MIDI");
  for (const auto& map : tree) {
    const int cc = map.getProperty("cc", -1);
    const int index = indexOf(map.getProperty("param").toString());
    if (cc >= 0 && cc < 128 && index >= 0) ccMap_[static_cast<size_t>(cc)].store(index);
  }
}

// ---------------------------------------------------------------- files
juce::String ParabolaProcessor::loadNam(int band, const juce::File& file) {
  auto error = slots_[static_cast<size_t>(band)].load(file);
  {
    const juce::ScopedLock l(fileLock_);
    lastError_ = error;
  }
  bumpFiles();
  return error;
}

void ParabolaProcessor::clearNam(int band) {
  slots_[static_cast<size_t>(band)].clear();
  bumpFiles();
}

juce::String ParabolaProcessor::loadIr(const juce::File& file) {
  juce::String error;
  if (!file.existsAsFile()) {
    error = "File not found: " + file.getFullPathName();
  } else {
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0) {
      error = "Couldn't read " + file.getFileName() + " as audio";
    } else {
      // Mono (first channel), at most 1 s: cab IRs are a few hundred ms.
      const int length = static_cast<int>(std::min(reader->lengthInSamples, static_cast<juce::int64>(reader->sampleRate)));
      juce::AudioBuffer<float> ir(1, length);
      reader->read(&ir, 0, length, 0, true, false);
      ir.applyGain(irLevelMatch(ir, reader->sampleRate));
      convolver_.loadImpulseResponse(std::move(ir), reader->sampleRate, juce::dsp::Convolution::Stereo::no,
                                     juce::dsp::Convolution::Trim::yes, juce::dsp::Convolution::Normalise::no);
      const juce::ScopedLock l(fileLock_);
      irName_ = file.getFileNameWithoutExtension();
      irPath_ = file.getFullPathName();
      irLoaded_.store(true);
    }
  }
  {
    const juce::ScopedLock l(fileLock_);
    lastError_ = error;
  }
  bumpFiles();
  return error;
}

void ParabolaProcessor::clearIr() {
  irLoaded_.store(false);
  {
    const juce::ScopedLock l(fileLock_);
    irName_ = {};
    irPath_ = {};
  }
  bumpFiles();
}

juce::String ParabolaProcessor::irName() const {
  const juce::ScopedLock l(fileLock_);
  return irName_;
}

juce::String ParabolaProcessor::irPath() const {
  const juce::ScopedLock l(fileLock_);
  return irPath_;
}

juce::String ParabolaProcessor::lastError() const {
  const juce::ScopedLock l(fileLock_);
  return lastError_;
}

// ---------------------------------------------------------------- state
void ParabolaProcessor::getStateInformation(juce::MemoryBlock& destData) {
  writeMidiState();
  auto state = params.copyState();
  state.removeChild(state.getChildWithName(kFilesNode), nullptr);
  juce::ValueTree files(kFilesNode);
  for (int b = 0; b < kNumBands; ++b) files.setProperty(kBandIds[b], slots_[static_cast<size_t>(b)].info().path, nullptr);
  files.setProperty(kIrProp, irPath(), nullptr);
  state.appendChild(files, nullptr);
  if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void ParabolaProcessor::setStateInformation(const void* data, int sizeInBytes) {
  auto xml = getXmlFromBinary(data, sizeInBytes);
  if (xml == nullptr || !xml->hasTagName(params.state.getType())) return;
  auto state = juce::ValueTree::fromXml(*xml);
  const auto files = state.getChildWithName(kFilesNode).createCopy();
  state.removeChild(state.getChildWithName(kFilesNode), nullptr);
  params.replaceState(state);
  readMidiState();

  for (int b = 0; b < kNumBands; ++b) {
    const auto path = files.getProperty(kBandIds[b]).toString();
    if (path.isEmpty()) clearNam(b);
    else if (path != slots_[static_cast<size_t>(b)].info().path) loadNam(b, juce::File(path));
  }
  const auto ir = files.getProperty(kIrProp).toString();
  if (ir.isEmpty()) clearIr();
  else if (ir != irPath()) loadIr(juce::File(ir));
}

}  // namespace parabola

#if !defined(PARABOLA_NO_PLUGIN_ENTRY)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new parabola::ParabolaProcessor(); }
#endif
