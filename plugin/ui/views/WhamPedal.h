// Fork (TK3J): the pitch deck's virtual expression pedal. A treadle drawn
// side-on that rocks from heel (flat, bottom) to toe (tilted down, top
// reading), with the position filling a track beside it.
//  - PEDAL mode: drag up/down (or scroll) to move it; it is the whamPedal
//    parameter, so automation and a MIDI-learned expression pedal move it
//    too. Double-click (Alt/Option-click) sends it back to the heel.
//  - AUTO mode: it follows the LFO live (MeterStore::whamPosition) and
//    ignores the mouse.
//  - Off: drawn at the heel, dimmed by the deck.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "services/ParamBinding.h"
#include "services/Services.h"

namespace t3k::ui {

class WhamPedal : public juce::Component, private MeterStore::Listener {
public:
  static constexpr int kWidth = 56;

  explicit WhamPedal(Services& services);
  ~WhamPedal() override;

  enum class Mode { off, pedal, autoSweep };
  void setMode(Mode mode);

  void paint(juce::Graphics& g) override;
  void mouseDown(const juce::MouseEvent& e) override;
  void mouseDrag(const juce::MouseEvent& e) override;
  void mouseUp(const juce::MouseEvent& e) override;
  void mouseDoubleClick(const juce::MouseEvent& e) override;
  void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
  void whamChanged() override;
  float shown() const;
  juce::Rectangle<float> track() const;

  Services& services_;
  ParamBinding pedal_;
  Mode mode_ = Mode::off;
  float dragStartValue_ = 0.0f;
};

}  // namespace t3k::ui
