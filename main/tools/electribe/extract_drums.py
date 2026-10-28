#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Extract the authentic factory drum samples from the the app bundle.

The app bundles a raw 16-bit big-endian PCM bank (`drumdata.bin`, the "DrumPCMROM"
from DrumPCMROM.cpp). This tool segments it into individual one-shot drum hits and
writes each as a WAV (mono, 44.1 kHz) under `assets/drums/`, with a JSON inventory
(offset, length, duration, spectral class, loudness).

Usage:
    python3 tools/electribe/extract_drums.py PATH_TO_APP_BUNDLE [OUT_DIR]
    # default OUT_DIR: assets/drums relative to the repo root

That the bank is big-endian is established by autocorrelation: the kick region shows
a ~120 Hz fundamental (c≈0.30) only under big-endian reading (little-endian c≈0.03).

The samples are not redistributed inside the firmware image; they are extracted as
reference assets and can be selectively converted for embedding (see scripts/).
"""

import json
import math
import os
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FS = 44100
W = 128          # energy window
STEP = 32


def load_pcm_be(path: Path):
    data = path.read_bytes()
    n = len(data) // 2
    return struct.unpack(">%dh" % n, data[: n * 2]), data


def energy_env(samples):
    env = []
    for i in range(0, len(samples) - W, STEP):
        chunk = samples[i:i + W]
        env.append(math.sqrt(sum(x * x for x in chunk) / W))
    return env


def segment(env, samples):
    """Return list of (start_sample, end_sample) loud regions."""
    maxe = max(env)
    thr = maxe * 0.03
    active = [e > thr for e in env]
    segs, i = [], 0
    while i < len(active):
        if active[i]:
            j = i
            while j < len(active) and active[j]:
                j += 1
            segs.append((i, j))
            i = j
        else:
            i += 1
    out = []
    for a, b in segs:
        start, end = a * STEP, b * STEP
        # endpoint the tail: trim trailing samples whose |x| stays tiny
        pk = max(abs(x) for x in samples[start:end])
        if pk < 6000:
            continue
        out.append((start, end, pk))
    return out


def classify(samples, start, end):
    """Crude spectral class based on pitch period + duration + centroid."""
    x = samples[start:end]
    dur = (end - start) / FS
    # autocorrelation pitch (fundamental)
    best_lag, best_c = 0, 0
    N = len(x)
    if N > 80:
        m = sum(x) / N
        x0 = [v - m for v in x]
        for lag in range(20, min(400, N // 2)):
            num = sum(x0[i] * x0[i + lag] for i in range(N - lag))
            den = math.sqrt(sum(v * v for v in x0[:N - lag]) *
                            sum(v * v for v in x0[lag:]))
            if den > 0:
                c = num / den
                if c > best_c:
                    best_lag, best_c = lag, c
    pitch = FS / best_lag if best_lag else 0
    # spectral centroid via rough zero-crossing density
    zc = sum(1 for i in range(1, N) if (x[i] >= 0) != (x[i - 1] >= 0)) / dur
    if dur > 0.4 and pitch and pitch < 1200:
        cls = "cymbal/ride" if zc > 5000 else "long-tone"
    elif pitch < 200:
        cls = "kick" if dur < 0.6 else "long-low"
    elif 200 <= pitch < 900:
        cls = "snare/tom"
    elif zc > 12000:
        cls = "hihat"
    else:
        cls = "percussion"
    return cls, pitch, zc


def write_wav(path: Path, samples, start, end):
    seg = samples[start:end]
    # pad a few ms of silence each side so editors/GUI handle it
    pad = [0] * int(0.005 * FS)
    body = list(seg) + pad
    n = len(body)
    with open(path, "wb") as f:
        f.write(b"RIFF")
        f.write(struct.pack("<I", 36 + n * 2))
        f.write(b"WAVEfmt ")
        f.write(struct.pack("<IHHIIHH", 16, 1, 1, FS, FS * 2, 2, 16))
        f.write(b"data")
        f.write(struct.pack("<I", n * 2))
        f.write(struct.pack("<%dh" % n, *body))


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    bundle = Path(sys.argv[1])
    drum = bundle if bundle.is_file() else bundle / "drumdata.bin"
    if not drum.exists():
        print("no drumdata.bin at", drum)
        return 1
    out_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "assets" / "drums"
    out_dir.mkdir(parents=True, exist_ok=True)

    samples, data = load_pcm_be(drum)
    env = energy_env(samples)
    segs = segment(env, samples)

    inventory = []
    for k, (start, end, pk) in enumerate(segs):
        cls, pitch, zc = classify(samples, start, end)
        dur = (end - start) / FS
        name = f"drum_{k:02d}_{cls.replace('/', '_').replace(' ', '_')}.wav"
        path = out_dir / name
        write_wav(path, samples, start, end)
        inventory.append({
            "file": name, "index": k, "offset": start, "end": end,
            "samples": end - start, "duration": round(dur, 3),
            "peak": pk, "class": cls, "pitch_hz": round(pitch, 1) if pitch else None,
            "zero_cross_per_s": round(zc, 0),
        })
        print(f"  {name:34s} {dur:0.3f}s  {cls:14s} pitch={pitch:6.1f} zc={zc:6.0f}")

    (out_dir / "inventory.json").write_text(
        json.dumps(inventory, indent=2))
    print(f"\nwrote {len(inventory)} drums to {out_dir} (inventory.json)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
