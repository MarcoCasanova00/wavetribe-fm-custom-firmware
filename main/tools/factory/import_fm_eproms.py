#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Import real 4-operator FM voices from vintage Yamaha EPROM images.

Two sources are supported:

  * TX81Z  - 27512 image, 128 voices.  A voice slot is 78 bytes: a 10 byte
             name, 11 reserved bytes, four 10 byte packed operator blocks
             (OP4, OP2, OP3, OP1) and the 17 packed "common" bytes that map
             to VMEM offsets 40..56.
  * FB-01  - 32 KiB image, three ROM banks of 48 voices.  A voice slot is
             64 bytes: an 8 byte name, four 10 byte packed operator blocks
             and 16 common bytes (algorithm/feedback at slot byte 59).

The packed operator block is the same 10 byte layout the TX81Z uses:

    byte 0  bits 0..4  attack rate          (0..31)
    byte 1  bits 0..4  decay 1 rate         (0..31)
    byte 2  bits 0..4  decay 2 rate         (0..31)
    byte 3  bits 0..3  release rate         (0..15)
    byte 4  bits 0..3  decay 1 level        (0..15)
    byte 5  bits 0..6  level scaling        (0..99)
    byte 6  bits 0..2  key velocity sens    (0..7)
            bits 3..5  EG bias sens         (0..7)
            bit  6     AM enable            (0/1)
    byte 7  bits 0..6  output level         (0..99)
    byte 8  bits 0..5  coarse frequency     (0..63)
    byte 9  bits 0..2  detune               (0..6)
            bits 3..4  rate scaling         (0..3)

The generator also writes the DSP tables used by fm_engine.c: a 1024 point
sine table and the TX81Z coarse/fine frequency ratio table (Q8).
"""

from __future__ import annotations

import argparse
import os
import re
import struct
import sys
from pathlib import Path


# --------------------------------------------------------------------------- tables

def build_sine() -> list[int]:
    """1024 point sine, Q15."""
    import math
    return [int(round(math.sin(2.0 * math.pi * i / 1024.0) * 32767.0))
            for i in range(1024)]


FALLBACK_RATIOS = None  # filled from the table below if the reference is absent


def reference_ratios() -> dict[tuple[int, int], float]:
    """Parse the TX81Z programmer's ratio table when it is available."""
    candidates = [
        Path("/tmp/tx81z-programmer/src/freqratios.c"),
        Path(__file__).resolve().parent / "freqratios.c",
    ]
    for p in candidates:
        if not p.exists():
            continue
        body = p.read_text(errors="ignore").split(
            "VoiceDlg_freqRatios[1024][5] = {", 1)
        if len(body) != 2:
            continue
        body = body[1].split("};", 1)[0]
        out: dict[tuple[int, int], float] = {}
        for m in re.finditer(
                r"\{([^}]*)\},\s*/\*\s*0x([0-9A-Fa-f]{2})\s+0x([0-9A-Fa-f]{2})\s*\*/",
                body):
            s = "".join(re.findall(r"'([^']*)'", m.group(1))).strip()
            if not s:
                continue
            try:
                out[(int(m.group(2), 16), int(m.group(3), 16))] = float(s)
            except ValueError:
                pass
        if len(out) >= 900:
            return out
    return {}


def build_ratios(ref: dict[tuple[int, int], float]) -> list[int]:
    """1024 Q8 ratios, indexed by coarse * 16 + fine."""
    import math
    ratios: list[int] = []
    for coarse in range(64):
        for fine in range(16):
            v = ref.get((coarse, fine))
            if v is None:
                # Coarse 0..3 only document fine 0..7; mirror the low half.
                v = ref.get((coarse, fine & 7))
            if v is None:
                # Smooth continuation: the documented table is roughly
                # 0.5 * 2 ** (coarse / 4) with fine as a linear detune step.
                v = 0.5 * (2.0 ** (coarse / 4.0)) * (1.0 + fine * 0.0625)
            ratios.append(max(1, min(0xFFFF, int(round(v * 256.0)))))
    return ratios


