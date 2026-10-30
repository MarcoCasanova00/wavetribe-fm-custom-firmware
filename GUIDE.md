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
  is erased (the screen shows `SEQ TOOLS · F3=CLEAR` while SEQ is held).

## 4. Shaping the sound (your four knobs)
Knobs 1–4 edit whichever part is selected:

- **Knob 1 (A) — Decay / Muffle** — how long it rings.
- **Knob 2 (B) — Pitch / Tone** (with a drum part: cycles the drum kit).
- **Knob 3 (C) — Modulation / Body**.
- **Knob 4 (D) — Level** — how loud this part is.

## 5. Patch banks & presets
- **Cycle category/bank:** latch **GLO** (Shift+click, or hold key `6`) then tap **OCT-**
  (`[`) — cycles APP → DX7 → ROM → TX81Z → FB01.
- **Cycle sound within category:** while GLO is held, turn **PRESETS**.
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
  MIDI OUT toggle, and the clock readout (BPM · 1/16 · swing%).

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
