# Hush

A zero-latency DI de-noiser (VST3 + standalone, Windows). The engine is
`shared/Hush.h` and the panel is `shared/HushPanel.*`; dJONt and Parabola
Bass Z build the same code in (off by default there). This plugin starts on.

## How it works

- Eight bands: bells at 200, 350, 600, 1k, 1.8k, 3.2k and 5.6k Hz (Q 1.2)
  plus a 9 kHz high shelf.
- Each band has a 4th-order detector and an RMS follower (1 ms attack,
  30 ms release). A band is fully open 6 dB over its threshold
  (learned floor + Sensitivity) and fully cut at it, with a soft knee in
  between.
- Bands open in about 1 ms and close over Release. A closed band is cut by
  up to 30 dB times Strength. There's no look-ahead, so latency is zero.
- LEARN averages each band for two seconds; the floor is that mean plus
  3 dB. Floors are saved with the plugin state.
- Off is a bit-exact bypass, with a 20 ms crossfade on switching.

## Build

```
cmake -B build-hush -S hush -G Ninja -DCMAKE_BUILD_TYPE=Release -DHUSH_TESTS=ON
cmake --build build-hush
./build-hush/HushTests_artefacts/Release/HushTests
```

CI: `.github/workflows/hush-windows.yml` builds the installer and publishes
the rolling `hush-latest` pre-release.
