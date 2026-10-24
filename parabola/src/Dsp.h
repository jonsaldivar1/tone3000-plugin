// Parabola Bass Z DSP building blocks. Everything here runs mono at the
// internal rate (48 kHz, the rate NAM A2 captures are trained at; the
// processor resamples around it when the host runs at another rate).
//
//   Crossover  two Linkwitz-Riley 24 dB/oct splits (low | mid | high). The
//              low band also runs through the upper split's all-pass, so the
//              three bands sum back flat (magnitude and phase) when nothing
//              in between changes them.
//   Gate       a downward expander on the DI, before the split.
//   Compressor the low band's: feed-forward, RMS-ish detector, auto makeup.
//   BandEq     per band Bass / Mid / Treble (shelf 100 Hz, peak 650 Hz,
//              shelf 3 kHz).
//   GraphicEq  six peaking bands after the bands are summed.
//
// All original code. Real-time safe: no allocation after prepare.
#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cmath>
#include <complex>

namespace parabola {

inline float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }
inline float gainToDb(float g) { return 20.0f * std::log10(std::max(g, 1.0e-9f)); }
inline float onePole(double fs, double ms) { return static_cast<float>(std::exp(-1.0 / (fs * ms * 0.001))); }

// RBJ biquad, transposed direct form II. Coefficients can be recomputed per
// block without clearing state.
struct Biquad {
  double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
  double z1 = 0, z2 = 0;

  void reset() { z1 = z2 = 0; }
  float process(float xf) {
    const double x = xf;
    const double y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return static_cast<float>(y);
  }

  enum class Type { lowPass, highPass, peak, lowShelf, highShelf };
  void set(Type type, double fs, double hz, double q, double gainDb = 0.0) {
    hz = juce::jlimit(10.0, fs * 0.45, hz);
    const double w = juce::MathConstants<double>::twoPi * hz / fs;
    const double cw = std::cos(w), sw = std::sin(w);
    const double alpha = sw / (2.0 * q);
    const double A = std::pow(10.0, gainDb / 40.0);
    double nb0 = 1, nb1 = 0, nb2 = 0, a0 = 1, na1 = 0, na2 = 0;
    switch (type) {
      case Type::lowPass:
        nb0 = (1 - cw) / 2; nb1 = 1 - cw; nb2 = (1 - cw) / 2; a0 = 1 + alpha; na1 = -2 * cw; na2 = 1 - alpha;
        break;
      case Type::highPass:
        nb0 = (1 + cw) / 2; nb1 = -(1 + cw); nb2 = (1 + cw) / 2; a0 = 1 + alpha; na1 = -2 * cw; na2 = 1 - alpha;
        break;
      case Type::peak:
        nb0 = 1 + alpha * A; nb1 = -2 * cw; nb2 = 1 - alpha * A;
        a0 = 1 + alpha / A; na1 = -2 * cw; na2 = 1 - alpha / A;
        break;
      case Type::lowShelf: {
        const double s = 2 * std::sqrt(A) * alpha;
        nb0 = A * ((A + 1) - (A - 1) * cw + s);
        nb1 = 2 * A * ((A - 1) - (A + 1) * cw);
        nb2 = A * ((A + 1) - (A - 1) * cw - s);
        a0 = (A + 1) + (A - 1) * cw + s;
        na1 = -2 * ((A - 1) + (A + 1) * cw);
        na2 = (A + 1) + (A - 1) * cw - s;
        break;
      }
      case Type::highShelf: {
        const double s = 2 * std::sqrt(A) * alpha;
        nb0 = A * ((A + 1) + (A - 1) * cw + s);
        nb1 = -2 * A * ((A - 1) + (A + 1) * cw);
        nb2 = A * ((A + 1) + (A - 1) * cw - s);
        a0 = (A + 1) - (A - 1) * cw + s;
        na1 = 2 * ((A - 1) - (A + 1) * cw);
        na2 = (A + 1) - (A - 1) * cw - s;
        break;
      }
    }
    b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0;
  }

  // |H| at `hz` (for drawing curves).
  double magnitude(double fs, double hz) const {
    const double w = juce::MathConstants<double>::twoPi * hz / fs;
    const std::complex<double> z1c = std::polar(1.0, -w), z2c = std::polar(1.0, -2 * w);
    return std::abs((b0 + b1 * z1c + b2 * z2c) / (1.0 + a1 * z1c + a2 * z2c));
  }
};

