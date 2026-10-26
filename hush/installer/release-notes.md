## Hush v{{VERSION}}

A de-noiser for your guitar or bass DI, with **zero latency**. It turns down hiss and electrical buzz (EMI) between notes and also under them, in the parts of the spectrum your note isn't using, before your amp sim or capture amplifies it. VST3 plugin and standalone app, Windows 10 / 11 (64-bit).

The same Hush is built into dJONt and Parabola Bass Z. This one is for everything else: other amp sims, or a Quad Cortex / modeller fed from your DAW.

### Install

1. Download **Hush-v{{VERSION}}-windows-x64.exe** below and run it.
2. Windows may say **"Windows protected your PC"**. It's an unsigned hobby build, not a store app: click **More info**, then **Run anyway**.
3. Leave both parts ticked (VST3 plugin + standalone app) and click Install. The guide opens when it's done.

| What | Where it goes |
| --- | --- |
| VST3 plugin | `C:\Program Files\Common Files\VST3\Hush.vst3` |
| Standalone app | `C:\Program Files\Hush\Hush.exe` (Start menu → Hush) |
| Guide (PDF) | Start menu → Hush |

### First run

1. **In a DAW:** rescan your VST3 plugins, then put Hush **first** on your guitar or bass track, before any amp sim, drive or compressor.
2. Volume up on your instrument, hands off the strings, click **LEARN** and wait two seconds.
3. Play, and raise **Strength** until the hiss is gone. If quiet notes lose sparkle, lower **Sensitivity** or **Strength**.

Hush starts **on** in its own plugin. Hover any control for a tip in the bar at the bottom.

### Guide

- **Hush-Guide.pdf** (2 pages): install, first run, every control, and tips. Also installed with the plugin.

To uninstall: Windows Settings → Apps → Hush → Uninstall.

<sub>Built from main at {{COMMIT}}.</sub>
