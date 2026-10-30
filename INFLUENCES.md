# Influences & Sources

Everything the *Frankentribe OS* firmware (and its web emulator) borrows from,
how much we took, and from where. Honest notes on fidelity are included.

**Legend for "how much":**
- *bytes* = we parsed the actual data at byte/parameter level from the source
- *params* = we extracted structured parameters (voices, envelopes, ratios)
- *assets* = we extracted visuals/audio (palettes, sprites, fonts, samples)
- *ideas* = we took the concept/UX, not code or data

---

## 1. FM voice banks (the synthesis core)

| Source | File / location | What we took | How much | Fidelity |
|---|---|---|---|---|
| **Yamaha TX81Z** EPROM | `~/4op synths/Yamaha TX81Z EPROM Firmware (v1.0).zip` | 128 4-op voices (operator rates, levels, ratios, algorithm, LFO) | bytes | param-accurate. algorithm routing via our engine's table |
| **Yamaha FB-01** ROM | `~/4op synths/Yamaha_FB01_ROM.zip` | 144 4-op voices | bytes | param-accurate (8-byte ops, inverted Total Level, OP4..OP1). *algorithm topology approximated* by carrier-set mapping (not bit-exact) |
| **Yamaha DX7** voices | `~/eeproms/DX7_Voice_Rom2.zip` (bank 1) | 64 voices | params | voice **names** + heuristic parameter mapping (not a byte-exact DX7 decode) |
| **ROM** template bank | `~/eeproms/` (misc) | 32 hand-derived patches | ideas | curated |

- Decoder: `main/tools/factory/import_fm_eproms.py` → `main/firmware/gen/fm_patches_tx81z.h`, `fm_patches_fb01.h`.

---

## 2. Wavetables, drums, FX (the sound sources)

| Source | What we took | How much |
|---|---|---|
| **Absynth** banks (`~/new absynth banks/`) | single-cycle wavetables | assets (waves) |
| **FunkBox** (iOS) drums/waves | drum samples → single-cycle waves | assets |
| **Linn** drum kits | kit samples/mappings | assets |
| **Zoom** FX ideas | FX chain concepts (chorus/echo) | ideas |

---

## 3. UI / visual skin (Abuse)

From **Abuse** (Crack dot Com, DOS 1995) — `~/Abuse_DOS_EN.zip`.

| What | Extracted | Used as |
|---|---|---|
| 256-colour palettes | `ART/*.SPE` (type-2 palette entries) | the **"default"** (magenta-olive), **ALIEN**, **LAVA** screen themes; full palette JSON + previews |
| Enemy sprite | `ART/ANT.SPE` | **mascot** emblem on the synth menu pages (silhouette, tinted) |
| Fonts | `ART/LETTERS.SPE`, `ART/CONSFNT.SPE` | decoded to PNG asset sheets (available for future use) |
| "Different look" | — | page chrome (double rule, corner ticks), olive/magenta mood |

- Asset pack: `main/site/abuse-assets/` (palettes, sprites, fonts, CSS skin).
- Abuse's source/codec is public domain; the `.SPE` format was decoded with REA (opened the DOS exe) + the open-source abuse engine.
- **Effect on the synth:** none. Abuse influences only the visual skin.

---

## 4. Reference / inspiration only (no data taken)

| Source | What we borrowed | How much |
|---|---|---|
| **Hortator** (DEADACTIVE FM-1 firmware, `https://deadactive.github.io/hortator/`) | UI pattern: *"four knobs edit the four values on screen per page"* + dedicated LFO / ENV menus | ideas |
| **KORG iELECTRIBE** `~/KORG_iELECTRIBE_1.5.1_ios_4.2.ipa` | currently **unused** (listed for completeness) | — |
| Other EPROMs in `~/eeproms/` (AAC-Tiracon, Bohm-DD16, Cheetah-MD16, Crumar-Bit, Doepfer ms404, …) | **untapped** — candidates for further banks | — |

---

## 5. Licensing notes
- **Abuse** assets: source released into the *public domain* (Crack dot Com); extracted palettes/sprites/fonts here are for skinning/reference.
- **Yamaha** EPROM/ROM voice data: extracted for hobby/research use; trademarks belong to Yamaha.
- **Absynth/FunkBox/Linn/Zoom**: third-party sounds used as inspiration/curated samples for a personal instrument project.
- **Hortator**: GPL-3.0; we took only interaction ideas, no code.

---

*Generated: 2026-10-30. This file accompanies `HANDOFF.md`.*
