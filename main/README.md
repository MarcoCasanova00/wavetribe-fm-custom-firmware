# Frankentribe OS — Firmware Guide (v0.3)

Frankentribe OS is a fast, modular groovebox firmware for the **M-VAVE FM-1**,
built on the Dinghy platform. It mashes a legacy the app-pattern sequencer,
 wavetable graphics, drum-rom samples, and a lightweight FM engine into one 240 × 240
 package.

## Layout

- `firmware/src/dinghy.c` — the application (UI, sequencer, synth, FX, MIDI).
- `firmware/gen/factory_*.h` — engraved sound banks, descriptive read-only patch tables.
- `firmware/hal/` — chip board drivers handled by the Dinghy platform.
- `tools/factory/` — bank import utilities for `wavetables` and drums.
- `docs/PORT.md`, `docs/API.md`, `docs/RULES.md`, `docs/SMK37.md` - source port notes.

## Sound palette

- 192 wavetable oscillators tuned to the FM-1 panel knob mapping.
- 7 drum kit selections (APP, TR-808, TR-909, CR-78, Linn, MD-110, and Linn-9000 ROM).
- 13 FM synth presets + 64 DX7 voice names sourced during the **Melting Pot** phase.

## Web emulator

The firmware compiles against a browser-safe Dinghy platform:

```
EMSDK=$HOME/emsdk ./tools/build_web.sh
cd ../docs
python3 -m http.server 8000
```

Then open http://localhost:8000/index.html.

## Build

```bash
# Desktop simulator
tests/run_host.sh tests/scripts/riptide-demo.txt

# Hardware (JieLi toolchain)
JIELI_TOOLCHAIN=$HOME/.jieli/toolchain AC79_SDK=$HOME/fw-AC79_AIoT_SDK ./build.sh
```

## Credits

- **The Dreamblade** — FM-1 platform (Hügelton Instruments)
- **Dinghy** — original Firmware scaffold
- **i-app relic** — legacy the app pattern data
- **FunkBox drum kits** — reconstructed classic machine samples

## License

Same files are distributed under the **GNU GPL-3.0-only**.

---

Frankentribe OS — An open melting pot of old school kit on a modern micro-controller.