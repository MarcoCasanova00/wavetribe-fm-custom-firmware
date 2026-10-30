# Frankentribe OS — User Guide

The Frankentribe OS is a 4-operator FM groovebox firmware (the Wavetribe FM-1)
with a generative soul: it composes from genres, writes Euclidean drum patterns,
and mixes in a palette of oscillator modes. This guide covers the whole system.

> **Live:** https://MarcoCasanova00.github.io/wavetribe-fm-custom-firmware/
> Hard-refresh (**Ctrl+Shift+R**) to pick up the latest build.

---

## 1. The panel

**Buttons** (FM-1):

| Button | Page / role |
|--------|-------------|
| OCT− / OCT+ | octave shift (keys), plus page actions (see below) |
| FX | FX page |
| SEL | (encoder) selected part |
| ENV | Envelope page |
| LFO | LFO page |
| EDIT | WAVE/EDIT page (GLO+EDIT = SOUNDS) |
| GLO | global latch = the **FUNC** modifier |
| HOME | system page (themes) |
| SAVE | save/load slots |
| ARP | ARPEGGIATOR page (GLO+ARP = COMPOSE) |
| SEQ | sequencer page (GLO+SEQ = EUCLID) |
| PLAY | start/stop (GLO+PLAY = drift) |
| REC | write arm (GLO+REC = load slot) |

**Encoders / knobs:** SELECT = part, ALGORITHM = loop length, PRESETS = browse,
KNOB1–4 = contextual (per page).

The skin is the **Frankentribe lab look**: a bolt-yellow → slime-green header
band, a colored page banner + accent rail per page (FX yellow, WAVE blue,
LFO green, ENV flesh, HOME orange, SAVE yellow, SOUNDS blue, ARP green,
COMPOSE red, EUCLID orange), color-coded part rows, and a status pill in the
footer. Type is the **Terminus 16** family — regular for body text, a bold
face for page titles, the brand and the selected part.

---

## 2. Transport & tempo

- **PLAY / STOP** (key `-`) starts/stops the sequencer.
- Tempo, swing, accent and loop length are edited on the **FX page**:
  KNOB1 = tempo (60–250 BPM), KNOB2 = swing, KNOB3 = accent, KNOB4 = loop length.
- The same four are available in **GLO mode** (latch GLO, then the knobs).

---

## 3. Sequencing

- The SEQ page shows 16 step cells and a moving play-cursor.
- **REC + key** toggles a step for the selected part (write mode).
- **PART ↑/↓** selects the part (0–4 drums, 5–7 synth).
- **PAGE ←/→** changes loop length (16/32/48/64).
- **SEQ + F3** clears the whole pattern.
- **EUCLID** (`GLO+SEQ`) writes evenly-spaced rhythm patterns (see §12).

---

## 4. Shaping the sound

Four knobs edit the selected part by default (A decay/muffle, B pitch/tone,
C modulation, D level). On **LFO/ENV** pages they edit the shared FM sound.

On **WAVE/EDIT**, **OCT+** cycles the oscillator mode for the selected synth part:

| Mode | Meaning |
|------|---------|
| NORM | standard FM |
| SYNC | hard sync — carrier restarts each OP4 cycle |
| RING | ring/AM — carrier scaled by OP4 output |
| PD | phase distortion (CZ-style) — amount follows the C/mod knob |
| VEC | vector synthesis — morphs four adjacent waves via a slow 2D sweep |

**CC 98** sets the same mode externally (0–4).

---

## 5. Patch banks & presets

Six banks: **APP** · **DX7** · **ROM** · **4OP-A** · **4OP-B** · **LOST**.
The LOST bank is a curated set of hand-authored "lost ROMpler" bread-and-butter
patches (FM-EP, Motown Bass, Glass Pad, Breath Brass, …) with a 90s workstation
identity.

- **PRESETS** browses sounds on any page except SEQ (SEQ browses demo patterns).
- **GLO + OCT−** cycles category; **GLO+EDIT** opens the SOUNDS browse page
  (KNOB1 category, KNOB2 sound). OCT+ toggles MIDI OUT elsewhere.

---

## 6. Save / load slots (8 slots)

- **SAVE** opens the slot page: KNOB1 slot, KNOB4 action (LOAD/SAVE/CLEAR),
  OCT+ confirm, OCT− back.
- **GLO+REC** loads the current slot directly. Slots persist (localStorage on web).

---

## 7. FX page

Mode cycles with the FX key: **OFF → CHORUS → ECHO → COMBO → STUT** (STUT is an
SP-202/303-style stutter: it captures a window and replays it twice). MIDI OUT
toggle and clock readout also live here. As in §2, the knobs edit the transport.

**Live FX layer (performance):** hold **FX** and press a key to momentarily
trigger a whole-mix effect — release (key or FX) to return to normal audio:

