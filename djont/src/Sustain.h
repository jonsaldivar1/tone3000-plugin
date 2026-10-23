// dJONt SUSTAIN: two ways to keep notes ringing.
//
//   Hold       a spectral freeze, the guitar version of a piano's sustain
//              pedals. While HOLD is down, what was ringing when you pressed
//              it keeps sounding as a seamless layer under whatever you play:
//                CHORD    (sostenuto, the piano's middle pedal) captures the
//                         chord at the press and holds just that;
//                PILE-UP  (damper, the right pedal) keeps adding: every note
//                         you play while it's down joins the held layer.
//              FADE sets how fast the layer swells in and dies away on
//              release; LEVEL its volume against the live guitar. The live
//              signal passes untouched.
//
//              Engine: a phase-vocoder freeze. The input is analysed in
//              4096-point frames every 1024 samples; at the press each bin's
//              magnitude and its phase advance per frame are captured, and
//              the layer is resynthesised from them with the phases running
//              on, so a held chord keeps its pitches instead of turning to
//              noise.
//
//   Sustainer  the sound of a Sustainiac / Fernandes sustainer pickup. Those
//              drive the string itself with a magnet, so it never decays;
//              software can't touch the string, so this keeps the note at
//              the level you picked it at as the string dies away (up to
//              SUSTAIN dB of make-up gain), and in HARMONIC it blooms into the
//              octave over BLOOM seconds, the way a sustainer's harmonic mode
//              jumps to an overtone. MIX blooms halfway. Below the noise floor
//              it lets go, so silence stays silent. Best on single notes,
//              like the real thing.
//
// All original code. Real-time safe after prepare().
#pragma once

#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <vector>

#include "Dsp.h"
#include "PitchShift.h"

namespace djont {

// ---------------------------------------------------------------- Hold
class Hold {
public:
  enum class Type { chord, pileUp };
  static constexpr int kOrder = 12, kSize = 1 << kOrder, kHop = kSize / 4, kBins = kSize / 2 + 1;

  void prepare(double fs) {
    fs_ = fs;
    fft_ = std::make_unique<juce::dsp::FFT>(kOrder);
    window_.resize(kSize);
    for (int i = 0; i < kSize; ++i)
      window_[static_cast<size_t>(i)] =
          0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * static_cast<float>(i) / kSize);
    in_.assign(kSize, 0.0f);
    out_.assign(kSize, 0.0f);
    work_.assign(2 * kSize, 0.0f);
    for (auto* v : {&mag_, &phase_, &prevPhase_, &delta_, &heldMag_, &heldDelta_, &synthPhase_})
      v->assign(kBins, 0.0f);
    reset();
  }

  void reset() {
    std::fill(in_.begin(), in_.end(), 0.0f);
    std::fill(out_.begin(), out_.end(), 0.0f);
    std::fill(heldMag_.begin(), heldMag_.end(), 0.0f);
    std::fill(mag_.begin(), mag_.end(), 0.0f);
    pos_ = 0;
    sinceHop_ = 0;
    gain_ = 0.0f;
    held_ = false;
    frames_ = 0;
  }

  // Voicing (the ADV page): SHIMMER jitters the held phases a little each
  // frame (movement, less of a static pad); LOW CUT / TONE filter the held
  // layer only.
  void setVoicing(float shimmer01, float lowCutHz, float toneHz) {
    shimmer_ = shimmer01;
    lowCut_.set(Biquad::Type::highPass, fs_, juce::jmax(20.0f, lowCutHz), 0.7071);
    tone_.set(Biquad::Type::lowPass, fs_, juce::jmin(toneHz, 19000.0f), 0.7071);
    lowCutOn_ = lowCutHz > 25.0f;
    toneOn_ = toneHz < 19000.0f;
  }

  void set(bool held, Type type, float fadeSeconds, float level01) {
    type_ = type;
    level_ = level01;
    fadeStep_ = 1.0f / static_cast<float>(juce::jmax(1.0, fs_ * juce::jmax(0.01f, fadeSeconds)));
    if (held && !held_) capture(true);
    held_ = held;
  }

  bool sounding() const { return held_ || gain_ > 0.0f; }

  // Adds the held layer to x (the live signal stays as it is).
  void process(float* x, int n) {
    for (int i = 0; i < n; ++i) {
      in_[static_cast<size_t>(pos_)] = x[i];
      const float layer = out_[static_cast<size_t>(pos_)];
      out_[static_cast<size_t>(pos_)] = 0.0f;
      pos_ = (pos_ + 1) % kSize;
      gain_ = held_ ? juce::jmin(1.0f, gain_ + fadeStep_) : juce::jmax(0.0f, gain_ - fadeStep_);
      float l = layer;
      if (lowCutOn_) l = lowCut_.process(l);
      if (toneOn_) l = tone_.process(l);
      x[i] += l * gain_ * level_;
      if (++sinceHop_ == kHop) {
        sinceHop_ = 0;
        analyse();
        if (held_ && type_ == Type::pileUp) capture(false);
        if (sounding()) synthesise();
      }
    }
  }

private:
  static float wrap(float p) {
    return p - juce::MathConstants<float>::twoPi * std::floor((p + juce::MathConstants<float>::pi) /
                                                             juce::MathConstants<float>::twoPi);
  }

