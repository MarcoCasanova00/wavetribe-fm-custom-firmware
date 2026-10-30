# Frankentribe OS — Web Emulator Quick Guide

A hands-on guide to the browser build of the Frankentribe OS firmware (the
Wavetribe FM-1 groovebox as a web emulator). Build the beat, shape the sound, save it.

> **Live:** https://MarcoCasanova00.github.io/wavetribe-fm-custom-firmware/
> Hard-refresh (**Ctrl+Shift+R**) to pick up the latest build.

## 1. Boot
1. Open the live link in a modern Chromium, Firefox or Safari.
2. Click **Switch on**.
3. The 240×240 screen lights up and the sequencer starts.
   (If audio stays silent, click once more — some browsers suspend the context.)

## 2. Transport
- **PLAY / STOP** (key `-`) — start / stop the sequencer.
- **REC** (key `=`) — arms write mode: while lit, the keys write steps instead of playing.
- **GLO** (key `6`) — enters global mode (knobs become Tempo / Swing / Accent / Page).
- **HOME** (key `7`) — opens the HOME system page (theme, kit, save slots).

## 3. Sequencing
- The screen shows 16 step slots and a moving play-cursor for the visible page.
- Hold **REC**, tap a key (F3…G5) to toggle a step for the selected part.
- Repeat for all 8 parts to build a pattern.
- **PART − / PART +** (`↑` / `↓`) selects the part you edit.
- **PAGE** (`←` / `→`) changes loop length (16 / 32 / 48 / 64 steps).
- **Clear the sequence:** hold **SEQ** and press **F3** — the whole current pattern
  is erased (the screen shows `SEQ TOOLS  F3=CLEAR` while SEQ is held).

## 4. Shaping the sound (your four knobs)
Knobs 1–4 edit whichever part is selected:

- **Knob 1 (A) — Decay / Muffle** — how long it rings.
- **Knob 2 (B) — Pitch / Tone** (with a drum part: cycles the drum kit).
- **Knob 3 (C) — Modulation / Body**.
- **Knob 4 (D) — Level** — how loud this part is.

On the **LFO** and **Envelope** pages, the knobs edit the selected sound, and
the changes apply to **all three synth parts** (the sound is shared) — so you'll
hear them whichever part you play. These tweaks are saved with your sound — a
**SAVE / LOAD** slot keeps them.

Held keyboard notes release when you switch page, part, or mode — so a note
can't keep ringing after you move elsewhere.

## 5. Patch banks & presets (GR!MM-style sound browsing)
- **Browse sounds:** turn **PRESETS** on any page except SEQ — it steps the the
  whole list (each bank acts as a category) and shows the patch name. Each part
  remembers its own browse position.
- **Cycle category/bank:** latch **GLO** (Shift+click, or hold key `6`) then tap
  **OCT-**** (`[`) — cycles APP → DX7 → ROM → TX81Z → FB01.
- **SOUNDS page:** press **GLO + EDIT** — a full browse page: **KNOB1** picks the
  category, **KNOB2** the sound. Press **EDIT** again to leave.
- On the **SEQ** page, **PRESETS** cycles the demo pattern (as before).
- The screen shows the category (bank) name + patch name + `sound n/count`.
- Changing a sound releases any ringing note (GR!MM-style): the old patch can't
  keep sustaining after the switch.

## 6. Global mode (holding GLO)
With GLO active, the four knobs become:
- **Tempo** · **Swing** (0–100%, deeper MPC-style feel) · **Accent** · **Page**.

## 7. Save / load slots (8 slots)
- **SAVE** (key `8`) opens the **SAVE / SOUNDS** page.
- On that page: **KNOB 1** picks the slot (1–8, shows `[USED]`/`[empty]`),
  **KNOB 4** picks the action (**LOAD / SAVE / CLEAR**), and **OCT+** confirms
  (OCT− backs out).
- Quick combos: **GLO + REC** loads the current slot directly.
- In the browser emulator the 8 slots are **persisted to localStorage**, so your
  saves survive reloads.

## 8. MIDI OUT
With MIDI out toggled, the sequencer emits clock (24 ppqn), Start (0xFA) and
Stop (0xFC) on the USB + TRS outlets.

## 9. FX
- **FX** (key `1`) opens the FX page: mode (OFF / CHORUS / ECHO / COMBO),
  MIDI OUT toggle, and the clock readout (BPM | 1/16 | swing%).
- The knobs edit the transport right on this page — **KNOB1 = tempo** (60–250 BPM),
  KNOB2 = swing, KNOB3 = accent, KNOB4 = loop length (no need to enter GLO).

## 10. GEN — generative genre engine
- Open the **GEN** page with the **ARP** key, or hold **GLO + ARP** to generate instantly without leaving the page you're on.
- **KNOB1** picks the genre — TECHNO / HOUSE / DUBSTEP / D&B / ELECTRO — and regenerates live.
- **KNOB2** bumps the variation seed (regenerates live). **KNOB3** adjusts note density.
  **KNOB4** sets the octave spread (LO / MID / HI).
- **OCT+** generates a fresh pattern; **OCT−** returns to SEQ.
- Generating also applies **genre-appropriate defaults**: BPM, swing, accent, FX mode, and the lead part's arp mode/rate — the generated chord is fed into the arpeggiator automatically, so switching on ARP arpeggiates it.
- The engine is **deterministic**: the same genre/seed/density always yields the same pattern, so you can find a variation you like and return to it.
- Notes: generated chord tones are a block triad across three consecutive steps (parts are monophonic), and the genre state (genre/seed/density) is **not persisted** — it resets on boot.

## 11. MIDI control (external)
Drive the box over MIDI (USB / TRS). In addition to notes (channels 1–3 = the three
synth parts, channel 10 = drums) the CC map is:

| CC | Action |
|----|--------|
| 1 | Part A (wavetable) |
| 2 | Part B (ratio) |
| 5 | Part C (mod) |
| 7 | Part D (level) |
| 91 | FX mode (0–3) |
| 92 | Drum kit (0..N) |
| 93 | Patch browse (current bank) |
| 94 | Genre select (0–4) — generates live |
| 95 | Seed (1–255) — generates live |
| 96 | Density (0–127) — generates live |

---
## Shortcut map
| Key | Action |
|-----|--------|
| `Z`…`;` | white keys (F3…G5) |
| `S,D,F,H,J,L` `; '` | black keys |
| `1`…`=` | FX SEL ENV LFO EDIT GLO / HOME SAVE ARP SEQ PLAY REC |
| `[` / `]` | OCT − / OCT + |
| `↑` / `↓` | PART selection |
| `←` / `→` | PAGE selection |

Enjoy — tag feedback on the repository issues.
