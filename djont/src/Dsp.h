// dJONt DSP: four mono modules, run in this order on the guitar's DI ahead
// of the amp (T3KJ, or any amp sim):
//
//   Tight  pick-attack / palm-mute punch. A detector band-passes the signal
//          at FREQ and compares a fast envelope with a slow one; on an attack
//          the band is lifted (up to ~+12 dB), on the decay it is trimmed a
//          little, so chugs hit harder and stop sooner.
//   Grit   a fuzz layer on the low end only: the lows below FREQ are split
//          off, driven into an asymmetric tanh, low-passed again so the fizz
//          stays out, and blended back under the dry signal.
//   Sub    an octave down, blended under the dry signal through a low-pass
//          (TONE). CLEAN shifts with a granular pitch shifter (handles
//          chords, ~20 ms behind the dry note); SYNTH is a flip-flop divider
//          like an analog octave pedal (zero delay, single notes only, buzzy).
//   Lo-Fi  a radio / telephone band-pass around TONE with a touch of drive.
//
// All original code. Real-time safe: no allocation after prepare().
#pragma once

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "PitchShift.h"

namespace djont {

inline float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }

// RBJ biquad, transposed direct form II. Coefficients can be recomputed per
// block without clearing state.
struct Biquad {
  float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
  float z1 = 0, z2 = 0;

  void reset() { z1 = z2 = 0; }
  float process(float x) {
    const float y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
  }

  enum class Type { lowPass, highPass, bandPass };
  void set(Type type, double fs, double hz, double q) {
    hz = juce::jlimit(10.0, fs * 0.45, hz);
    const double w = juce::MathConstants<double>::twoPi * hz / fs;
    const double cw = std::cos(w), alpha = std::sin(w) / (2.0 * q);
    double nb0, nb1, nb2;
    switch (type) {
      case Type::lowPass: nb0 = (1 - cw) / 2; nb1 = 1 - cw; nb2 = (1 - cw) / 2; break;
      case Type::highPass: nb0 = (1 + cw) / 2; nb1 = -(1 + cw); nb2 = (1 + cw) / 2; break;
      case Type::bandPass: default: nb0 = alpha; nb1 = 0; nb2 = -alpha; break;  // 0 dB peak
    }
    const double a0 = 1 + alpha;
    b0 = static_cast<float>(nb0 / a0);
    b1 = static_cast<float>(nb1 / a0);
    b2 = static_cast<float>(nb2 / a0);
    a1 = static_cast<float>(-2 * cw / a0);
    a2 = static_cast<float>((1 - alpha) / a0);
  }
};

// Two cascaded Butterworth sections: a 4th-order (24 dB/oct) filter.
struct Biquad4 {
  Biquad s1, s2;
  void reset() { s1.reset(); s2.reset(); }
  void set(Biquad::Type type, double fs, double hz) {
    s1.set(type, fs, hz, 0.5412);
    s2.set(type, fs, hz, 1.3066);
  }
  float process(float x) { return s2.process(s1.process(x)); }
};

inline float onePole(double fs, double ms) {
  return static_cast<float>(std::exp(-1.0 / (fs * ms * 0.001)));
}

// ---------------------------------------------------------------- Tight
class Tight {
public:
  void prepare(double fs) {
    fs_ = fs;
    fastAttack_ = onePole(fs, 0.5);
    fastRelease_ = onePole(fs, 12.0);
    slowAttack_ = onePole(fs, 20.0);
    slowRelease_ = onePole(fs, 150.0);
    smooth_ = onePole(fs, 0.5);
    reset();
  }
  void reset() {
    detector_.reset();
    fast_ = slow_ = 0;
    gain_ = 1;
  }
  void set(float amount01, float freqHz) {
    amount_ = amount01;
    detector_.set(Biquad::Type::bandPass, fs_, freqHz, 0.8);
  }
  void process(float* x, int n) {
    for (int i = 0; i < n; ++i) {
      const float band = detector_.process(x[i]);
      const float a = std::abs(band);
      fast_ = a > fast_ ? fastAttack_ * fast_ + (1 - fastAttack_) * a : fastRelease_ * fast_ + (1 - fastRelease_) * a;
      slow_ = a > slow_ ? slowAttack_ * slow_ + (1 - slowAttack_) * a : slowRelease_ * slow_ + (1 - slowRelease_) * a;
      const float denom = slow_ + 1.0e-5f;
      const float attack = juce::jlimit(0.0f, 1.0f, (fast_ / denom - 1.0f) * 0.5f);
      const float decay = juce::jlimit(0.0f, 1.0f, (slow_ - fast_) / denom);
      const float target = (1.0f + amount_ * 3.0f * attack) * (1.0f - amount_ * 0.35f * decay);
      gain_ = smooth_ * gain_ + (1 - smooth_) * target;
      x[i] += (gain_ - 1.0f) * band;
    }
  }

private:
  double fs_ = 48000;
  Biquad detector_;
  float amount_ = 0.5f;
  float fast_ = 0, slow_ = 0, gain_ = 1;
  float fastAttack_ = 0, fastRelease_ = 0, slowAttack_ = 0, slowRelease_ = 0, smooth_ = 0;
};

