# RIPTIDE Web Emulator — Quick Guide (v0.1)

A fast hands-on guide to the browser build of the RIPTIDE firmware.
Build the beat, shape the sounds, listen.

> Live: https://MarcoCasanova00.github.io/wavetribe-fm-custom-firmware/

## 1. Boot
1. Open the live link in a modern Chromium, Firefox or Safari.
2. Click **Switch on**.
3. The 240×240 screen lights up and the sequencer starts.

(If audio stays silent, click again — some browsers suspend the context.)

## 2. Transport
- **PLAY / STOP** (or `1` on the keyboard) — start / stop the sequencer.
- **REC** (`2`) — arms write mode. When lit, the 16 keys write steps instead of playing sounds.
- **GLO** (`5`) — enters global mode: the knobs become **Tempo / Swing / Accent / Page**.
- **HOME** (`8`) held + a key — mutes / unmutes a part.

## 3. Sequencing
- The bottom of the screen shows 16 step slots and a moving play-cursor.
- Hold **REC** and tap a key (F3 to G5) to toggle a step for the selected part.
- Repeat for all 8 parts to build a full 16-step pattern.
- **PART − / PART +** (`Digit1`/`Digit2` base) selects which part you edit.
- **PAGE** changes loop length (16 / 32 / 48 / 64 steps).

## 4. Shaping the sound (your four knobs)
The four right-hand knobs (Knob 1–4) affect whatever part is selected.

- **Knob 1 (A) — Decay / Muffle** — how long it rings.
- **Knob 2 (B) — Pitch / Tone** — bright vs. dark.
- **Knob 3 (C) — Modulation / Body** — the part's voice.
- **Knob 4 (D) — Level** — how loud this part is in the mix.

Idle knobs stay idle until you move them; every move lands on the selected part.

## 5. Presets
- **PRESET + / PRESET −** cycles through 97 factory presets.
- The screen shows the preset's name and the part bank it loads (drums for banks 1–5, FM parts for 6–8).

## 6. Playing live
- With REC off, tapping a key triggers it in real time.
- The keybed (F3 → G5) maps to 27 notes — good for triggering one-shot layers over the pattern.

## 7. Known limits (v0.1)
- FX, SAVE, ARP, SEQ, KNOB BANK, FADER BANK are wired in the browser panel but are
  not backed by firmware behavior in this build.
- No project save to disk yet (settings persist in-browser for the session only).
- Audio works best in Chromium; Firefox / Safari are fine but the latency is a little higher.

## Shortcut map
| Key         | Action                 |
|-------------|------------------------|
| `Z`–`'`     | white keys (F3..G5)    |
| `S,D,..,`   | black keys             |
| `1`–`=`     | FX…REC buttons         |
| `[` / `]`   | OCT − / OCT +          |
| `↑` / `↓`   | PART selection         |
| `←` / `→`   | PAGE selection         |

Enjoy. Tag your feedback on the repository issues.
## 8. Wave banks (experimental)
- The three right-hand FM parts now read their carrier wave from a 128-entry embedded (Absynth + Linn + EPROM waves)
  wavetable bank (single-cycle waves extracted from the supplied Absynth-format banks).
- Knob A (KNOB 1) now also steps through those 32 wave shapes for whichever part you
  have selected (step by 8 gives a full cycle of 32 values).

## 8. Wave banks (experimental)
- The three right-hand FM parts now read their carrier wave from a 128-entry embedded (Absynth + Linn + EPROM waves)
  wavetable bank (single-cycle waves extracted from the supplied Absynth-format banks).
  Knob A (KNOB 1) now also steps through those 32 wave shapes for whichever part you
  have selected (each click advances; hold to sweep).

## 9. Drum kits
- With a drum part selected (parts 1-5), turning Knob B (B) cycles available drum kits.
  Three kits ship + four more: the original authentic APP kit and a FunkBox-style 909 kit.
