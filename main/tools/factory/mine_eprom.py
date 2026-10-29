#!/usr/bin/env python3
"""Scan any raw EPROM/binary (or ZIP-wrapped dump) for single-cycle wavetable regions."""
import os, struct, sys, math, zipfile, io

def mine_bytes(data, limit):
    """Scan a raw byte buffer for candidate 2048-sample int16 wavetables."""
    cands = []
    for off in range(0, len(data) - 4096, 512):
        chunk = data[off:off + 4096]
        if len(chunk) < 4096:
            break
        vals = struct.unpack("<%dh" % 2048, chunk)
        mn, mx = min(vals), max(vals)
        mean = sum(vals) / len(vals)
        rms = math.sqrt(sum(v * v for v in vals) / len(vals))
        if abs(mean) >= 200:
            continue
        if mn > -800 or mx < 800:
            continue
        # Reject near-silent regions (waves that are mostly zeros/padding)
        nonzero = sum(1 for v in vals if abs(v) > 50)
        if nonzero < len(vals) * 0.3:
            continue
        inc = sum(1 for a, b in zip(vals, vals[1:]) if b > a)
        if inc > len(vals) * 0.9:
            continue
        cands.append((off, rms, mn, mx, mean, vals))
    cands.sort(reverse=True, key=lambda x: x[1])
    picked = []
    seen = set()
    for off, rms, mn, mx, mean, vals in cands:
        if len(picked) >= limit:
            break
        key = sum(vals[:16])
        if key in seen:
            continue
        seen.add(key)
        picked.append((off, mn, rms, vals))
    return picked

def extract_bins(path):
    """Read all raw binaries from a file or ZIP archive."""
    name = os.path.basename(path)
    raw = open(path, "rb").read()
    if not raw[:2] == b'PK':
        return [(name, raw)]
    bins = []
    try:
        zf = zipfile.ZipFile(io.BytesIO(raw))
        for info in zf.infolist():
            if info.filename.endswith("/") or info.is_dir():
                continue
            # Only process raw binary dumps (.BIN/.bin), skip WAV/HEX/MID/IDX etc
            ext = info.filename.rsplit(".", 1)[-1].lower() if "." in info.filename else ""
            if ext in ("wav", "hex", "mid", "midi", "aiff", "aif", "mp3", "txt", "idx", "cue"):
                continue
            data = zf.read(info.filename)
            bins.append((info.filename.rsplit("/", 1)[-1], data))
    except Exception:
        pass
    return bins

def main():
    if len(sys.argv) < 3:
        print("usage: mine_eprom.py IN.bin|IN.zip OUT.h [LIMIT]")
        sys.exit(1)
    inp = sys.argv[1]
    out = sys.argv[2]
    limit = int(sys.argv[3]) if len(sys.argv) > 3 else 16
    bins = extract_bins(inp)
    all_waves = []
    label = os.path.basename(inp)
    for bin_name, data in bins:
        waves = mine_bytes(data, limit)
        all_waves.extend(waves)
        if len(all_waves) >= limit:
            all_waves = all_waves[:limit]
            break
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    with open(out, "w") as f:
        f.write("/* EPROM auto-extract: %s */\n" % label)
        f.write("#ifndef MINED_WAVES_H\n#define MINED_WAVES_H\n")
        f.write("#define MWT_SIZE 2048u\n")
        f.write("#define MWT_COUNT %uu\n" % len(all_waves))
        f.write("static const int16_t MINED_WAVES[MWT_COUNT][MWT_SIZE] = {\n")
        for (off, mn, rms, vals) in all_waves:
            f.write("  { ")
            for k, v in enumerate(vals):
                if k and k % 16 == 0:
                    f.write("\n    ")
                f.write("%d, " % v)
            f.write("},\n")
        f.write("};\n#endif\n")
    print("mined %d waves -> %s" % (len(all_waves), out))

if __name__ == "__main__":
    main()
