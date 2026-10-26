// HUSH: a zero-latency de-noiser for a guitar or bass DI, shared by dJONt,
// Parabola Bass Z and the Hush plugin. Mono, real-time safe, no lookahead.
//
// The noise a pickup brings (hiss, EMI buzz) is spread across the whole
// spectrum and never stops; a note fills only some of it, and less and less
// of it as it rings out. HUSH watches kBands bands (200 Hz to 10 kHz) on
// their own:
//   - each band has a detector (a band-pass, or a high-pass for the top one,
//     and an RMS follower) and a
//     threshold: that band's noise floor plus Sensitivity;
//   - a band with your note in it (6 dB or more over its threshold) stays
//     open; a band with only noise in it is turned down, by up to Strength;
//   - the turning down is a bell EQ cut at that band (a dynamic EQ), so
//     with every band open the signal is untouched, bit for bit.
// Bands open within about two milliseconds (pick attacks get through) and
// close over Release. So noise is cut between notes and also under them, in
// the parts of the spectrum the note isn't covering.
//
// LEARN: with your hands off the strings, it listens for about two seconds
// and sets each band's noise floor from what it hears. Nothing is delayed
// or subtracted; learning only sets the thresholds. Unlearned, every band
// assumes kDefaultFloorDb.
//
// Below 200 Hz nothing changes (that's where a bass's notes live, and mains
// hum is a job for the gates and both pickups at full volume).
//
// Switching on or off crossfades over 20 ms; off and settled, it's a
// passthrough. All original code.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

class Hush {
public:
  static constexpr int kBands = 8;
  // Bell centres; the top band is a high shelf from kHz.back() up, so the
  // whole top of the spectrum is covered.
  static constexpr std::array<double, kBands> kHz{200.0, 350.0, 600.0, 1000.0, 1800.0, 3200.0, 5600.0, 9000.0};
  static constexpr double kQ = 1.2;
  static constexpr float kMaxCutDb = 30.0f;
  static constexpr float kDefaultFloorDb = -78.0f;
  static constexpr double kLearnSeconds = 2.0;

  Hush() {
    for (auto& f : floorDb) f.store(kDefaultFloorDb);
    for (auto& r : reduction) r.store(0.0f);
  }

  void prepare(double sampleRate) {
    fs_ = sampleRate;
    // Detectors are 4th order (two sections) so a band hears mostly its own
    // part of the spectrum, not a loud neighbour.
    for (int b = 0; b < kBands - 1; ++b)
      for (auto* d : {&detect_[b], &detect2_[b]}) d->setBandPass(fs_, kHz[b], kQ);
    for (auto* d : {&detect_[kBands - 1], &detect2_[kBands - 1]}) d->setHighPass(fs_, kHz[kBands - 1] * 0.8);
    envAttack_ = coeff(1.0);
    envRelease_ = coeff(30.0);
    openDbPerSample_ = static_cast<float>(kMaxCutDb / (0.001 * fs_));  // fully open in 1 ms
    mixStep_ = static_cast<float>(1.0 / (0.02 * fs_));
    reset();
  }

  void reset() {
    for (int b = 0; b < kBands; ++b) {
      detect_[b].reset();
      detect2_[b].reset();
      cut_[b].reset();
      tune(b, 0.0);
      env_[b] = 0.0f;
      gainDb_[b] = 0.0f;
      appliedDb_[b] = 0.0f;
    }
  }

  // strength 0..1, sensitivity in dB above each band's floor, release in ms.
  void set(bool on, float strength, float sensitivityDb, float releaseMs) {
    on_ = on;
    depthDb_ = kMaxCutDb * std::clamp(strength, 0.0f, 1.0f);
    sensitivityDb_ = sensitivityDb;
    closeDbPerSample_ = (depthDb_ + 1e-3f) / static_cast<float>(std::max(1.0, releaseMs * 0.001 * fs_));
  }

  bool active() const { return on_ || mix_ > 0.0f || learnLeft_.load() > 0; }

  // LEARN (message thread starts it, the audio thread does it).
  void startLearn() {
    for (auto& s : learnSum_) s = 0.0;
    learnCount_ = 0;
    learnLeft_.store(static_cast<int>(kLearnSeconds * fs_));
  }
  bool learning() const { return learnLeft_.load() > 0; }
  float learnProgress() const {
    return 1.0f - static_cast<float>(learnLeft_.load()) / static_cast<float>(std::max(1.0, kLearnSeconds * fs_));
  }
  std::atomic<int> learnedCount{0};  // bumps when a LEARN finishes

  // Each band's noise floor (dBFS RMS in the band), learned or restored.
  std::array<std::atomic<float>, kBands> floorDb;
  // How far each band is turned down right now (dB, >= 0), for meters.
  std::array<std::atomic<float>, kBands> reduction;