// Linkwitz-Riley 4th order (two cascaded Butterworth 2nd-order sections)
// low-pass and high-pass at the same frequency. LP + HP is an all-pass.
struct LR4Split {
  std::array<Biquad, 2> lp, hp;
  double hz = 0, fs = 0;

  void reset() { for (auto* s : {&lp[0], &lp[1], &hp[0], &hp[1]}) s->reset(); }
  void set(double sampleRate, double cutoff) {
    if (cutoff == hz && sampleRate == fs) return;
    hz = cutoff;
    fs = sampleRate;
    for (auto& s : lp) s.set(Biquad::Type::lowPass, fs, hz, 0.70710678);
    for (auto& s : hp) s.set(Biquad::Type::highPass, fs, hz, 0.70710678);
  }
  void process(float x, float& low, float& high) {
    low = lp[1].process(lp[0].process(x));
    high = hp[1].process(hp[0].process(x));
  }

  // Analytic magnitudes, for the display.
  static double lowMag(double f, double fc) { const double r = std::pow(f / fc, 4.0); return 1.0 / (1.0 + r); }
  static double highMag(double f, double fc) { const double r = std::pow(f / fc, 4.0); return r / (1.0 + r); }
};

// Low | mid | high, phase-coherent: low = LP1 -> AP2, mid = HP1 -> LP2,
// high = HP1 -> HP2, where AP2 = LP2 + HP2 (a second LR4 pair at the upper
// frequency, summed).
class Crossover {
public:
  void prepare(double sampleRate) { fs_ = sampleRate; reset(); }
  void reset() { split1_.reset(); split2_.reset(); allpass2_.reset(); }
  void setFrequencies(double lowHz, double highHz) {
    highHz = std::max(highHz, lowHz * 1.5);
    split1_.set(fs_, lowHz);
    split2_.set(fs_, highHz);
    allpass2_.set(fs_, highHz);
  }
  void process(float x, float& low, float& mid, float& high) {
    float l, rest;
    split1_.process(x, l, rest);
    float apLow, apHigh;
    allpass2_.process(l, apLow, apHigh);
    low = apLow + apHigh;
    split2_.process(rest, mid, high);
  }

private:
  double fs_ = 48000.0;
  LR4Split split1_, split2_, allpass2_;
};

// Downward expander / gate on the DI. threshold <= -90 dB means off.
class Gate {
public:
  void prepare(double fs) {
    fs_ = fs;
    envRelease_ = onePole(fs, 40.0);
    gainAttack_ = onePole(fs, 0.8);
    gainRelease_ = onePole(fs, 60.0);
    env_ = 0.0f;
    gain_ = 1.0f;
  }
  void setThresholdDb(float db) { thresholdDb_ = db; }
  float process(float x) {
    if (thresholdDb_ <= -89.5f) return x;
    const float a = std::abs(x);
    env_ = a > env_ ? a : a + envRelease_ * (env_ - a);
    const float envDb = gainToDb(env_);
    // Open above the threshold; 4 dB of hysteresis so it doesn't chatter.
    if (envDb > thresholdDb_) open_ = true;
    else if (envDb < thresholdDb_ - 4.0f) open_ = false;
    const float target = open_ ? 1.0f : 0.0f;
    gain_ = target > gain_ ? target + gainAttack_ * (gain_ - target) : target + gainRelease_ * (gain_ - target);
    return x * gain_;
  }
  bool isOpen() const { return thresholdDb_ <= -89.5f || open_; }

private:
  double fs_ = 48000.0;
  float thresholdDb_ = -90.0f, env_ = 0.0f, gain_ = 1.0f;
  float envRelease_ = 0.0f, gainAttack_ = 0.0f, gainRelease_ = 0.0f;
  bool open_ = false;
};

