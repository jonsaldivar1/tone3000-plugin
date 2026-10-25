# Parabola Bass Z

A 3-band parallel bass processor in the spirit of a Parallax X, built on
T3KJ's NAM engine. VST3 + standalone, Windows.

```
DI -> Input -> Gate -> Crossover (Linkwitz-Riley 24 dB/oct, two splits)
   LOW : Drive -> [NAM, optional] -> Comp -> Bass/Mid/Treble -> Level
   MID : Drive -> NAM -> Tone -> Bass/Mid/Treble -> Level
   HIGH: Drive -> NAM -> Tone -> Bass/Mid/Treble -> Level
   bands with CAB on (mid, high by default) -> cab IR; the rest bypass it
   -> sum -> 6-band graphic EQ (80, 250, 500, 1k, 2.5k, 6k) -> Output
```

- The low band also runs through the upper split's all-pass, so with
  nothing loaded and every band at 0 dB the bands sum back flat.
- Every capture's output is normalised to -18 dB from its own loudness
  metadata, so captures swap at matched level. A capture whose metadata says
  it includes a cab (`gear_type` full-rig / cab) skips the IR.
- Cab IRs are level-matched over 100 Hz - 3 kHz, so switching the cab on
  doesn't jump the volume.
- Everything runs at 48 kHz (NAM A2's rate). At other host rates the chain
  sits behind AudioDSPTools' Lanczos resampler and reports its delay
  (27 samples at 44.1 kHz).
- No captures or IRs ship with it (their licenses don't allow it). The
  installer makes `Documents\Parabola Bass Z\NAMs` and `\Cabs`; LOAD starts
  there, and `.nam` / `.wav` files can be dropped on a band / the cab.
  File paths are saved in the plugin state.

UI: design A from the mockups (a dJONt sibling): crossover display with
draggable split handles, three band strips (ADV flips each to its EQ and
CAB switch), graphic EQ, cab. Skins: Foil, Inferno, Hologram, Ram, Ember.
Resizable (60% - 200%), hint bar, **?** manual (text in `manual/manual.json`,
also the PDF manual's reference section). Icon: `icon/parabola.png`,
`installer/parabola.ico`.

Build (needs the `plugin/NeuralAmpModelerCore` and `plugin/AudioDSPTools`
submodules, recursive):

```
cmake -B build-parabola -S parabola -DCMAKE_BUILD_TYPE=Release -DPARABOLA_TESTS=ON
cmake --build build-parabola --config Release
build-parabola/ParabolaTests_artefacts/Release/ParabolaTests [--nam-dir <dir with .nam files>] [--ir <wav>]
```

Windows installer: `.github/workflows/parabola-windows.yml` -> the
`parabola-latest` release. Release page text: `installer/release-notes.md`.

0.2 adds:

- Presets (`src/Presets.*`): every parameter plus each band's capture and
  the IR by file name and last path. Loading tries the path, then a file of
  that name anywhere in the tones folders. Starter presets are built around
  free TONE3000 packs (SVT-CL, B7K, Alpha Omega, Microtubes 500, three IR
  packs) and work on any machine that has them downloaded. User presets:
  `%APPDATA%\Parabola Bass Z\presets.json`.
- My Gear: every capture and IR in `Documents\Parabola Bass Z` and
  `Documents\T3KJ Tones` (T3KJ's My Gear folder), searchable, one click to a
  band or the cab.
- Pitch (T3KJ's `PitchShift`, ±12 st, before the crossover; adds ~16 ms
  while not 0, reported to the host), Tuner (T3KJ's `TunerDetector`; mutes
  the output while open), MIDI learn (right-click, as in dJONt).

Original code. Font: Anton (SIL OFL 1.1, `assets/Anton-LICENSE.txt`).
Skin photos: Jon's.
