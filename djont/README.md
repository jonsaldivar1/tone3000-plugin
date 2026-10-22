# dJONt

A low-end toolkit plugin (VST3 + standalone) to run before your amp sim
(T3KJ or anything else). Mono in; five modules, each with a power LED:

| Module | Controls | What it does |
|---|---|---|
| **Wham** | Pedal / Auto, Heel, Toe, Rate, Shape, Retrig, the pedal, presets | A virtual whammy: pitch sweeps between Heel and Toe (±24 st). Pedal: drag the rocker, automate it or MIDI-map it in your DAW. Auto: an LFO (Sine, Triangle, Rise, Square) moves it; Retrig restarts it on every pick. Name and save your settings as presets (kept in `%APPDATA%\dJONt\wham-presets.json`). Adds ~16 ms latency while on. |
| **Tight** | Amount, Freq | Lifts each pick attack / palm mute in the Freq band and trims the decay, so chugs hit harder and stop sooner. |
| **Grit** | Drive, Blend, Freq | A fuzz layer on the lows only (below Freq), blended under the dry signal. |
| **Sub** | Clean / Synth, Blend, Tone | An octave down under the dry signal. Clean = pitch shifter (handles chords, ~20 ms behind). Synth = analog-style divider (zero delay, single notes, buzzier). |
| **Lo-Fi** | Tone, Blend | Radio / telephone band-pass around Tone with a touch of drive. |

Two looks (gear → Settings): **Chrome** (orange on chrome foil) and **Red
Foil** (red on red/blue foil), both color-graded free-license photos. The bar
along the bottom says what whatever you hover does, with its value.

Settings also holds the audio setup when dJONt runs as the standalone app:
driver type (ASIO), interface, which input your guitar is on, outputs, sample
rate and buffer size, the latency readout (input + output + buffer; dJONt
itself adds 0 ms) and Hear Yourself. In a DAW the DAW owns those.

Original code. Font: Anton (SIL OFL 1.1, `assets/Anton-LICENSE.txt`).
Sub's Clean mode uses T3KJ's pitch shifter (`plugin/src/PitchShift.cpp`).

Build: `cmake -B build-djont -S djont -DCMAKE_BUILD_TYPE=Release && cmake --build build-djont`
(add `-DDJONT_TESTS=ON` for the `DjontTests` checks). Windows installer:
`.github/workflows/djont-windows.yml` → the `djont-latest` release.