# --------------------------------------------------------------------------- parsing

def slot_name(raw: bytes) -> str:
    s = raw.split(b"\0", 1)[0]
    s = bytes(c if 32 <= c < 127 else 32 for c in s).decode("ascii", "replace")
    return s.strip()


def decode_op(b: bytes) -> dict[str, int]:
    return {
        "ar": b[0] & 0x1F,
        "d1r": b[1] & 0x1F,
        "d2r": b[2] & 0x1F,
        "rr": b[3] & 0x0F,
        "d1l": b[4] & 0x0F,
        "ls": min(99, b[5] & 0x7F),
        "kvs": b[6] & 0x07,
        "ebs": (b[6] >> 3) & 0x07,
        "ame": (b[6] >> 6) & 0x01,
        "out": min(99, b[7] & 0x7F),
        "crs": b[8] & 0x3F,
        "det": b[9] & 0x07,
        "rs": (b[9] >> 3) & 0x03,
    }


def decode_tx81z(data: bytes, base: int = 0x51F9, stride: int = 78,
                 count: int = 128):
    voices = []
    for i in range(count):
        off = base + i * stride
        rec = data[off:off + stride]
        if len(rec) < 78:
            break
        name = slot_name(rec[:10])
        if not name:
            continue
        blocks = [decode_op(rec[21 + 10 * k:31 + 10 * k]) for k in range(4)]
        # EPROM operator order is OP4, OP2, OP3, OP1; the engine uses OP1..OP4.
        ops = [blocks[3], blocks[1], blocks[2], blocks[0]]
        g = rec[61:78]
        alg = g[0] & 0x07
        fb = (g[0] >> 3) & 0x07
        ensure_audible(ops, alg)
        voices.append({
            "name": name, "ops": ops, "alg": alg, "fb": fb,
            "lfo_speed": g[1] & 0x7F,
            "lfo_delay": g[2] & 0x7F,
            "pmd": g[3] & 0x7F,
            "amd": g[4] & 0x7F,
            "lfo_wave": g[5] & 0x03,
            "ams": (g[5] >> 2) & 0x03,
            "pms": (g[5] >> 4) & 0x07,
            "transpose": g[6] & 0x3F,
            "pb_range": g[7] & 0x0F,
            "sustain": (g[8] >> 2) & 0x01,
        })
    return voices


def ensure_audible(ops: list[dict[str, int]], alg: int):
    """Our algorithms always use OP1 as a carrier: never let it be silent."""
    mask = (0x01, 0x03, 0x03, 0x01, 0x01, 0x01, 0x03, 0x0F)[alg & 7]
    carriers = [k for k in range(4) if mask & (1 << k)]
    # Lift weak carriers so every patch has a solid, audible presence.
    total = sum(ops[k]["out"] for k in carriers)
    if total < 150 and carriers:
        scale = 150.0 / max(1, total)
        for k in carriers:
            ops[k]["out"] = min(99, int(round(ops[k]["out"] * scale)))
    if ops[0]["out"] < 55:
        ops[0]["out"] = 90
    if alg == 7 and sum(o["out"] for o in ops) < 16:
        ops[0]["out"] = 90


