# Patch Format (DOC) - v0.3

This firmware exposes a tiny synth-patch store in ROM:

```c
#define N_PATCHES 5u
static const char *const PATCH_NAME[N_PATCHES] = { "Init", "Bright", "Dark", "Pluck", "Soft" };
static const uint8_t PATCH_PRM[N_PATCHES][3][4] = {   /* 3 FM parts x 4 params */
    { { A,B,C,D }, { A,B,C,D }, { A,B,C,D } },
    ...
};
```

* Each FM part stores 4 params: `A` (wavetable index >> 3), `B` (ratio), `C` (mod index), `D` (level).
* In global mode (press GLO) the PRESET encoder cycles patches and applies all 12 values.
* Write your own patch by appending to `PATCH_PRM` in `firmware/gen/factory_patches.h`.

## MIDI CC map (so you can drive it externally)
| CC | Target |
|----|--------|
| 1 | Part A (wavetable) |
| 2 | Part B (ratio) |
| 5 | Part C (mod) |
| 7 | Part D (level) |
| 91 | FX mode (0..3) |
| 92 | Drum kit (0..N) |
| 93 | Patch selector (0..N-1) |
| 123 | All notes off |

MIDI out (notes + channel drains) on **OCT+** toggle.