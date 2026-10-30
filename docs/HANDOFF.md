# HANDOFF — Frankentribe OS: current state

## Status
Shipping. Firmware + web emulator both build clean; all host regression tests pass;
live on GitHub Pages.

## Patch banks (5)
| Bank | Name | Count | Source |
|------|------|-------|--------|
| 0 | APP | 13 | Hand-designed |
| 1 | DX7 | 64 | DX7 voice names + heuristic params |
| 2 | ROM | 32 | EPROM source names + heuristic params |
| 3 | TX81Z | 128 | Parsed from Yamaha TX81Z EPROM |
| 4 | FB01 | 144 | Parsed from Yamaha FB-01 EPROM (FB01Editor format) |

## Drum kits (7)
Factory kits (TR-808 default, APP, FunkBox, Linn, …) + an **ABUSE** kit built
from the DOS game's 8-bit/11025 Hz SFX (resampled to 16-bit, gritty).

## Features
- Real 4-op FM engine (`fm_engine.c`): 8 algorithms, 8 operator waveforms, per-op ADSR.
- 8-part step sequencer (16/32/48/64 steps), MPC-style deeper swing, accent.
- Live patch-bank cycling (GLO + OCT-) and patch selection (GLO + PRESETS).
- **Save/load slots**: 8 flash slots (`SAVE` store, `GLO+SAVE` pick, `GLO+REC` load).
- HOME system page: theme palettes ("default"/AMBER/CYAN/RED/ALIEN/LAVA + more).
- Web emulator (Shift+click latches buttons for combos).
- **Generative genre engine** (COMPOSE page via `GLO+ARP`, build with OCT+, one-step undo with OCT-, optional per-bar **drift** with `GLO+PLAY`): seeded, deterministic rules fill the three synth rows (bass/chord/lead) per genre — TECHNO / HOUSE / DUBSTEP / D&B / ELECTRO / AMBIENT / LOFI / ACID — and apply genre defaults (BPM, swing, accent, FX, arp flavour). MIDI CCs 94–96 drive it externally.
- **ARPEGGIATOR page** (ARP button): per-part on/off, mode, octaves and rate; fed by held keys / MIDI / COMPOSE chord bank.
- **EUCLID page** (`GLO+SEQ`): Euclidean rhythm generator (steps/hits/rotation) writes gates to the selected part; drift has an OFF/ON/LOCK cycle (`GLO+PLAY`).
- **Osc modes** per synth part: hard sync, ring/AM, phase distortion and vector synthesis (WAVE page OCT+ / CC 98).
- **Volca-style FUNC keys:** with GLO latched, the first 16 white keys run grid-key shortcuts (clear / save / load / pattern ± / copy / paste / step mode).
- **LOST bank:** sixth curated bank (16 hand-authored "lost ROMpler" patches).
- **Stutter FX:** SP-202/303-style loop-roll stutter (FX mode 4).
- **Frankentribe lab skin:** full visual refresh — gradient header band, per-page color banner + accent rail (FX Y / WAVE B / LFO G / ENV flesh / HOME O / SAVE Y / SOUNDS B / ARP G / COMPOSE R / EUCLID O), color-coded part rows, status-pill footer. Still 240×240, same primitives.
- Transport knobs live on the **FX page** (tempo / swing / accent / loop length), so tempo changes don't require entering GLO.

## Sources
- `INFLUENCES.md` — full source/attribution table (EPROMs, ipa, Abuse, etc.).
- `REPORT-MPC3000.md` — AKAI MPC3000 / Vailixi 3.50 RE notes.

## Build verification
- Host: `cc -DDINGHY_HOST …` clean; `sh tests/run_host.sh` scripts pass.
- Web: `EMSDK=$HOME/emsdk sh tools/build_web.sh` → `docs/` → GitHub Pages.
- Live: https://MarcoCasanova00.github.io/wavetribe-fm-custom-firmware/

## Known limitations
- FB-01 algorithm *topology* mapped by carrier-set (not a bit-exact hardware clone).
- Abuse SFX are deliberately lo-fi (8-bit resampled).
- MPC3000 string decode blocked by font-code obfuscation; took ideas only (see report).
- Genre engine state (genre/seed/density) is volatile — not persisted in SAVE slots.
- Synth chords are a block triad over three consecutive steps (parts are monophonic).
