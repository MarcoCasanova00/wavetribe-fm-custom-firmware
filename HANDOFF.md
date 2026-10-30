# HANDOFF — Frankentribe OS: FM engine + spiral skin

## Status: FM engine refined, banks verified, spiral skin implemented (web, uncommitted)

## Patch banks (5 total)

| Bank | Name | Count | Source |
|------|------|-------|--------|
| 0 | APP | 13 | Hand-designed |
| 1 | DX7 | 64 | DX7 voice names + heuristic params |
| 2 | ROM | 32 | EPROM source names + heuristic params |
| 3 | TX81Z | 128 | Parsed from Yamaha TX81Z EPROM |
| 4 | FB01 | 128 | Parsed from Yamaha FB-01 EPROM |

## Files

- `main/firmware/gen/fm_patches_tx81z.h` — TX81Z voices (128)
- `main/firmware/gen/fm_patches_fb01.h` — FB-01 voices (128)
- `main/firmware/gen/factory_patchbanks.h` — All 5 banks with names + param arrays
- `main/firmware/src/fm_engine.h` — fm_patch_t, per-op arrays, API
- `main/firmware/src/fm_engine.c` — Fixed-point 4-op FM engine (8 algorithms, 8 waveforms, per-op ADSR)
- `main/firmware/src/dinghy.c` — App/UI, patch loaders, bank cycling, sequencer
- `main/site/main.js` — Web panel: spiral skin (scalable SVG spiral + square screen + ring knobs)
- `main/tools/factory/import_fm_eproms.py` — EPROM decoder/table generator

## FM engine notes (current)

- Macro A selects operator waveform; default value 64 now maps to **sine**
  (rotation `((macro>>4)+4)&7` in `fm_process`, mirrored by `fm_macro_wave_label`).
- `draw_wave_page` now shows FM macro labels ("A: wave sine…", B ratio / C mod / D lvl)
  and the bank patch name via `patch_display_name()` for all 5 banks.
- Host build clean; six host test scripts pass; audio verified audible (RMS/peak).
- Bank 0–2 sweep: patches are audibly distinct (spectral centroids differ per bank).

## Controls

| Control | Action |
|---------|--------|
| OCT-DOWN + GLO | Cycle patch banks (APP→DX7→ROM→TX81Z→FB01) |
| GLO + ENC_PRESETS | Cycle patches within current bank |
| MIDI CC 93 | Select patch by number (bank-dependent count) |

## Build verification

- Host test: `sh tests/run_host.sh` passes, audio tonal
- Web build: `EMSDK=$HOME/emsdk sh tools/build_web.sh` succeeds, docs/ updated
- Last commit `6f9219b`; all new FM-engine + skin work currently **uncommitted**

## Next steps

1. Commit the FM-engine refinements + spiral skin; push to GitHub Pages.
2. (Optional) Verify TX81Z/FB-01 sonic correctness against original hardware references.
3. (Optional) Extract Absynth single-cycle waves for FM carriers.
