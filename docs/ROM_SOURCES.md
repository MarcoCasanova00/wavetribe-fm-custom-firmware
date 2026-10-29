# ROM Source Reference — Melting-pot 0.5

These rare synth and drum-machine ROM collections were documented via public
archives (DBWBP.com) and processed for sound extraction.

## Status key
- **WAVETABLES** — single-cycle waves successfully mined and integrated (64 waves in `mined_roms.h`)
- **DRUMS** — PCM samples extracted into new drum kits (ROCK + ALT in `epl_drum.h`)
- **ARCHIVE** — downloaded but not yet mined (small firmware/OS ROMs)

## Obscure synthesizers (wavetable sources)

### Extracted (in `mined_roms.h`, 64 waves total)
| Source | Origin | Waves mined | Notes |
|--------|--------|------------|-------|
| **Hohner Adam** | German/Arranger | 32 | Largest yield — 10MB dump with multiple 512KB sample ROMs |
| **Nord Lead 1** | Swedish/VA | 16 | EPROM firmware, metallic PWM content |
| **Yamaha TG77** | Japanese/FM | 8 | PCM ROM portion, digital artifacts |
| **Prophet VS** | US/Vector | 8 | Wave ROM, wavetable-friendly content |

> **Caveat on mined waves:** most EPROM dumps are *firmware code*, not audio
> samples. The `mine_eprom.py` tool extracts statistically wave-like regions; these
> are experimental wavetables (indices 256–319) with a harsh, digital character and
> are **not** selected by default. Verified, musical sources are the factory
> wavetables (indices 0–255) and the six WAV-derived drum kits.

### Downloaded but not mined (small firmware-only ROMs, 0 waves extracted)
- **Jen Synx 508** (3.7KB) — Italian poly, firmware ROM only
- **AAC Tiracon 6V** (7.3KB) — German digital, firmware/OS ROM only
- **Crumar Bit One/99/01** (8-19KB) — Italian DCO hybrids, mostly firmware
- **Soviet Cadans S12** (3.7KB) — Soviet synth, firmware ROM only

> Many of these firmware dumps contain program code rather than audio samples, so the miner correctly finds zero candidate waves. The wave-mining tool (`mine_eprom.py`) uses heuristics (zero-DC, dynamic range, cycle variation) to reject noise and firmware data.

## Obscure drum machines

### Extracted
| Source | Result | Notes |
|--------|--------|-------|
| Emu Drumulator (Rock + Alt sets) | ❌ Removed | The archive is **u-Law encoded**, not linear PCM; decoding as PCM produced heavy DC bias (clicks/noise). These kits were dropped to keep audio clean. |
| Linn 9000 raw EPROMs | ❌ Removed | Raw 8-bit bins mis-decoded (DC-biased); dropped for the same reason. |

### Downloaded but not yet processed
- **MXR 185 Drum Computer** — CONTROL 10 + CONTROL 11 BOOT ROM (mechanical rhythm data)
- **Bohm DD16** (878KB) — 512KB sound EPROMs (mined: 16 waves, in `mined_roms.h`)
- **Vermona SK86** (24KB) — 16× 2KB drum ROMs (no minable waves)
- **Cheetah MD16** (24KB) — 256KB single ROM (mined: 2 waves, in `mined_roms.h`)
- **Emu Drumulator OS** — firmware only

### Wave table mining from drum ROMs
- Bohm DD16: 16 waves mined (digital artifacts, gritty textures)
- Cheetah MD16: 2 waves mined (rare British digital character)

## Pentacle picks for hard genres
1. **Vermona SK86** — best for EBM / industrial / primitive gabber (downloaded, pending sample extraction)
2. **MXR 185 Drum Computer** — cold mechanical rhythms (downloaded, firmware ROM)
3. **Jen Synx 508** — aggressive basses + leads (downloaded, firmware only)
4. **AAC Tiracon 6V** — unsettling metallic textures (downloaded, firmware only)
5. **Cheetah MD16** — hardcore / rave / breakbeat percussion (downloaded, 2 waves mined)

## Notes
- Many dumps are system firmware/EPROMs, not playable audio — preprocessing with `mine_eprom.py` is required.
- The miner rejects firmware data (code regions) automatically via heuristic filters.
- Reference URL: https://dbwbp.com/index.php/9-misc/37-synth-eprom-dumps
