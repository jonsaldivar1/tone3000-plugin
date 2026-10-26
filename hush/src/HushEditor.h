// Hush's window: the wordmark and in / out meters, the HUSH panel (shared
// with dJONt and Parabola), and a hint bar. Resizable with a fixed aspect
// ratio (60% to 200%); the layout is drawn at its design size and scaled.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "HushPanel.h"
#include "HushProcessor.h"

class HushLookAndFeel : public juce::LookAndFeel_V4 {
public:
  HushLookAndFeel();
  void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end,
                        juce::Slider&) override;
  static juce::Font display(const juce::Component& c, float px);
  static const juce::Colour bg, ink, accent, plate;

private:
  juce::Typeface::Ptr anton_;
};

class HushView : public juce::Component, private juce::Timer {
public:
  static constexpr int kWidth = 880, kHeight = 440;
  explicit HushView(HushProcessor& processor);
  ~HushView() override;
  void paint(juce::Graphics& g) override;
  void resized() override;
  juce::String currentHint() const { return hint_; }

private:
  void timerCallback() override;
  HushProcessor& processor_;
  HushLookAndFeel lnf_;
  hushui::HushPanel panel_;
  float inDb_ = -60.0f, outDb_ = -60.0f;
  juce::String hint_;
};

class HushEditor : public juce::AudioProcessorEditor {
public:
  explicit HushEditor(HushProcessor& processor);
  void resized() override;
  void paint(juce::Graphics& g) override { g.fillAll(HushLookAndFeel::bg); }
  juce::String currentHint() const { return view_.currentHint(); }

private:
  HushProcessor& processor_;
  HushView view_;
};
