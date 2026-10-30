# Frankentribe OS — FM-1 Groovebox Firmware

**Experimental. Unrefined by design.**

A single-file sequencer / 4-operator FM synth / drum kit hybrid. Sounds pulled from EPROM dumps, game SFX, and third-party banks — no studio polish, deliberate grit.

## What it is
- **Synth:** Real 4-op FM engine (`fm_engine.c`) with 8 algorithms, 8 operator waveforms, per-op envelopes, LFO + ENV pages.
- **Drums:** 7 factory kits (TR-808 default) + one **ABUSE** kit — 8-bit/11025 Hz DOS SFX resampled to gritty 16-bit (not polished loops).
- **Sequencer:** Pattern + live MIDI, Shift+click button latching for combos. MPC-inspired deeper **swing** (up to ~62%), step-resolution readout, and **SAVE / SOUNDS** slots (8 flash slots, persisted to browser localStorage in the web emulator).
- **Generative genre engine:** seeded rules fill the three synth rows (bass/chord/lead) per genre — TECHNO / HOUSE / DUBSTEP / D&B / ELECTRO — via the **GEN** page (ARP key) or `GLO+ARP`. It applies genre defaults (BPM, swing, accent, FX, arp flavour) and is deterministic (same seed → same pattern). Externally drivable over MIDI CCs 94–96.
- **Patch/sound selection:** category-first browsing (GLO+PRESET = sound, GLO+OCT- = category). **SAVE** opens a SOUNDS page: `SLOT / SOURCE / ACTION (LOAD|SAVE|CLEAR)` with OCT+ to confirm.
- **Transport on the FX page:** KNOB1 = tempo (60–250 BPM), KNOB2 = swing, KNOB3 = accent, KNOB4 = loop length — no need to enter GLO.
- **UI:** HOME screen with theme palette ("default" = Abuse olive/magenta; plus AMBER, CYAN, RED, ALIEN, LAVA).

## Sources — how much we took, honestly

### FM voices (byte-level decode)
- **Yamaha TX81Z EPROM** (`4op synths/`): 128 voices — param-accurate.
- **Yamaha FB-01 ROM** (`4op synths/`): 144 voices — decoded per authoritative `FB01Editor` format (8-byte ops, inverted Total Level, OP4..OP1 storage). Algorithm routing approximated by carrier-set mapping.
- **Yamaha DX7** (`eeproms/`): 64 voices — names + heuristic params (not byte-exact).
- **ROM template / mined EPROMs**: 32 curated + extra EPROM dumps (Crumar Bit-01/99, AAC-Tiracon, Bohm-DD16, Cheetah-MD16, Doepfer ms404, etc.) — mostly untapped.

### Drum / wave sources
- **FunkBox** (iOS) drums: TR-808 samples.
- **Linn** drum kits: mapped to factory banks.
- **Absynth** banks (`new absynth banks/`): single-cycle wavetables.
- **Abuse** (`ABUSE/` DOS): 8-bit/11025 Hz SFX → resampled ABUSE kit (MGHIT02 kick, SHOTGN31 snare, DELOBJ01 clap, SWISH01 chh, LASRMIS2 ohh). Intentionally loud/quiet mix.

### Ideas only (no code/data taken)
- **Hortator / DEADACTIVE FM-1**: "4 knobs per page" interaction design (GPL-3.0).
- **Zoom FX ideas**: chorus/echo chain concepts.
- **KORG iELECTRIBE** (`ipa`): listed, not integrated.

### Visual skin
- **Abuse** (Crack dot Com, DOS 1995): palettes (`ART/*.SPE`), ANT sprite mascot, font sheets — public-domain source, decoded with REA + open-source engine. No code; only colour/theme assets.

See [`INFLUENCES.md`](INFLUENCES.md) for per-source fidelity notes (bytes / params / assets / ideas).

## What's experimental / unrefined
- FB-01 algorithm topology mapped, not bit-exact (voices sound correct but not hardware-identical).
- Abuse SFX are 8-bit, resampled, clipped/gain-varied — deliberately lo-fi.
- Bank selection is number-based (OCT- / OCT+) — functional, not polished.
- Genre generator state (genre/seed/density) is **not persisted** in slots — it resets on boot.
- Synth chords are a block triad across three consecutive steps (parts are monophonic).
- Docs: `docs/GUIDE.md` (web-emulator guide), `docs/HANDOFF.md` (state), `docs/PATCHES.md` (MIDI CC map).

## Build
Host (test): `cc -DDINGHY_HOST ...`
Web: `sh tools/build_web.sh` → `/docs/` → GitHub Pages.

Live (hard-refresh `Ctrl+Shift+R`): https://MarcoCasanova00.github.io/wavetribe-fm-custom-firmware/

---
*Brand: FRA N K E N T R I B E OS — built from scraps, redeems nothing.*
