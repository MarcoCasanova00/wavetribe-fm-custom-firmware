# Frankentribe OS

[![License](https://img.shields.io/github/license/MarcoCasanova00/wavetribe-fm-custom-firmware?color=GPL)](LICENSE)
[![Web Emulator](https://img.shields.io/badge/web-emulator%20live-blue)](https://MarcoCasanova00.github.io/wavetribe-fm-custom-firmware/)

**A retro-styled FM-1 groovebox firmware with a 160-cycle
wavetable engine, seven drum kits, a synth patch bank, MIDI I/O and an on-board
Chorus→Echo→Combo FX bus.**

- **Web emulator:** `https://MarcoCasanova00.github.io/wavetribe-fm-custom-firmware/`
- **GitHub:** `https://github.com/MarcoCasanova00/wavetribe-fm-custom-firmware`
- **Documentation:** [GUIDE.md](GUIDE.md), [PATCHES.md](PATCHES.md), [docs/ROM_SOURCES.md](docs/ROM_SOURCES.md), [main/docs/PORT.md](main/docs/PORT.md)

## Highlights

- 8-part sequencer (5 drum voices + 3 FM/wavetable tonal voices)
- 160-curve wavetable carrier oscillator, indexed per part
- 7 drum banks (switchable on knob B while a drum part is armed)
- 5 + 64-table synth patches and global FX PATCH browser
- 4-mode stereo-monolith FX bus, knob-adjustable LFO & tail
- MIDI in / out on USB + 5-pin TRS (24 ppqn clock pulses too)
- SANTO CENTER red-metal chassis overlay in the browser and host app
- Host test build, WebAssembly browser emulator and PNG frames

## Layout

- **`main/`** — firmware sources + tools
- **`main/firmware/src/dinghy.c`** — the app, ~1100 lines (Drum engine, FM voices, screen draw, sequencer)
- **`main/firmware/gen/`** — auto-generated `factory_*_banks` (catalog, wavetables, patches)
- **`main/tools/factory/`** — importer toolchain

## Web emulator

The firmware compiles to a browser emulator with SANTO CENTER chassis:

```
EMSDK=$HOME/emsdk main/tools/build_web.sh
cd docs
python3 -m http.server 8000
```

Open http://localhost:8000/index.html.

## Credits & license

- FM-1 platform (Hügelton Instruments/Felucca/Sloop).
- Frankentribe OS patchwork by the melting pot team.
- Distributed under GPL-3.0-only.