  void process(float* x, int n) {
    if (!active()) return;
    const bool learn = learnLeft_.load(std::memory_order_relaxed) > 0;
    for (int i = 0; i < n; ++i) {
      const float in = x[i];
      // Detect per band every sample; decide every 4 (the log is the cost).
      const bool decide = (tick_ & 3) == 0;
      for (int b = 0; b < kBands; ++b) {
        const float d = detect2_[b].process(detect_[b].process(in));
        const float p = d * d;
        env_[b] = p > env_[b] ? p + envAttack_ * (env_[b] - p) : p + envRelease_ * (env_[b] - p);
        if (!decide) continue;
        const float levelDb = 10.0f * std::log10(env_[b] + 1e-20f);
        const float threshold = floorDb[b].load(std::memory_order_relaxed) + sensitivityDb_;
        // Soft knee: fully cut at the threshold, fully open 6 dB over it.
        const float over = std::clamp((levelDb - threshold) / 6.0f, 0.0f, 1.0f);
        const float target = -depthDb_ * (1.0f - over);
        gainDb_[b] = target > gainDb_[b] ? std::min(target, gainDb_[b] + 4.0f * openDbPerSample_)
                                         : std::max(target, gainDb_[b] - 4.0f * closeDbPerSample_);
        if (learn && (tick_ & 31) == 0) learnSum_[b] += levelDb;
      }
      if (learn && (tick_ & 31) == 0) ++learnCount_;

      // Re-tune the cuts every 16 samples (cheap; the bands move slowly).
      if ((tick_ & 15) == 0)
        for (int b = 0; b < kBands; ++b) {
          const float want = gainDb_[b] * mix_;
          if (std::abs(want - appliedDb_[b]) > 0.05f || (want == 0.0f && appliedDb_[b] != 0.0f)) {
            appliedDb_[b] = want;
            tune(b, want);
          }
        }
      ++tick_;

      // A bell at 0 dB is an exact identity, so open bands change nothing.
      float y = in;
      for (int b = 0; b < kBands; ++b) y = cut_[b].process(y);
      mix_ = on_ ? std::min(1.0f, mix_ + mixStep_) : std::max(0.0f, mix_ - mixStep_);
      x[i] = y;
    }
    for (int b = 0; b < kBands; ++b) reduction[b].store(-appliedDb_[b], std::memory_order_relaxed);

    // Finish a LEARN: each floor is the band's average level plus 3 dB.
    if (int left = learnLeft_.load(); left > 0) {
      left = std::max(0, left - n);
      learnLeft_.store(left);
      if (left == 0 && learnCount_ > 0) {
        for (int b = 0; b < kBands; ++b)
          floorDb[b].store(static_cast<float>(learnSum_[b] / learnCount_) + 3.0f);
        learnedCount.fetch_add(1);
      }
    }
  }

private:
  // RBJ biquad, transposed direct form II.
  struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    float process(float xf) {
      const double xd = xf, y = b0 * xd + z1;
      z1 = b1 * xd - a1 * y + z2;
      z2 = b2 * xd - a2 * y;
      return static_cast<float>(y);
    }
    void setBandPass(double fs, double hz, double q) {  // 0 dB peak
      const double w = 2.0 * 3.14159265358979323846 * hz / fs, cw = std::cos(w), alpha = std::sin(w) / (2.0 * q);
      const double a0 = 1 + alpha;
      b0 = alpha / a0; b1 = 0; b2 = -alpha / a0; a1 = -2 * cw / a0; a2 = (1 - alpha) / a0;
    }
    void setHighPass(double fs, double hz) {
      const double w = 2.0 * 3.14159265358979323846 * hz / fs, cw = std::cos(w), alpha = std::sin(w) / (2.0 * 0.70710678);
      const double a0 = 1 + alpha;
      b0 = (1 + cw) / 2 / a0; b1 = -(1 + cw) / a0; b2 = (1 + cw) / 2 / a0; a1 = -2 * cw / a0; a2 = (1 - alpha) / a0;
    }
    void setHighShelf(double fs, double hz, double gainDb) {  // S = 1
      const double w = 2.0 * 3.14159265358979323846 * hz / fs, cw = std::cos(w);
      const double A = std::pow(10.0, gainDb / 40.0), alpha = std::sin(w) / 2.0 * std::sqrt(2.0);
      const double s = 2 * std::sqrt(A) * alpha, a0 = (A + 1) - (A - 1) * cw + s;
      b0 = A * ((A + 1) + (A - 1) * cw + s) / a0;
      b1 = -2 * A * ((A - 1) + (A + 1) * cw) / a0;
      b2 = A * ((A + 1) + (A - 1) * cw - s) / a0;
      a1 = 2 * ((A - 1) - (A + 1) * cw) / a0;
      a2 = ((A + 1) - (A - 1) * cw - s) / a0;
    }
    void setPeak(double fs, double hz, double q, double gainDb) {
      const double w = 2.0 * 3.14159265358979323846 * hz / fs, cw = std::cos(w), alpha = std::sin(w) / (2.0 * q);
      const double A = std::pow(10.0, gainDb / 40.0), a0 = 1 + alpha / A;
      b0 = (1 + alpha * A) / a0; b1 = -2 * cw / a0; b2 = (1 - alpha * A) / a0; a1 = -2 * cw / a0; a2 = (1 - alpha / A) / a0;
    }
  };

  void tune(int b, double gainDb) {
    if (b == kBands - 1) cut_[b].setHighShelf(fs_, kHz[b] * 0.8, gainDb);
    else cut_[b].setPeak(fs_, kHz[b], kQ, gainDb);
  }

  float coeff(double ms) const { return static_cast<float>(std::exp(-1.0 / (fs_ * ms * 0.001))); }

  double fs_ = 48000.0;
  std::array<Biquad, kBands> detect_, detect2_, cut_;
  std::array<float, kBands> env_{}, gainDb_{}, appliedDb_{};
  std::array<double, kBands> learnSum_{};
  int learnCount_ = 0;
  std::atomic<int> learnLeft_{0};
  unsigned tick_ = 0;
  bool on_ = false;
  float mix_ = 0.0f, mixStep_ = 0.001f;
  float envAttack_ = 0.0f, envRelease_ = 0.0f, openDbPerSample_ = 1.0f, closeDbPerSample_ = 0.01f;
  float depthDb_ = 18.0f, sensitivityDb_ = 6.0f;
};
