# Frankentribe OS — FM-1 Groovebox Firmware

<p align="center">
  <img src="docs/logo.png" width="192" height="208" alt="Frankentribe OS logo — the Abuse ANT enemy silhouette">
  <br>
  <em>the Frankentribe mascot: the ANT from Abuse (Crack dot Com, 1995)</em>
</p>

**Experimental. Unrefined by design.**

A single-file sequencer / 4-operator FM synth / drum kit hybrid. Sounds pulled from EPROM dumps, game SFX, and third-party banks — no studio polish, deliberate grit.

## What it is
- **Synth:** Real 4-op FM engine (`fm_engine.c`) with 8 algorithms, 8 operator waveforms, per-op envelopes, LFO + ENV pages.
- **Drums:** 7 factory kits (TR-808 default) + one **ABUSE** kit — 8-bit/11025 Hz DOS SFX resampled to gritty 16-bit (not polished loops).
- **Sequencer:** Pattern + live MIDI, Shift+click button latching for combos. MPC-inspired deeper **swing** (up to ~62%), step-resolution readout, and **SAVE / SOUNDS** slots (8 flash slots, persisted to browser localStorage in the web emulator).
- **Generative genre engine:** seeded rules fill the three synth rows (bass/chord/lead) per genre — TECHNO / HOUSE / DUBSTEP / D&B / ELECTRO / AMBIENT / LOFI / ACID — via the **COMPOSE** page (`GLO+ARP`; build with OCT+, one-step undo with OCT-, per-bar **drift** `GLO+PLAY` with an OFF/ON/LOCK cycle). It applies genre defaults (BPM, swing, accent, FX, arp flavour) and is deterministic (same seed → same pattern). Externally drivable over MIDI CCs 94–96. The **ARP** button opens a dedicated per-part arpeggiator page (on/off, mode, octaves, rate); **EUCLID** (`GLO+SEQ`) writes Euclidean gate rhythms to any part.
- **Osc modes:** per-synth-part **hard sync**, **ring/AM**, **phase distortion (CZ-style)** and **vector synthesis** (OCT+ on the WAVE/EDIT page, or CC 98) for buzzy/metallic/morphing 80s textures.
- **LOST bank:** a sixth curated bank of hand-authored "lost ROMpler" patches (FM-EP, Motown Bass, Glass Pad, …).
- **Stutter FX:** SP-202/303-style loop-roll stutter as a fourth FX mode (OFF/CHORUS/ECHO/COMBO/STUT).
- **Frankentribe lab skin:** colorful multi-accent UI — yellow→green header band, per-page color banners and accent rails, color-coded part rows, status-pill footer.
- **Live FX layer:** hold FX + keys (F3..C4, D4/E4 throws) for momentary whole-mix effects — stutter, reverse/echo throws, tape stop, bitcrush, gate, filter sweep, half speed. Also triggerable by MIDI CC 99 (0–7 start / 127 stop).
- **Quick mute:** hold HOME (≥300 ms) + F3…C4 toggles part mutes live (tap HOME still opens the system page).
- **Bidirectional MIDI clock:** MASTER mode sends 24 PPQ clock + Start/Stop over USB; **SLAVE** mode follows external Start/Continue/Stop and steps the sequencer on incoming F8 (USB or TRS). Clock mode cycles via GLO+FX or CC 100.
- **MORPH / VMORPH osc modes:** phase distortion and vector synthesis whose amounts follow the note envelope — crisp attack settling to a sine (MORPH) or a per-note four-wave sweep (VMORPH) — alongside SYNC/RING/PD/VEC as per-part integrated oscillator modes.
- **Font:** normalized to Terminus 16 (regular body + generated bold for titles/brand/selected part); Nerd Fonts not compatible with the 1-bit BDF bitmap pipeline.
- **Volca-style FUNC keys:** with GLO latched, the first 16 white keys act as grid-key shortcuts (clear / save / load / pattern ± / copy / paste / step mode).
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

## Device build / flashing

`python3 tools/build.py` (Linux x86-64, the JieLi toolchain and AC79 SDK — see
`tools/get_toolchain.sh`) produces `build/dinghy.fwsc` (FM-1 identity; `--board smk37`
with `--model pro|elite|mke` for the SMK-37 family). Install it with the M-Vave
updater or a Web-MIDI SysEx installer.

The bundled update loader is conservative: it writes **only the app area**
(never the SPL/head, calibration/BTIF, stock USR resources or key_mac), checks the
package's key before touching flash, skips already-equal sectors, verifies each
sector and the full-package CRC, and is resumable across power loss (the update
record survives; a damaged package leaves the loader waiting). `bootguard.h`
adds a recovery/ROM fallback after repeated failed boots. Rollback to stock
remains possible (its resources are kept and protected).

Live (hard-refresh `Ctrl+Shift+R`): https://MarcoCasanova00.github.io/frankentribe-os/

---
*Brand: FRA N K E N T R I B E OS — built from scraps, redeems nothing.*