def decode_fb01(data: bytes):
    """Read the three ROM voice banks (48 patches each).

    FB-01 voice format (64 bytes), per the FB01Editor model:
      0..6  name (7)
      7     user code
      8     LFO speed
      9     b7 loadLFOData; b0..6 amplitude-mod depth
      10    b7 LFO sync; b0..6 pitch-mod depth
      11    bits3..6 operator enable
      12    b0..2 algorithm; b3..5 feedback; b6 right-out; b7 left-out
      13    b4..6 pitch-mod sens; b0..1 amp-mod sens
      14    b5..6 LFO waveform
      15    transpose (signed)
      16..  4 x 8-byte operator blocks, stored OP4,OP3,OP2,OP1
    Operator (8 bytes): b0 total level (0=loudest, inverted); b1 KLS/vel;
      b2 KLS depth / level adjust; b3 detune1 / multiple; b4 rate-scale / AR;
      b5 carrier flag / vel / D1R; b6 detune2 / D2R; b7 sustain / RR.
    """
    def op(b):
        total = b[0] & 0x7F
        return {
            "ar": b[4] & 0x1F,
            "d1r": b[5] & 0x0F,
            "d2r": b[6] & 0x1F,
            "rr": b[7] & 0x0F,
            "d1l": (b[7] >> 4) & 0x0F,
            "ls": (b[2] >> 4) & 0x0F,
            "kvs": (b[1] >> 4) & 0x07,
            "ebs": 0,
            "ame": 0,
            "out": 99 - min(99, (total * 99) // 127),
            "crs": b[3] & 0x0F,          # multiple
            "det": (b[3] >> 4) & 0x07,
            "rs": (b[4] >> 5) & 0x07,
        }
    voices = []
    for base in (0x4380, 0x5BC0, 0x7400):
        for i in range(48):
            off = base + 64 * i
            rec = data[off:off + 64]
            if len(rec) < 64:
                break
            name = slot_name(rec[:7])
            if not name or name.lower().startswith("rom"):
                continue
            blocks = [op(rec[16 + 8 * k:24 + 8 * k]) for k in range(4)]
            # ROM stores OP4,OP3,OP2,OP1; the engine wants OP1..OP4.
            ops = [blocks[3], blocks[2], blocks[1], blocks[0]]
            alg = rec[12] & 0x07
            # FB-01 carrier sets; map to an engine algorithm with the same
            # carriers where possible (engine alg0: OP1 only; alg2: OP1+OP2;
            # alg7: all four).
            carriers = ({1}, {1}, {1}, {1}, {1, 3}, {1, 2, 3}, {1, 2, 3}, {1, 2, 3, 4})[alg]
            if len(carriers) == 1:
                ealg = 0
            elif len(carriers) == 2:
                ealg = 2
            else:
                ealg = 7
            sust = (rec[7] >> 4) & 0x0F
            voices.append({
                "name": name, "ops": ops,
                "alg": ealg, "fb": (rec[12] >> 3) & 0x07,
                "lfo_speed": rec[8] & 0x7F, "lfo_delay": 0,
                "pmd": rec[10] & 0x7F, "amd": rec[9] & 0x7F,
                "lfo_wave": (rec[14] >> 5) & 0x03,
                "ams": rec[13] & 0x03, "pms": (rec[13] >> 4) & 0x07,
                "transpose": 24, "pb_range": 2, "sustain": 0,
                "_sust": sust,
            })
    return voices


# --------------------------------------------------------------------------- emit

def cstr(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def emit_tables(path: Path):
    sine = build_sine()
    ratios = build_ratios(reference_ratios())
    notes = [int(round(440.0 * (2.0 ** ((n - 69) / 12.0)) * (2.0 ** 32) / 44100.0))
             for n in range(128)]
    lines = ["/* GENERATED by tools/factory/import_fm_eproms.py - do not edit. */",
             "#ifndef FM_TABLES_H", "#define FM_TABLES_H", "#include <stdint.h>", "",
             "#define FM_SINE_BITS 10", "#define FM_SINE_SIZE 1024",
             "static const int16_t FM_SINE[FM_SINE_SIZE] = {"]
    for i in range(0, len(sine), 12):
        lines.append("    " + ", ".join(f"{v}" for v in sine[i:i + 12]) + ",")
    lines += ["};", "",
              "/* MIDI note -> 2^32 phase increment at 44100 Hz */",
              "#define FM_NOTE_COUNT 128",
              "static const uint32_t FM_NOTE_INC[FM_NOTE_COUNT] = {"]
    for i in range(0, len(notes), 8):
        lines.append("    " + ", ".join(f"{v}u" for v in notes[i:i + 8]) + ",")
    lines += ["};", "",
              "/* TX81Z coarse/fine frequency ratio, Q8, index = coarse*16+fine */",
              "#define FM_RATIO_COUNT 1024",
              "static const uint16_t FM_RATIO_Q8[FM_RATIO_COUNT] = {"]
    for i in range(0, len(ratios), 16):
        lines.append("    " + ", ".join(f"{v}" for v in ratios[i:i + 16]) + ",")
    lines += ["};", "", "#endif /* FM_TABLES_H */", ""]
    path.write_text("\n".join(lines))


def emit_patches(path: Path, prefix: str, voices: list[dict]):
    n = len(voices)
    fields = ["AR", "D1R", "D2R", "RR", "D1L", "LS", "RS", "EBS", "AME",
              "KVS", "OUT", "CRS", "DET"]
    lines = [f"/* GENERATED by tools/factory/import_fm_eproms.py - do not edit. */",
             f"#ifndef FM_PATCHES_{prefix}_H",
             f"#define FM_PATCHES_{prefix}_H",
             "#include <stdint.h>",
             f"#define FM_PATCH_{prefix}_COUNT {n}u",
             ""]
    lines.append(f"static const char *const FM_PATCH_{prefix}_NAME[{max(n, 1)}] = {{")
    for v in voices:
        lines.append(f"    {cstr(v['name'])},")
    lines += ["};", ""]
    lines.append(f"static const uint8_t FM_PATCH_{prefix}_ALG[{max(n, 1)}] = {{")
    lines.append("    " + ", ".join(str(v["alg"]) for v in voices) + ",")
    lines.append("};")
    lines.append(f"static const uint8_t FM_PATCH_{prefix}_FB[{max(n, 1)}] = {{")
    lines.append("    " + ", ".join(str(v["fb"]) for v in voices) + ",")
    lines.append("};")
    for f in fields:
        key = f.lower()
        lines.append(f"static const uint8_t FM_PATCH_{prefix}_{f}[{max(n, 1)}][4] = {{")
        for v in voices:
            lines.append("    {" + ", ".join(str(o[key]) for o in v["ops"]) + "},")
        lines.append("};")
    for f, key in (("LFO_SPEED", "lfo_speed"), ("LFO_DELAY", "lfo_delay"),
                   ("PMD", "pmd"), ("AMD", "amd"), ("LFO_WAVE", "lfo_wave"),
                   ("AMS", "ams"), ("PMS", "pms"), ("TRANSPOSE", "transpose"),
                   ("PB_RANGE", "pb_range"), ("SUSTAIN", "sustain")):
        lines.append(f"static const uint8_t FM_PATCH_{prefix}_{f}[{max(n, 1)}] = {{")
        lines.append("    " + ", ".join(str(v[key]) for v in voices) + ",")
        lines.append("};")
    lines += ["", f"#endif /* FM_PATCHES_{prefix}_H */", ""]
    path.write_text("\n".join(lines))


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--tx81z", default="/home/maltomare/4op synths/"
                    "Yamaha TX81Z EPROM Firmware (v1.0).zip")
    ap.add_argument("--fb01", default="/home/maltomare/4op synths/"
                    "Yamaha_FB01_ROM.zip")
    ap.add_argument("--out", default=str(Path(__file__).resolve().parents[2]
                                        / "firmware" / "gen"))
    args = ap.parse_args(argv)
    out = Path(args.out)

    import zipfile
    with zipfile.ZipFile(args.tx81z) as z:
        name = next(n for n in z.namelist() if n.upper().endswith(".BIN"))
        tx = z.read(name)
    with zipfile.ZipFile(args.fb01) as z:
        name = next(n for n in z.namelist() if n.lower().endswith(".bin"))
        fb = z.read(name)

    txv = decode_tx81z(tx)
    fbv = decode_fb01(fb)
    emit_tables(out / "fm_tables.h")
    emit_patches(out / "fm_patches_tx81z.h", "TX81Z", txv)
    emit_patches(out / "fm_patches_fb01.h", "FB01", fbv)
    print(f"TX81Z voices: {len(txv)}")
    print(f"FB-01  voices: {len(fbv)}")
    print(f"tables + headers -> {out}")


if __name__ == "__main__":
    main()
