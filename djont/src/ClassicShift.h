// dJONt WHAM's CLASSIC engine: the old-school two-tap delay-line pitch
// shifter, the method the first-generation whammy-style pedals were built on.
// Two read taps sweep through a short delay line at the pitch ratio, half a
// cycle apart, each faded in and out (sin^2, so they always sum to one) as
// it wraps. On a single note that is a clean shift; on chords and double
// stops the taps cross different points of each wave, which is the
// "glitchy" warble the classic sound is known for.
//
// Like the original pedals it tracks the pitch of what you play (a small
// YIN detector on a 4x-decimated copy, 100 times a second) and sizes the
// sweep to a whole number of periods, so on a single note both taps stay in
// step and the shift is clean; a chord has no single period, so the taps
// drift against each other and it warbles. GRAIN sets the sweep size it
// aims for: short is tighter and more metallic on chords, long is smoother
// and wobblier. No splice search: what it does to chords is the point.
//
// Delay: the taps average (kMinMs + grain / 2). Real-time safe after
// prepare().
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <vector>

namespace djont {

class ClassicShift {
public:
  static constexpr float kMinMs = 1.0f, kMaxGrainMs = 80.0f;

  void prepare(double fs) {
    fs_ = fs;
    size_ = static_cast<int>(fs * (kMaxGrainMs * 1.6f + kMinMs + 10.0f) * 0.001) + 4;
    buffer_.assign(static_cast<size_t>(size_), 0.0f);
    dec_.assign(kDecSize, 0.0f);
    diff_.assign(kMaxLag + 2, 0.0f);
    hop_ = juce::jmax(1, static_cast<int>(fs / 100.0));
    grainSmooth_ = static_cast<float>(1.0 - std::exp(-1.0 / (fs * 0.03)));
    reset();
  }

  void reset() {
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    std::fill(dec_.begin(), dec_.end(), 0.0f);
    write_ = decWrite_ = decCount_ = sinceHop_ = 0;
    decAcc_ = 0.0f;
    phase_ = 0.0;
    period_ = 0.0;
    grain_ = target_;
  }

  void setGrainMs(float ms) {
    targetMs_ = juce::jlimit(5.0, static_cast<double>(kMaxGrainMs), static_cast<double>(ms));
    retarget();
  }

  static int latencySamples(float grainMs, double fs) {
    return static_cast<int>(std::lround((kMinMs + grainMs * 0.5f) * 0.001 * fs));
  }

  // In place, at `semitones`.
  void process(float* x, int n, float semitones) {
    const double ratio = std::pow(2.0, static_cast<double>(semitones) / 12.0);
    const double minDelay = kMinMs * 0.001 * fs_;
    for (int i = 0; i < n; ++i) {
      track(x[i]);
      grain_ += (target_ - grain_) * grainSmooth_;
      buffer_[static_cast<size_t>(write_)] = x[i];
      phase_ += (1.0 - ratio) / grain_;  // per sample
      phase_ -= std::floor(phase_);
      const double p2 = phase_ + 0.5 - std::floor(phase_ + 0.5);
      const float a = read(minDelay + phase_ * grain_), b = read(minDelay + p2 * grain_);
      const float ga = static_cast<float>(std::sin(juce::MathConstants<double>::pi * phase_));
      const float gb = static_cast<float>(std::sin(juce::MathConstants<double>::pi * p2));
      x[i] = a * ga * ga + b * gb * gb;
      write_ = (write_ + 1) % size_;
    }
  }

private:
  static constexpr int kDecim = 4, kDecSize = 1024, kWindow = 512, kMinLag = 12, kMaxLag = 300;

  // The sweep: GRAIN snapped to an even number of periods (so the taps,
  // half a sweep apart, are a whole number of periods apart).
  void retarget() {
    const double want = targetMs_ * 0.001 * fs_;
    if (period_ > 0.0) {
      const double pairs = juce::jmax(1.0, std::round(want / (2.0 * period_)));
      target_ = juce::jmin(2.0 * period_ * pairs, static_cast<double>(kMaxGrainMs) * 1.5 * 0.001 * fs_);
    } else {
      target_ = want;
    }
  }

  void track(float sample) {
    decAcc_ += sample;
    if (++decCount_ == kDecim) {
      dec_[static_cast<size_t>(decWrite_)] = decAcc_ / kDecim;
      decWrite_ = (decWrite_ + 1) % kDecSize;
      decAcc_ = 0.0f;
      decCount_ = 0;
    }
    if (++sinceHop_ >= hop_) {
      sinceHop_ = 0;
      detect();
    }
  }

  // YIN: the lag where the signal best repeats (cumulative-mean-normalised
  // difference under a threshold). Quiet or unpitched: keep the last period.
  void detect() {
    const int start = (decWrite_ - kWindow - kMaxLag + 2 * kDecSize) % kDecSize;
    const auto at = [&](int i) { return dec_[static_cast<size_t>((start + i) % kDecSize)]; };
    float energy = 0.0f;
    for (int i = 0; i < kWindow; ++i) energy += at(i) * at(i);
    if (energy < 1e-4f) return;
    float running = 0.0f;
    int found = -1;
    for (int lag = 1; lag <= kMaxLag; ++lag) {
      float d = 0.0f;
      for (int i = 0; i < kWindow; ++i) {
        const float e = at(i) - at(i + lag);
        d += e * e;
      }
      running += d;
      const float norm = running > 0.0f ? d * static_cast<float>(lag) / running : 1.0f;
      diff_[static_cast<size_t>(lag)] = norm;
      if (lag > kMinLag + 1 && found < 0 && diff_[static_cast<size_t>(lag - 1)] < 0.15f &&
          diff_[static_cast<size_t>(lag - 1)] <= norm)
        found = lag - 1;
      if (found >= 0) break;
    }
    if (found < 0) return;
    // Parabolic refinement around the minimum.
    const float a = diff_[static_cast<size_t>(found - 1)], b = diff_[static_cast<size_t>(found)],
                c = diff_[static_cast<size_t>(found + 1)];
    const float den = a - 2.0f * b + c;
    const double lag = found + (std::abs(den) > 1e-9f ? 0.5 * (a - c) / den : 0.0);
    period_ = lag * kDecim;
    retarget();
  }

  // Linear interpolation `delay` samples behind the newest.
  float read(double delay) const {
    double pos = static_cast<double>(write_) - delay;
    while (pos < 0) pos += size_;
    const int i0 = static_cast<int>(pos);
    const float frac = static_cast<float>(pos - i0);
    const float s0 = buffer_[static_cast<size_t>(i0 % size_)], s1 = buffer_[static_cast<size_t>((i0 + 1) % size_)];
    return s0 + frac * (s1 - s0);
  }

  double fs_ = 48000.0, phase_ = 0.0, grain_ = 1440.0, target_ = 1440.0, targetMs_ = 30.0, period_ = 0.0;
  float grainSmooth_ = 0.001f, decAcc_ = 0.0f;
  std::vector<float> buffer_, dec_, diff_;
  int size_ = 1, write_ = 0, decWrite_ = 0, decCount_ = 0, sinceHop_ = 0, hop_ = 480;
};

}  // namespace djont