  // The newest kSize input samples, windowed, to magnitudes and the phase
  // advance per hop (the bin's true frequency).
  void analyse() {
    for (int i = 0; i < kSize; ++i)
      work_[static_cast<size_t>(i)] = in_[static_cast<size_t>((pos_ + i) % kSize)] * window_[static_cast<size_t>(i)];
    std::fill(work_.begin() + kSize, work_.end(), 0.0f);
    fft_->performRealOnlyForwardTransform(work_.data(), true);
    for (int k = 0; k < kBins; ++k) {
      const float re = work_[static_cast<size_t>(2 * k)], im = work_[static_cast<size_t>(2 * k + 1)];
      const auto kk = static_cast<size_t>(k);
      mag_[kk] = std::sqrt(re * re + im * im);
      phase_[kk] = std::atan2(im, re);
      delta_[kk] = phase_[kk] - prevPhase_[kk];
      prevPhase_[kk] = phase_[kk];
    }
    ++frames_;
  }

  // At the press: take the spectrum. Pile-up, while held: keep the louder of
  // what's held and what's playing, per bin.
  void capture(bool fresh) {
    for (size_t k = 0; k < static_cast<size_t>(kBins); ++k) {
      if (fresh) {
        heldMag_[k] = mag_[k];
        heldDelta_[k] = delta_[k];
        synthPhase_[k] = phase_[k];
      } else if (mag_[k] > heldMag_[k]) {
        heldMag_[k] = mag_[k];
        heldDelta_[k] = delta_[k];
      }
    }
  }

  void synthesise() {
    std::fill(work_.begin(), work_.end(), 0.0f);
    for (int k = 0; k < kBins; ++k) {
      const auto kk = static_cast<size_t>(k);
      synthPhase_[kk] = wrap(synthPhase_[kk] + heldDelta_[kk] +
                             (shimmer_ > 0.0f ? shimmer_ * 1.2f * (random_.nextFloat() - 0.5f) : 0.0f));
      work_[static_cast<size_t>(2 * k)] = heldMag_[kk] * std::cos(synthPhase_[kk]);
      work_[static_cast<size_t>(2 * k + 1)] = heldMag_[kk] * std::sin(synthPhase_[kk]);
    }
    for (int k = 1; k < kSize / 2; ++k) {  // conjugate-symmetric upper half
      work_[static_cast<size_t>(2 * (kSize - k))] = work_[static_cast<size_t>(2 * k)];
      work_[static_cast<size_t>(2 * (kSize - k) + 1)] = -work_[static_cast<size_t>(2 * k + 1)];
    }
    fft_->performRealOnlyInverseTransform(work_.data());
    // Hann in and out at 4x overlap sums to 1.5.
    constexpr float kNorm = 1.0f / 1.5f;
    for (int i = 0; i < kSize; ++i)
      out_[static_cast<size_t>((pos_ + i) % kSize)] += work_[static_cast<size_t>(i)] * window_[static_cast<size_t>(i)] * kNorm;
  }

  double fs_ = 48000.0;
  std::unique_ptr<juce::dsp::FFT> fft_;
  std::vector<float> window_, in_, out_, work_;
  std::vector<float> mag_, phase_, prevPhase_, delta_, heldMag_, heldDelta_, synthPhase_;
  int pos_ = 0, sinceHop_ = 0, frames_ = 0;
  Type type_ = Type::chord;
  bool held_ = false;
  float gain_ = 0.0f, fadeStep_ = 0.001f, level_ = 0.8f, shimmer_ = 0.0f;
  Biquad lowCut_, tone_;
  bool lowCutOn_ = false, toneOn_ = false;
  juce::Random random_{20260922};
};

// ---------------------------------------------------------------- Sustainer
class Sustainer {
public:
  enum class Type { fundamental, harmonic, mix };
  static constexpr float kMaxBoostDb = 40.0f;
  static constexpr float kFloor = 0.0006f;  // about -64 dBFS: below this it lets go

  void prepare(double fs, int maxBlock) {
    fs_ = fs;
    shifter_.prepare(fs, maxBlock);
    PitchShift::Params p;
    p.semitones = interval_;
    p.window = PitchShift::Window::ms20;
    shifter_.setParams(p);
    scratch_.assign(static_cast<size_t>(juce::jmax(1, maxBlock)), 0.0f);
    envAttack_ = onePole(fs, 1.0);
    envRelease_ = onePole(fs, 60.0);
    slowAttack_ = onePole(fs, 40.0);
    slowRelease_ = onePole(fs, 250.0);
    gainSmooth_ = onePole(fs, static_cast<double>(responseMs_));
    reset();
  }

