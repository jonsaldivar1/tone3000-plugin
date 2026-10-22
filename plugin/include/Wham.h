// Fork (TK3J): the pitch shifter's virtual expression pedal ("wham").
//
// A whammy is a pitch shifter plus a pedal; this is the pedal, for players
// without one. With the pitch deck's WHAM mode on, the faceplate Pitch knob
// stops being the shift and becomes the toe (where the pedal goes when it's
// all the way down), the deck's Heel knob is where it rests, and the shift
// sweeps between them by:
//
//   PEDAL  the virtual pedal's position (whamPedal, 0 = heel, 1 = toe):
//          drag the rocker in the deck, automate it in the DAW, or map a
//          MIDI CC (an expression pedal on a QC) to it.
//   AUTO   an LFO moves the pedal for you at RATE in one of four shapes:
//            Sine      smooth up and back
//            Triangle  straight up and back
//            Rise      heel to toe, then snaps back (a saw); with RETRIG on
//                      it rises once per pick and holds at the toe: the
//                      classic "pick, then scoop up an octave" move
//            Square    jumps between heel and toe (trill / octave stabs)
//          RETRIG restarts the cycle at the heel on every pick attack, so
//          the sweep follows your playing instead of free-running.
//
// The position is computed per slice (kSliceSamples) and handed to the
// shifter as a semitone value, so a sweep is smooth at any host buffer. A
// short glide (kGlideMs) keeps Square's jumps from clicking and the pedal
// from zippering on coarse MIDI steps.
//
// Audio thread only, no allocation; prepare() from prepareToPlay.
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>

namespace t3k {

class Wham {
public:
  enum class Mode { off = 0, pedal = 1, autoSweep = 2 };
  enum class Shape { sine = 0, triangle = 1, rise = 2, square = 3 };

  static constexpr int kSliceSamples = 32;
  static constexpr float kGlideMs = 8.0f;
  static constexpr float kMinRateHz = 0.1f, kMaxRateHz = 10.0f, kDefaultRateHz = 1.0f;

  struct Settings {
    Mode mode = Mode::off;
    Shape shape = Shape::sine;
    float pedal = 0.0f;  // 0..1, PEDAL mode
    float rateHz = kDefaultRateHz;
    bool retrig = false;
  };

  void prepare(double sampleRate) {
    fs_ = sampleRate > 0 ? sampleRate : 48000.0;
    glide_ = coefficient(kGlideMs);
    fastAttack_ = coefficient(0.5f);
    fastRelease_ = coefficient(15.0f);
    slowAttack_ = coefficient(40.0f);
    slowRelease_ = coefficient(250.0f);
    refractorySamples_ = static_cast<int>(fs_ * 0.09);
    reset();
  }

  void reset() {
    phase_ = 0.0;
    position_ = 0.0f;
    fast_ = slow_ = 0.0f;
    sinceOnset_ = refractorySamples_;
    held_ = false;
  }

  // Advances the pedal over `numSamples` of `buffer` from `start` (the
  // channels' mean drives the pick detector) and returns its position,
  // 0 (heel) .. 1 (toe), for this slice.
  float advance(const Settings& s, const juce::AudioBuffer<float>& buffer, int start, int numSamples) {
    if (s.mode == Mode::autoSweep && s.retrig && detectOnset(buffer, start, numSamples)) {
      phase_ = 0.0;
      held_ = false;
    }

    float target = 0.0f;
    if (s.mode == Mode::pedal) {
      target = juce::jlimit(0.0f, 1.0f, s.pedal);
    } else if (s.mode == Mode::autoSweep) {
      target = shapeAt(s.shape, phase_);
      const double rate = juce::jlimit(static_cast<double>(kMinRateHz), static_cast<double>(kMaxRateHz),
                                       static_cast<double>(s.rateHz));
      phase_ += rate * numSamples / fs_;
      if (phase_ >= 1.0) {
        // One-shot Rise under RETRIG: rise once per pick, then hold the toe.
        if (s.shape == Shape::rise && s.retrig) held_ = true;
        phase_ -= std::floor(phase_);
      }
      if (held_ && s.shape == Shape::rise) target = 1.0f;
    }

    const float g = std::pow(glide_, static_cast<float>(numSamples));
    position_ = g * position_ + (1.0f - g) * target;
    return position_;
  }

  float position() const { return position_; }

  static float shapeAt(Shape shape, double phase) {
    const float p = static_cast<float>(phase);
    switch (shape) {
      case Shape::triangle: return 1.0f - std::abs(2.0f * p - 1.0f);
      case Shape::rise: return p;
      case Shape::square: return p < 0.5f ? 1.0f : 0.0f;
      case Shape::sine: break;
    }
    return 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * p);
  }

private:
  float coefficient(float ms) const { return static_cast<float>(std::exp(-1.0 / (fs_ * ms * 0.001))); }

  // A pick attack: the fast envelope jumping well over the slow one, above
  // the noise floor, at most once per refractory window.
  bool detectOnset(const juce::AudioBuffer<float>& buffer, int start, int numSamples) {
    const int channels = juce::jmin(2, buffer.getNumChannels());
    if (channels <= 0) return false;
    const float scale = 1.0f / static_cast<float>(channels);
    bool onset = false;
    for (int i = 0; i < numSamples; ++i) {
      float x = 0.0f;
      for (int ch = 0; ch < channels; ++ch) x += buffer.getSample(ch, start + i);
      const float a = std::abs(x * scale);
      fast_ = a > fast_ ? fastAttack_ * fast_ + (1 - fastAttack_) * a : fastRelease_ * fast_ + (1 - fastRelease_) * a;
      slow_ = a > slow_ ? slowAttack_ * slow_ + (1 - slowAttack_) * a : slowRelease_ * slow_ + (1 - slowRelease_) * a;
      ++sinceOnset_;
      if (sinceOnset_ >= refractorySamples_ && fast_ > 0.004f && fast_ > 2.2f * slow_) {
        sinceOnset_ = 0;
        onset = true;
      }
    }
    return onset;
  }

  double fs_ = 48000.0;
  double phase_ = 0.0;
  float position_ = 0.0f;
  float glide_ = 0.0f;
  float fast_ = 0.0f, slow_ = 0.0f;
  float fastAttack_ = 0.0f, fastRelease_ = 0.0f, slowAttack_ = 0.0f, slowRelease_ = 0.0f;
  int refractorySamples_ = 4320, sinceOnset_ = 4320;
  bool held_ = false;
};

}  // namespace t3k
