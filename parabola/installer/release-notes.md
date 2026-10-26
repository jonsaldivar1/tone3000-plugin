## Parabola Bass Z v{{VERSION}}

A 3-band parallel bass processor in the spirit of a Parallax X: your bass is split into **low / mid / high**, each band runs its own amp capture (**NAM** files you load), the mid and high bands go through a **cab IR**, and a **6-band graphic EQ** shapes the result. The low band stays clean and compressed so the bottom never turns to mud. VST3 plugin and standalone app, Windows 10 / 11 (64-bit).

**New in 0.5.1:** every band's **LOAD** (and the cab's) now opens in the folder you last loaded a capture from, on any band, through My Gear or by drag and drop, so you can go LOW, MID, HIGH without digging for the folder each time. It's remembered per computer, so loading a preset doesn't reset it.

**New in 0.5: the RACK.** Three units on your DI before the bands (click **RACK** under HUSH): **Split Drive**, a built-in drive on everything above a split you choose, clean below it, so you get a full parallel bass drive with no captures loaded. **Sub Comp**, a compressor on only the lows, blended back in. **DI Shaper**, a 7-band EQ on the DI every band hears. **SOLO** lets you hear the rack alone. All off by default; no added latency.

**Also in 0.4: HUSH**, a zero-latency de-noiser on your DI.

### Install

1. Download **Parabola-Bass-Z-v{{VERSION}}-windows-x64.exe** below and run it.
2. Windows may say **"Windows protected your PC"**. It's an unsigned hobby build, not a store app: click **More info**, then **Run anyway**.
3. Leave both parts ticked (VST3 plugin + standalone app) and click Install. The Quick Start guide opens when it's done.

| What | Where it goes |
| --- | --- |
| VST3 plugin | `C:\Program Files\Common Files\VST3\Parabola Bass Z.vst3` |
| Standalone app | `C:\Program Files\Parabola Bass Z\` (Start menu → Parabola Bass Z) |
| Your captures and cab IRs | `Documents\Parabola Bass Z\NAMs` and `\Cabs` (made for you, empty) |
| Quick Start + Manual (PDF) | Start menu → Parabola Bass Z |

### You'll need captures and a cab IR

Parabola doesn't include any amp captures or IRs (their licenses don't allow passing them around). Grab some free ones from [TONE3000](https://www.tone3000.com), for example:

- **Low band:** a clean bass amp, e.g. an Ampeg SVT clean capture (or leave the low band empty for a clean DI).
- **Mid / high bands:** a bass drive, e.g. a Darkglass B7K pack.
- **Cab:** a bass cab IR, e.g. an Ampeg 8x10 or Eden 4x10.

Put them in `Documents\Parabola Bass Z\NAMs` and `\Cabs`, keep them in `Documents\T3KJ Tones`, or keep them anywhere and add that folder in **MY GEAR → FOLDERS**. Then:

- pick a **starter preset** (the PRESET bar): it finds its captures and IR by file name in those folders, and names anything it can't find, or
- open **MY GEAR** and click captures into bands and an IR onto the cab, or
- click **LOAD** on a band (or drag a `.nam` onto it).

### First run

- **In a DAW:** rescan your VST3 plugins (Reaper: Options → Preferences → Plug-ins → VST → Re-scan), then put Parabola on your bass DI track. It replaces your amp and cab.
- **Standalone:** click the gear, pick **ASIO** and your audio interface, tick your bass's input, and turn on **Hear yourself** (it's off by default, the #1 reason for "no sound").
- Click **?** in the header for the manual inside the plugin.
- Also in the header: **NOISE** (a second gate after the drive bands: kills their hiss as notes fade), **PITCH** (transpose ±12 semitones), **TUNER** (mutes the output while open). Right-click any control to put it on a MIDI footswitch or pedal.
- Mid and high bands have **PRE / POST**: split before the capture (default) or after it (thicker).

### Guides

- **Parabola-Bass-Z-Quick-Start.pdf** (2 pages): install, captures, first sound, getting around.
- **Parabola-Bass-Z-Manual.pdf**: every section explained.

To uninstall: Windows Settings → Apps → Parabola Bass Z → Uninstall.

<sub>Built from main at {{COMMIT}}.</sub>