  void reset() {
    env_ = slow_ = 0.0f;
    gain_ = 1.0f;
    target_ = 0.0f;
    sinceOnset_ = 1 << 30;
    bloom_ = 0.0f;
  }

  // Voicing (the ADV page): the interval Harmonic blooms into, the FLOOR
  // below which it lets go, how fast it rides the level (RESPONSE), and a
  // TONE low-pass on its output.
  void setVoicing(float intervalSemitones, float floorDb, float responseMs, float toneHz) {
    if (!juce::exactlyEqual(intervalSemitones, interval_)) {
      interval_ = intervalSemitones;
      PitchShift::Params p;
      p.semitones = interval_;
      p.window = PitchShift::Window::ms20;
      shifter_.setParams(p);
    }
    floor_ = dbToGain(floorDb);
    if (!juce::exactlyEqual(responseMs, responseMs_)) {
      responseMs_ = responseMs;
      gainSmooth_ = onePole(fs_, juce::jmax(1.0f, responseMs));
    }
    tone_.set(Biquad::Type::lowPass, fs_, juce::jmin(toneHz, 19000.0f), 0.7071);
    toneOn_ = toneHz < 19000.0f;
  }

  void set(float amount01, float bloomSeconds, Type type, bool active) {
    maxGain_ = dbToGain(kMaxBoostDb * juce::jlimit(0.0f, 1.0f, amount01));
    bloomSeconds_ = juce::jmax(0.05f, bloomSeconds);
    type_ = type;
    shifter_.setEnabled(active && type != Type::fundamental);
  }

  void process(float* x, int n) {
    n = juce::jmin(n, static_cast<int>(scratch_.size()));
    const int captureSamples = static_cast<int>(fs_ * 0.03);
    for (int i = 0; i < n; ++i) {
      const float a = std::abs(x[i]);
      env_ = a > env_ ? envAttack_ * env_ + (1 - envAttack_) * a : envRelease_ * env_ + (1 - envRelease_) * a;
      slow_ = a > slow_ ? slowAttack_ * slow_ + (1 - slowAttack_) * a : slowRelease_ * slow_ + (1 - slowRelease_) * a;
      // A pick: start a new note, and take its level over the next 30 ms.
      if (env_ > 0.004f && env_ > 2.0f * slow_ && sinceOnset_ > captureSamples * 3) {
        sinceOnset_ = 0;
        target_ = 0.0f;
      }
      if (sinceOnset_ < captureSamples) target_ = juce::jmax(target_, env_);
      if (sinceOnset_ < (1 << 30)) ++sinceOnset_;
      // Hold the note at its picked level as the string decays; let go at
      // the floor so silence (and muting) stays quiet.
      const float want = env_ > floor_ && target_ > 0.0f ? juce::jlimit(1.0f, maxGain_, target_ / juce::jmax(env_, 1e-6f)) : 1.0f;
      gain_ = gainSmooth_ * gain_ + (1 - gainSmooth_) * want;
      x[i] *= gain_;
      if (toneOn_) x[i] = tone_.process(x[i]);
    }
    if (!shifter_.isRunning()) return;

    // Harmonic / Mix: an octave-up copy blooms in after each pick.
    float* up = scratch_.data();
    std::copy(x, x + n, up);
    float* channels[1] = {up};
    juce::AudioBuffer<float> view(channels, 1, n);
    shifter_.process(view);
    const float most = type_ == Type::harmonic ? 1.0f : type_ == Type::mix ? 0.5f : 0.0f;
    const float step = 1.0f / static_cast<float>(fs_ * bloomSeconds_);
    const int start = n > 0 ? sinceOnset_ - n : 0;
    for (int i = 0; i < n; ++i) {
      const int since = start + i;
      // Each pick restarts the bloom after a short dry attack.
      if (since >= 0 && since < static_cast<int>(fs_ * 0.12)) bloom_ = 0.0f;
      else bloom_ = juce::jmin(1.0f, bloom_ + step);
      const float w = most * bloom_;
      x[i] = x[i] * (1.0f - w) + up[i] * w;
    }
  }

private:
  double fs_ = 48000.0;
  PitchShift shifter_;
  std::vector<float> scratch_;
  Type type_ = Type::fundamental;
  float env_ = 0, slow_ = 0, gain_ = 1, target_ = 0, maxGain_ = 10, bloom_ = 0, bloomSeconds_ = 1.5f;
  float envAttack_ = 0, envRelease_ = 0, slowAttack_ = 0, slowRelease_ = 0, gainSmooth_ = 0;
  int sinceOnset_ = 1 << 30;
  float interval_ = 12.0f, floor_ = kFloor, responseMs_ = 15.0f;
  Biquad tone_;
  bool toneOn_ = false;
};

}  // namespace djont