| Key | Effect | Key | Effect |
|-----|--------|-----|--------|
| F3 | Stutter (16th loop roll) | A3 | Bitcrush (4-bit hold) |
| F#3 | Reverse throw | A#3 | Gate (16th square) |
| G3 | Echo throw | B3 | Filter sweep (LP climb) |
| G#3 | Tape stop | C4 | Half speed (octave down) |
| D4 / E4 | REV / ECHO throws | | |

While an effect is running its name shows on the FX page (`PERF: …`).



---

## 8. ARPEGGIATOR (ARP key)

Per-part arpeggiator for synth parts 5–7:

- KNOB1 = on/off · KNOB2 = mode (UP/DN/UD/RND/CHD)
- KNOB3 = octaves (0–3) · KNOB4 = rate (×1/×2/×4)
- OCT+ toggles on/off; hold keys or send MIDI note to feed it.
- COMPOSE pre-loads the lead part's arp bank with its generated chord.

---

## 9. COMPOSE (GLO+ARP) — auto-compose

Pick a genre and the machine writes bass/chord/lead for you. Eight genres:
TECHNO · HOUSE · DUBSTEP · D&B · ELECTRO · AMBIENT · LOFI · ACID.

- KNOB1 = genre (live) · KNOB2 = seed (live) · KNOB3 = density (live)
  · KNOB4 = octave spread (LO/MID/HI)
- **OCT+** builds; **OCT−** undoes the build, then backs out.
- Building applies genre defaults (BPM, swing, accent, FX, arp flavour) —
  previewed on the page.
- **Drift** (`GLO+PLAY` cycles OFF → ON → LOCK): on, the pattern gently
  re-rolls each bar; LOCK freezes it. Notes only; deterministic.
- **Chord-follow**: press **ARP while on COMPOSE** to toggle. When on, the
  generated phrase locks onto the root of whatever chord you are holding.
- Deterministic: same genre/seed/density ⇒ same pattern.

---

## 10. EUCLID (GLO+SEQ) — rhythm generator

Writes a Euclidean (evenly-spaced) gate pattern to the selected part:

- KNOB1 = steps (4–64) · KNOB2 = hits · KNOB3 = rotation
- **OCT+** applies to the selected part (pick a drum, e.g. the kick); OCT− back.

---

## 11. Volca-style FUNC keys

With **GLO latched**, the first 16 white keys run grid-key shortcuts instead of
playing notes (like a Volca's step keys). Unlatch GLO to play normally.

| Key | Command | Key | Command |
|-----|---------|-----|---------|
| F3 (0) | Clear pattern | A#3 (5) | Copy pattern |
| F#3 (1) | Save → slot | B3 (6) | Paste pattern |
| G3 (2) | Load ← slot | C4 (7) | Step mode (REC) |
| G#3 (3) | Pattern − | F#4 (8)…C#5 (15) | reserved |
| A3 (4) | Pattern + | | |

---

## 12. MIDI / external control

**Quick mute:** hold **HOME** for ≥300 ms (tap HOME still opens the system page)
and press keys **F3…C4** to toggle mute of parts 0–7 — great for live dropping
in/out of drum and synth parts. Muted parts show an `M` and dim steps.

Notes: channels 1–3 = the three synth parts; channel 10 = drums (GM).

| CC | Action |
|----|--------|
| 1 / 2 / 5 / 7 | Part A/B/C/D |
| 91 | FX mode (0–3) |
| 92 | Drum kit (0..N) |
| 93 | Patch browse (current bank) |
| 94 | Genre select (0–7) — generates live |
| 95 | Seed (1–255) — generates live |
| 96 | Density (0–127) — generates live |
| 98 | Osc mode (0 NORM / 1 SYNC / 2 RING / 3 PD / 4 VEC) |
| 99 | Live FX trigger (0–7 start: STUT/REV/ECHO/TAPE/CRUSH/GATE/SWEEP/HALF; 127 stop) |
| 123 | All notes off |

Clock (24 ppqn), Start and Stop are emitted over USB + TRS when MIDI OUT is on.

---

## 13. Shortcut map (keys)

| Key | Action |
|-----|--------|
| `Z`…`;` | white keys (F3…G5) |
| `S,D,F,H,J,L` `; '` | black keys |
| `1`…`=` | FX SEL ENV LFO EDIT GLO / HOME SAVE ARP SEQ PLAY REC |
| `[` / `]` | OCT − / OCT + |
| `↑` / `↓` | PART selection |
| `←` / `→` | PAGE selection |

**Combos:** GLO+ARP = COMPOSE · GLO+SEQ = EUCLID · GLO+PLAY = drift ·
GLO+EDIT = SOUNDS · GLO+REC = load slot · GLO+OCT− = category ·
GLO+step keys = FUNC commands · hold FX + keys = momentary live FX ·
hold HOME + keys = mute parts.

---

## 14. Notes & gotchas

- The generative engine is deterministic; its state (genre/seed/density) is
  **not persisted** in slots — it resets on boot.
- Chords are a block triad across three consecutive steps (parts are monophonic).
- While GLO is latched, the knobs control transport — unlatch to edit sounds.
- In the web emulator, hold/latch buttons with Shift+click.

Enjoy — tag feedback on the repository issues.
