// The RACK: three units on the DI, after Hush, the gate and Pitch, before
// the crossover.
//   DI SHAPER   seven peaking bands (50 Hz to 3.2 kHz) to shape the DI that
//               every band and capture hears.
//   SUB COMP    a compressor on only the lows below FREQ, blended back in:
//               a steady bottom without squashing the attack above it.
//   SPLIT DRIVE a built-in drive on everything above SPLIT; below it stays
//               clean. Oversampled 4x so the drive stays smooth up top.
// SOLO plays the rack's output alone (no bands, cab or EQ), to dial the DI.
// Runs at the internal 48 kHz and adds no latency.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <memory>
#include <vector>

#include "Dsp.h"

namespace parabola {

class DiShaper {
public:
  static constexpr int kBands = 7;
  static constexpr std::array<double, kBands> kHz{50.0, 100.0, 200.0, 400.0, 800.0, 1600.0, 3200.0};
  void prepare(double fs) {
    fs_ = fs;
    for (auto& b : bq_) b.reset();
    db_.fill(1000.0f);
  }
  void set(int band, float db) {
    const auto i = static_cast<size_t>(band);
    if (db == db_[i]) return;
    db_[i] = db;
    bq_[i].set(Biquad::Type::peak, fs_, kHz[i], 1.1, db);
  }
  bool flat() const {
    for (float d : db_) if (std::abs(d) > 0.01f) return false;
    return true;
  }
  float process(float x) {
    for (size_t i = 0; i < kBands; ++i)
      if (std::abs(db_[i]) > 0.01f) x = bq_[i].process(x);
    return x;
  }

private:
  double fs_ = 48000.0;
  std::array<Biquad, kBands> bq_;
  std::array<float, kBands> db_{};
};

// Compresses only below `hz`: LR4 split, compressor on the low part, the
// compressed lows blended against the dry lows, highs untouched. The split
// always runs (LP + HP is an all-pass), so switching it on and off doesn't
// shift the phase of the rest.
class SubComp {
public:
  void prepare(double fs) {
    fs_ = fs;
    split_.reset();
    comp_.prepare(fs);
    mix_.reset(fs, 0.02);
    mix_.setCurrentAndTargetValue(0.0f);
  }
  void set(bool on, float amount01, float hz, float blend01) {
    split_.set(fs_, hz);
    comp_.setAmount(amount01);
    mix_.setTargetValue(on ? blend01 : 0.0f);
  }
  float process(float x) {
    float lo, hi;
    split_.process(x, lo, hi);
    const float c = comp_.process(lo);
    const float m = mix_.getNextValue();
    return hi + lo + m * (c - lo);
  }
  float reductionDb() const { return comp_.reductionDb(); }

private:
  double fs_ = 48000.0;
  LR4Split split_;
  Compressor comp_;
  juce::SmoothedValue<float> mix_;
};

// Clean below SPLIT, driven above it. Drive 0..1 sets 0 to +40 dB into an
// asymmetric soft clipper (4x oversampled); Tone is a low-pass on the drive;
// Blend mixes driven against clean highs; Level trims the drive.
class SplitDrive {
public:
  void prepare(double fs, int maxBlock) {
    fs_ = fs;
    os_ = std::make_unique<juce::dsp::Oversampling<float>>(1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                                                         true, false);
    os_->initProcessing(static_cast<size_t>(juce::jmax(16, maxBlock)));
    os_->reset();
    // The oversampling filters delay the drive by a couple of samples. Not
    // compensated: the drive is all new harmonics, and 3 samples at 48 kHz is
    // under 10 degrees at the split, so Parabola keeps zero added latency.
    delay_ = 0;
    lo_.assign(static_cast<size_t>(maxBlock), 0.0f);
    hi_.assign(static_cast<size_t>(maxBlock), 0.0f);
    wet_.assign(static_cast<size_t>(maxBlock), 0.0f);
    for (auto& r : rings_) r.assign(static_cast<size_t>(juce::jmax(1, delay_)), 0.0f);
    pos_ = 0;
    split_.reset();
    for (auto& t : tone_) t.reset();
    pre_.reset();
    preHp_.reset();
    postHp_.reset();
    splitHz_ = -1.0f;
    dcX_ = dcY_ = 0.0f;
    dcR_ = 1.0f - static_cast<float>(juce::MathConstants<double>::twoPi * 8.0 / fs);
    mix_.reset(fs, 0.02);
    mix_.setCurrentAndTargetValue(0.0f);
    toneHz_ = -1.0f;
  }
  int latency() const { return delay_; }
  void set(bool on, float drive01, float splitHz, float toneHz, float blend01, float levelDb) {
    split_.set(fs_, splitHz);
    drive_ = dbToGain(drive01 * 40.0f);
    level_ = dbToGain(levelDb);
    on_ = on;
    mix_.setTargetValue(on ? blend01 : 0.0f);
    if (splitHz != splitHz_) {
      splitHz_ = splitHz;
      preHp_.set(Biquad::Type::highPass, fs_, splitHz, 0.7071);  // keep the lows out of the clipper
      postHp_.set(Biquad::Type::highPass, fs_, splitHz * 0.7, 0.7071);  // and out of what it makes
    }
    if (toneHz != toneHz_) {
      toneHz_ = toneHz;
      for (auto& t : tone_) t.set(Biquad::Type::lowPass, fs_, toneHz, 0.7071);
      pre_.set(Biquad::Type::peak, fs_, 900.0, 0.7, 3.0);  // a little mid push into the clipper
    }
  }
  bool idle() const { return !on_ && !mix_.isSmoothing() && mix_.getCurrentValue() <= 0.0f; }
  void process(float* x, int n) {
    for (int i = 0; i < n; ++i) split_.process(x[i], lo_[static_cast<size_t>(i)], hi_[static_cast<size_t>(i)]);
    if (idle()) {
      for (int i = 0; i < n; ++i) {
        const auto si = static_cast<size_t>(i);
        const float low = push(0, lo_[si]), clean = push(1, hi_[si]);
        step();
        x[i] = low + clean;
      }
      return;
    }
    // drive the highs at 4x
    for (int i = 0; i < n; ++i) wet_[static_cast<size_t>(i)] = pre_.process(preHp_.process(hi_[static_cast<size_t>(i)]));
    float* chans[1] = {wet_.data()};
    juce::dsp::AudioBlock<float> block(chans, 1, static_cast<size_t>(n));
    auto up = os_->processSamplesUp(block);
    float* u = up.getChannelPointer(0);
    constexpr float kCeil = 0.22f, kBias = 0.25f;
    const float off = std::tanh(kBias);
    for (size_t i = 0; i < up.getNumSamples(); ++i) u[i] = kCeil * (std::tanh(drive_ * u[i] / kCeil + kBias) - off);
    os_->processSamplesDown(block);
    for (int i = 0; i < n; ++i) {
      const auto si = static_cast<size_t>(i);
      const float d0 = wet_[si];
      const float dc = d0 - dcX_ + dcR_ * dcY_;  // the bias leaves DC behind
      dcX_ = d0;
      dcY_ = dc;
      const float d = postHp_.process(tone_[1].process(tone_[0].process(dc))) * level_;
      const float low = push(0, lo_[si]), clean = push(1, hi_[si]);
      step();
      const float m = mix_.getNextValue();
      x[i] = low + clean + m * (d - clean);
    }
  }

private:
  // Lows and clean highs wait `delay_` samples so they line up with the
  // oversampled drive.
  float push(int line, float v) {
    if (delay_ == 0) return v;
    auto& r = rings_[static_cast<size_t>(line)];
    const float out = r[static_cast<size_t>(pos_)];
    r[static_cast<size_t>(pos_)] = v;
    return out;
  }
  void step() { if (delay_ > 0) pos_ = (pos_ + 1) % delay_; }

  double fs_ = 48000.0;
  std::unique_ptr<juce::dsp::Oversampling<float>> os_;
  int delay_ = 0, pos_ = 0;
  bool on_ = false;
  std::vector<float> lo_, hi_, wet_;
  std::array<std::vector<float>, 2> rings_;
  LR4Split split_;
  std::array<Biquad, 2> tone_;
  Biquad pre_, preHp_, postHp_;
  float drive_ = 1.0f, level_ = 1.0f, toneHz_ = -1.0f, splitHz_ = -1.0f;
  float dcX_ = 0.0f, dcY_ = 0.0f, dcR_ = 0.999f;
  juce::SmoothedValue<float> mix_;
};

}  // namespace parabola