// The low band's compressor. One knob: amount 0..1 lowers the threshold
// (-6 to -36 dB) and raises the ratio (2:1 to 8:1); makeup restores about
// up to +6 dB.
class Compressor {
public:
  void prepare(double fs) {
    attack_ = onePole(fs, 8.0);
    release_ = onePole(fs, 140.0);
    detect_ = onePole(fs, 5.0);
    ms_ = 0.0f;
    grDb_ = 0.0f;
  }
  void setAmount(float amount01) {
    amount_ = juce::jlimit(0.0f, 1.0f, amount01);
    thresholdDb_ = -6.0f - 30.0f * amount_;
    ratio_ = 2.0f + 6.0f * amount_;
    makeupDb_ = 6.0f * amount_;
  }
  float process(float x) {
    if (amount_ <= 0.0f) {
      grDb_ = 0.0f;
      return x;
    }
    ms_ = x * x + detect_ * (ms_ - x * x);
    const float levelDb = 10.0f * std::log10(std::max(ms_, 1.0e-12f)) + 3.0f;  // RMS -> peak-ish
    const float over = levelDb - thresholdDb_;
    const float targetGr = over > 0.0f ? over * (1.0f - 1.0f / ratio_) : 0.0f;
    grDb_ = targetGr > grDb_ ? targetGr + attack_ * (grDb_ - targetGr) : targetGr + release_ * (grDb_ - targetGr);
    return x * dbToGain(makeupDb_ - grDb_);
  }
  float reductionDb() const { return grDb_; }

private:
  float amount_ = 0.0f, thresholdDb_ = -6.0f, ratio_ = 2.0f, makeupDb_ = 0.0f;
  float attack_ = 0.0f, release_ = 0.0f, detect_ = 0.0f, ms_ = 0.0f, grDb_ = 0.0f;
};

// Bass / Mid / Treble for one band.
class BandEq {
public:
  static constexpr double kBassHz = 100.0, kMidHz = 650.0, kTrebleHz = 3000.0;
  void prepare(double fs) { fs_ = fs; for (auto& b : bq_) b.reset(); set(0, 0, 0); }
  void set(float bassDb, float midDb, float trebleDb) {
    if (bassDb == bass_ && midDb == mid_ && trebleDb == treble_) return;
    bass_ = bassDb; mid_ = midDb; treble_ = trebleDb;
    bq_[0].set(Biquad::Type::lowShelf, fs_, kBassHz, 0.7071, bass_);
    bq_[1].set(Biquad::Type::peak, fs_, kMidHz, 0.8, mid_);
    bq_[2].set(Biquad::Type::highShelf, fs_, kTrebleHz, 0.7071, treble_);
    flat_ = bass_ == 0.0f && mid_ == 0.0f && treble_ == 0.0f;
  }
  float process(float x) {
    if (flat_) return x;
    return bq_[2].process(bq_[1].process(bq_[0].process(x)));
  }

private:
  double fs_ = 48000.0;
  std::array<Biquad, 3> bq_;
  float bass_ = 1e9f, mid_ = 1e9f, treble_ = 1e9f;
  bool flat_ = true;
};

// Six peaking bands, Parallax-style centres.
class GraphicEq {
public:
  static constexpr std::array<double, 6> kHz{80.0, 250.0, 500.0, 1000.0, 2500.0, 6000.0};
  static constexpr double kQ = 1.1;
  void prepare(double fs) { fs_ = fs; for (auto& b : bq_) b.reset(); gains_.fill(1e9f); }
  void set(int band, float db) {
    if (gains_[static_cast<size_t>(band)] == db) return;
    gains_[static_cast<size_t>(band)] = db;
    bq_[static_cast<size_t>(band)].set(Biquad::Type::peak, fs_, kHz[static_cast<size_t>(band)], kQ, db);
  }
  float process(float x) {
    for (size_t i = 0; i < bq_.size(); ++i)
      if (gains_[i] != 0.0f) x = bq_[i].process(x);
    return x;
  }

private:
  double fs_ = 48000.0;
  std::array<Biquad, 6> bq_;
  std::array<float, 6> gains_{};
};

// A smoothed gain (linear ramp per block).
struct SmoothGain {
  float current = 1.0f;
  void process(float* x, int n, float target) {
    if (n <= 0) return;
    const float step = (target - current) / static_cast<float>(n);
    for (int i = 0; i < n; ++i) {
      current += step;
      x[i] *= current;
    }
    current = target;
  }
};

}  // namespace parabola
