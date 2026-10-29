## dJONt v{{VERSION}}

A pedalboard plugin for guitar that goes in front of your amp sim: **Wham** (a virtual whammy), **Sustain** (Hold and Sustainer), **Tight**, **Grit**, **Sub** and **Lo-Fi**, plus **Hush**, a de-noiser for your DI. VST3 plugin and standalone app, Windows 10 / 11 (64-bit).

**New in 0.9: presets and scenes.** The header's **PRESET** bar saves your whole board under a name: SAVE edits the current one, ••• saves a new one, renames, deletes or reorders. The **SCENES** row (A to H, + to add) switches everything at once: modules on / off, knobs, Input, Output and Hush. Presets and scenes follow MIDI: a Program Change loads the preset at that number, and a CC learned on a scene picks scenes by value (0 = A, 1 = B ...), so one Quad Cortex footswitch can switch dJONt and T3KJ together. The manual has a new page on automating dJONt in REAPER.

**Also in 0.8:** **Tight** has a second voicing, **FOCUS**: the classic tight boost in front of a high-gain amp (low cut, mid push, a little clipping, no added latency). Flip it with the PUNCH / FOCUS switch. And a global **Blend** knob in the header: the whole board against your dry DI, lined up even when Wham adds delay.

**Also in 0.7: HUSH**, a zero-latency de-noiser on your DI.

### Install

1. Download **dJONt-v{{VERSION}}-windows-x64.exe** below and run it.
2. Windows may say **"Windows protected your PC"**. It's an unsigned hobby build, not a store app: click **More info**, then **Run anyway**.
3. Leave both parts ticked (VST3 plugin + standalone app) and click Install. The Quick Start guide opens when it's done.

| What | Where it goes |
| --- | --- |
| VST3 plugin | `C:\Program Files\Common Files\VST3\dJONt.vst3` |
| Standalone app | `C:\Program Files\dJONt\dJONt.exe` (Start menu → dJONt) |
| Quick Start + Manual (PDF) | Start menu → dJONt |

### First run

- **In a DAW:** rescan your VST3 plugins (Reaper: Options → Preferences → Plug-ins → VST → Re-scan), then put dJONt on your guitar track **before** your amp sim.
- **Standalone:** click the gear, pick **ASIO** and your audio interface, tick your guitar's input, and turn on **Hear yourself** (it's off by default, the #1 reason for "no sound").
- Click **?** in dJONt's header for the manual inside the plugin: every module, how it works, and settings to try.

### Guides

- **dJONt-Quick-Start.pdf** (2 pages): install, first sounds, getting around.
- **dJONt-Manual.pdf**: the full manual (Wham modes and presets, Hold vs Sustainer, footswitches and MIDI, every control).

Both are also installed with the plugin.

To uninstall: Windows Settings → Apps → dJONt → Uninstall.

<sub>Built from main at {{COMMIT}}.</sub>
