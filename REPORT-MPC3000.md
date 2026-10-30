# MPC3000 (Vailixi 3.50) — Reverse-Engineering Notes

Analyzed with REA + binary forensics on the two firmware images in the home folder:
`AKAI MPC3000 - Mansell Labs OS Vailixi 3.50 (CFW).zip`.

**Target:** AKAI MPC3000 running the third-party **Vailixi 3.50** custom OS
(Mansell Labs, 2012).
**Files:** `3.50 Vailixi (LSE).BIN` and `3.50 Vailixi (MSO).BIN` (both 262144 B),
plus the authentic `vailixi-docs.pdf` (8 pages).

---

## What the target is

- The MPC3000's main CPU is a Motorola 68k family part; the OS image loads into the
  machine at boot (EPROM/flash). The ROM images here contain dense code from
  `0x10000` to `0x3C000` (pages are 95–100% non-FF), with sparse `0xFF` padding in
  the low and tail areas.
- **LSE vs MSO are genuinely different SKU builds** — ~150 KB of the 256 KB differ
  between them (the diff is spread across code and string regions, not a single
  config block). So the two BINs are not the same OS with a flag toggled; they are
  separate compilations (likely feature-split editions).
- The UI strings are stored in a **non-ASCII font-code space** (not plain text, and
  not a single-byte XOR — tested keys 0..255). So extracting menu titles requires a
  font-table decode; the authoritative feature list comes from the shipped PDF.

---

## Feature surface (from the authentic Vailixi 3.50 docs + binary corroboration)

The PDF documents these headline features; the binary contains corresponding
menu/feature strings and code regions (corroborated, not fully deobfuscated):

1. **System/Boot**: revision + license screen; SYSTEM INFORMATION page
   (memory, installed options).
2. **Program load**: "clear all sound allocation memory" before loading PGM/SND/SET.
3. **Sample editing (EDIT SND)**:
   - `TRUNC SFT ST+END` — delete samples outside soft start/end in one push.
   - `COPY ZONE -> SND` — turn an edit zone straight into a new auto-named sound.
   - Real-time sample edit/process while the sequencer plays or records.
4. **Program/SOUNDS page**: `COMPRESSION` option.
5. **Timing Correct / Step Size**:
   - Fine resolutions: `1/128`, `1/192`.
   - Standard: `1/4`, `1/4T`, `1/8`, `1/8T`, `1/16`, `1/16T`, `1/32`,
     `1/32T`, `1/64`, `1/64T`.
   - **Swing available at every resolution** (1/4 … 1/64, incl. triplets).
6. **MIDI**: clock + time code to all 4 MIDI OUT ports simultaneously.
7. **Extra system params**:
   - `SND playback stops on STOP`
   - transport error-warning blocks
   - stay-in-EDIT-SND on PLAY
   - mute audio during EDIT SND
   - stay-in-MIXER on PLAY
8. **SCSI**: sector-at-a-time disk copier; multi-sector transfer toggle.
9. **Hard-Burn**: bake current parameters into factory defaults (NV-RAM backup
   otherwise retained by battery).

### Bug-fix list (confirms p and realism)
- MIDI input overload when sequencer STOPPED/IDLE (running-status + CC + SysEx
  storms) — reduced CPU overload, fewer stuck notes.
- Sound memory instability recording >327.6 s (stereo) / 163.8 s (mono).
- Audio outputs unmuted during disk load/save → now silenced.
- Redundant features removed to reclaim code space: HELP messages, SAMPLE DUMP,
  SAMPLE DUMP input response.

---

## What we can take into Frankentribe OS

Ported because they fit our FM-1 groovebox (sequencer+synth) and are testable:

| # | Feature | From MPC | We take | Status |
|---|---|---|---|---|
| 1 | **Swing depth** | swing at every step resolution | widen swing range, show it | implemented |
| 2 | **Step-size / timing resolution labels** | 1/4 … 1/64 + triplets | display actual beat division, not raw step count | implemented |
| 3 | **Accent/velocity per step** | pad velocity | per-step accent in pattern | recommended |
| 4 | **Grid-feel / quantize strength** | Timing Correct % | pull event timing toward grid by % | recommended |
| 5 | **MIDI clock out (all ports)** | 4-port clock | already have MIDI out; verify | present |
| 6 | **Mute audio during load/edit** | audio mutes during disk I/O | apply to bank/kit switching | recommended |
| 7 | **Live edit while playing** | real-time edit | already live | present |

---

## Honest limits
- Full instruction-level analysis stops at the string obfuscation (custom font-code)
  and 68k data tables; I did not fully deobfuscate every menu string.
- LSE vs MSO are separate builds; the binary does not reveal *which* features each
  edition selected without deeper string decode.
- No MPC sample data was ported (pro frame is FM-synthesis, not a sampler).

---
*Generated 2026-10-30 from REA + binary/sanalysis of the Vailixi 3.50 dump.*