// ---------------------------------------------------------------- Grit
class Grit {
public:
  void prepare(double fs) {
    fs_ = fs;
    dcR_ = 1.0f - static_cast<float>(juce::MathConstants<double>::twoPi * 10.0 / fs);
    reset();
  }
  void reset() {
    split_.reset();
    post_.reset();
    dcX_ = dcY_ = 0;
  }
  void set(float drive01, float freqHz, float blend01) {
    drive_ = dbToGain(6.0f + drive01 * 42.0f);
    blend_ = blend01;
    split_.set(Biquad::Type::lowPass, fs_, freqHz);
    post_.set(Biquad::Type::lowPass, fs_, freqHz * 4.0f);
  }
  void process(float* x, int n) {
    constexpr float kBias = 0.3f;
    const float offset = std::tanh(kBias);
    for (int i = 0; i < n; ++i) {
      const float low = split_.process(x[i]);
      const float fuzz = std::tanh(drive_ * low + kBias) - offset;
      // DC blocker: the bias makes the fuzz asymmetric.
      const float dc = fuzz - dcX_ + dcR_ * dcY_;
      dcX_ = fuzz;
      dcY_ = dc;
      x[i] += blend_ * 0.5f * post_.process(dc);
    }
  }

private:
  double fs_ = 48000;
  Biquad4 split_, post_;
  float drive_ = 1, blend_ = 0.4f;
  float dcX_ = 0, dcY_ = 0, dcR_ = 0.999f;
};

// ---------------------------------------------------------------- Sub
class Sub {
public:
  enum class Mode { clean, synth };

  void prepare(double fs, int maxBlock) {
    fs_ = fs;
    scratch_.assign(static_cast<size_t>(juce::jmax(1, maxBlock)), 0.0f);
    shifter_.prepare(fs, maxBlock);
    PitchShift::Params p;
    p.semitones = -12.0f;
    p.tonalityHz = 0.0f;
    p.window = PitchShift::Window::ms40;  // the long window keeps low notes smooth
    shifter_.setParams(p);
    envAttack_ = onePole(fs, 2.0);
    envRelease_ = onePole(fs, 60.0);
    pre_.set(Biquad::Type::lowPass, fs, 400.0, 0.7071);
    reset();
  }
  void reset() {
    pre_.reset();
    tone_.reset();
    env_ = 0;
    armed_ = false;
    flip_ = 1.0f;
  }
  void set(Mode mode, float blend01, float toneHz, bool active) {
    mode_ = mode;
    blend_ = blend01;
    tone_.set(Biquad::Type::lowPass, fs_, toneHz);
    shifter_.setEnabled(active && mode == Mode::clean);
  }
  void process(float* x, int n) {
    n = juce::jmin(n, static_cast<int>(scratch_.size()));
    float* wet = scratch_.data();
    for (int i = 0; i < n; ++i) wet[i] = pre_.process(x[i]);

    if (mode_ == Mode::clean) {
      float* channels[1] = {wet};
      juce::AudioBuffer<float> view(channels, 1, n);
      if (shifter_.isRunning())
        shifter_.process(view);
      else
        std::fill(wet, wet + n, 0.0f);
    } else {
      // Flip-flop divider: the square flips on every other rising zero
      // crossing (with hysteresis scaled to the envelope), so it runs at
      // half the input's pitch, then takes on the input's envelope.
      for (int i = 0; i < n; ++i) {
        const float s = wet[i];
        const float a = std::abs(s);
        env_ = a > env_ ? envAttack_ * env_ + (1 - envAttack_) * a : envRelease_ * env_ + (1 - envRelease_) * a;
        const float th = env_ * 0.25f;
        if (s < -th) armed_ = true;
        if (armed_ && s > th) {
          armed_ = false;
          flipHalf_ = !flipHalf_;
          if (flipHalf_) flip_ = -flip_;
        }
        wet[i] = flip_ * env_ * 1.6f;
      }
    }
    for (int i = 0; i < n; ++i) x[i] += blend_ * 1.2f * tone_.process(wet[i]);
  }
  // Delay of the CLEAN octave behind the dry note (the wet path only; the
  // plugin itself adds no latency).
  int cleanDelaySamples() const { return shifter_.latencySamples(); }

private:
  double fs_ = 48000;
  Mode mode_ = Mode::clean;
  float blend_ = 0.4f;
  Biquad pre_;
  Biquad4 tone_;
  PitchShift shifter_;
  std::vector<float> scratch_;
  float env_ = 0, envAttack_ = 0, envRelease_ = 0;
  bool armed_ = false, flipHalf_ = false;
  float flip_ = 1.0f;
};

// ---------------------------------------------------------------- Lo-Fi
class LoFi {
public:
  void prepare(double fs) {
    fs_ = fs;
    reset();
  }
  void reset() {
    hp_.reset();
    lp_.reset();
  }
  void set(float toneHz, float blend01) {
    blend_ = blend01;
    hp_.set(Biquad::Type::highPass, fs_, toneHz * 0.5f);
    lp_.set(Biquad::Type::lowPass, fs_, toneHz * 2.2f);
  }
  void process(float* x, int n) {
    const float norm = 1.4f / std::tanh(2.0f);
    for (int i = 0; i < n; ++i) {
      const float band = lp_.process(hp_.process(x[i]));
      const float wet = std::tanh(2.0f * band) * norm;
      x[i] = x[i] * (1.0f - blend_) + wet * blend_;
    }
  }

private:
  double fs_ = 48000;
  Biquad4 hp_, lp_;
  float blend_ = 1.0f;
};

}  // namespace djont
