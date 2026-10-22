# dJONt

A low-end toolkit plugin (VST3 + standalone) to run before your amp sim
(T3KJ or anything else). Mono in; four modules, each with a power LED:

| Module | Controls | What it does |
|---|---|---|
| **Tight** | Amount, Freq | Lifts each pick attack / palm mute in the Freq band and trims the decay, so chugs hit harder and stop sooner. |
| **Grit** | Drive, Blend, Freq | A fuzz layer on the lows only (below Freq), blended under the dry signal. |
| **Sub** | Clean / Synth, Blend, Tone | An octave down under the dry signal. Clean = pitch shifter (handles chords, ~20 ms behind). Synth = analog-style divider (zero delay, single notes, buzzier). |
| **Lo-Fi** | Tone, Blend | Radio / telephone band-pass around Tone with a touch of drive. |

Original code. Font: Anton (SIL OFL 1.1, `assets/Anton-LICENSE.txt`).
Sub's Clean mode uses TK3J's pitch shifter (`plugin/src/PitchShift.cpp`).

Build: `cmake -B build-djont -S djont -DCMAKE_BUILD_TYPE=Release && cmake --build build-djont`
(add `-DDJONT_TESTS=ON` for the `DjontTests` checks). Windows installer:
`.github/workflows/djont-windows.yml` → the `djont-latest` release.
