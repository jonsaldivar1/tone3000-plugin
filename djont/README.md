# dJONt

A low-end toolkit plugin (VST3 + standalone) to run before your amp sim
(T3KJ or anything else). Mono in; six modules, each with a power LED:

| Module | Controls | What it does |
|---|---|---|
| **Wham** | Pedal / Auto, Classic / Chords, Interval, Heel, Toe, Rate, Shape, Retrig, Glide, the pedal, presets; ADV: Grain, Buffer, Tone, Mix | A virtual whammy. Classic is the old-school two-tap shifter (pitch-tracked, so single notes are clean and chords warble); Chords is the clean, chord-friendly engine. Interval: Custom (Heel / Toe), the Whammy list (2 Oct Up ... Dive Bomb), Harmony pairs, Detune. Pedal: drag the rocker, automate it or MIDI-map it; Auto: an LFO; Retrig restarts it on every pick. Named presets in `%APPDATA%\dJONt\wham-presets.json`. Custom / Whammy add ~16 ms latency while on (Harmony / Detune none). |
| **Sustain** | Hold / Sustainer. Hold: HOLD, Latch, Chord / Pile-up, Fade, Level (ADV: Shimmer, Low Cut, Tone). Sustainer: Fund / Harm / Mix, Sustain, Bloom (ADV: Interval, Floor, Response, Tone) | Hold is a freeze, like a piano's sustain pedals: Chord holds what was ringing when you pressed (sostenuto), Pile-up keeps adding while held (damper); you play over it. Sustainer keeps each note at its picked level as the string dies (like a sustainer pickup), and Harm / Mix bloom into the octave. |
| **Tight** | Amount, Freq | Lifts each pick attack / palm mute in the Freq band and trims the decay, so chugs hit harder and stop sooner. |
| **Grit** | Drive, Blend, Freq | A fuzz layer on the lows only (below Freq), blended under the dry signal. |
| **Sub** | Clean / Synth, Blend, Tone | An octave down under the dry signal. Clean = pitch shifter (handles chords, ~20 ms behind). Synth = analog-style divider (zero delay, single notes, buzzier). |
| **Lo-Fi** | Tone, Blend | Radio / telephone band-pass around Tone with a touch of drive. |

MIDI learn: right-click any control → MIDI Learn, then press a footswitch or
move a pedal (a QC footswitch sending a CC works). Mappings save with the
plugin's state. HOLD with Latch on flips on each press of a momentary switch.
Wham's Glide turns a footswitch's on/off into a timed sweep.

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
