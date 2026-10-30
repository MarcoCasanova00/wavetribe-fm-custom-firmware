# Patch Format (DOC) - v0.5

This firmware exposes a multi-bank patch store in ROM:

```c
#define N_PATCHBANKS 3u    /* APP, DX7-voices, ROM-bank */
static const uint8_t PATCHBANK_COUNT[N_PATCHBANKS] = { 13, 64, 32 };
```

- **Bank 0 (APP)**: 13 hand-authored synth patches
- **Bank 1 (DX7)**: 64 voice names extracted from the DX7 Voice ROM
- **Bank 2 (ROM)**: 32 patches derived from raw EPROM byte-sums

Navigation (firmware menus):
- `GLO` + `PRESET` cycles through the current bank's patches
- `OCT-` switches banks (APP → DX7 → ROM → back to APP)

Each patch controls 3 FM parts × 4 params (`A` wavetable, `B` ratio, `C` mod, `D` level).

## MIDI CC map (so you can drive it externally)
| CC | Target |
|----|--------|
| 1 | Part A (wavetable) |
| 2 | Part B (ratio) |
| 5 | Part C (mod) |
| 7 | Part D (level) |
| 91 | FX mode (0..3) |
| 92 | Drum kit (0..N) |
| 93 | Patch selector — cycles current bank (0..N-1) |
| 94 | Genre select (0..N_GENRE-1) — generates live |
| 95 | Seed (1..255) — generates live (0 = no-op) |
| 96 | Density (0..127) — generates live |
| 123 | All notes off |

Genre CCs (94–96) generate immediately on receipt and follow the same deterministic
seed rule as the COMPOSE page (same genre/seed/density ⇒ same pattern).

MIDI out (notes + clock) on **OCT+** toggle (USB + TRS).