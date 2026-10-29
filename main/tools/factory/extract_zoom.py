#!/usr/bin/env python3
"""Extract Zoom pedal firmware binary from .EXE updater files."""
import os, sys, struct, math

BIN_START_PATTERN = bytes.fromhex("55AA00010400")
BLOCK_SIZE = 4096
SYS_BLOCKS_COUNT = 11
FIRST_DATA_BLOCK = 10

def extract_firmware(exe_path):
    """Extract firmware data from a Zoom .EXE updater file."""
    data = open(exe_path, "rb").read()
    
    # Find the BIN start pattern
    pos = data.find(BIN_START_PATTERN)
    if pos == -1:
        print(f"  No BIN pattern found in {os.path.basename(exe_path)}")
        return None
    
    # Read block count (2 bytes at offset +8 from pattern)
    block_count_raw = data[pos + 8: pos + 10]
    if len(block_count_raw) < 2:
        return None
    bin_blocks = struct.unpack("<H", block_count_raw)[0]
    print(f"  Found at 0x{pos:06x}, {bin_blocks} blocks ({bin_blocks * BLOCK_SIZE / 1024:.1f}KB)")
    
    # Extract all firmware data (system + data blocks)
    total_size = bin_blocks * BLOCK_SIZE
    if pos + total_size > len(data):
        total_size = len(data) - pos
    
    firm_data = data[pos:pos + total_size]
    return firm_data

def mine_wavetables(data, limit=16):
    """Scan raw firmware data for candidate single-cycle wavetable regions."""
    cands = []
    for off in range(0, len(data) - 4096, 512):
        chunk = data[off:off + 4096]
        if len(chunk) < 4096:
            break
        vals = struct.unpack("<%dh" % 2048, chunk)
        mn, mx = min(vals), max(vals)
        mean = sum(vals) / len(vals)
        if abs(mean) >= 200:
            continue
        if mn > -800 or mx < 800:
            continue
        nonzero = sum(1 for v in vals if abs(v) > 50)
        if nonzero < len(vals) * 0.3:
            continue
        rms = math.sqrt(sum(v * v for v in vals) / len(vals))
        cands.append((rms, vals))
    cands.sort(reverse=True, key=lambda x: x[0])
    picked = []
    seen = set()
    for _rms, vals in cands:
        if len(picked) >= limit:
            break
        key = sum(vals[:16])
        if key in seen:
            continue
        seen.add(key)
        mxv, mnv = max(vals), min(vals)
        rng = max(abs(mxv), abs(mnv))
        if rng > 0:
            scale = 28000.0 / rng
            vals = tuple(int(v * scale) for v in vals)
        # Post-normalization filter
        zc = sum(1 for v in vals if v == 0)
        if zc > len(vals) * 0.3:
            continue
        picked.append(vals)
    return picked

def main():
    base = "/home/maltomare/zoom-firmware-lab/firmwares"
    out_dir = "/home/maltomare/wavetribe/main/firmware/gen"
    
    all_waves = []
    sources = []
    
    for dirpath, dirnames, filenames in os.walk(base):
        for fn in filenames:
            if fn.endswith(".exe") and fn.startswith("ZOOM"):
                fpath = os.path.join(dirpath, fn)
                print(f"\n=== {fn} ===")
                data = extract_firmware(fpath)
                if data is None:
                    continue
                
                # Also check for WAV/Patch data
                waves = mine_wavetables(data, 32)
                print(f"  Mined {len(waves)} waves")
                if len(waves) > 0:
                    sources.append(f"{fn} ({len(waves)} waves)")
                    all_waves.extend(waves)
                    
                    # Check for recognizable data patterns
                    non_zero_pct = sum(1 for b in data if b != 0) * 100 / len(data)
                    print(f"  Data: {len(data)} bytes, {non_zero_pct:.1f}% non-zero")
                    
                    # Look for patch/presets
                    for keyword in [b"P00", b"EFF", b"REV", b"DLY", b"CHR", b"FLA", b"PZ"]:
                        idx = data.find(keyword)
                        if idx >= 0:
                            print(f"  Found '{keyword.decode()}' at offset 0x{idx:06x}")
                
                if len(all_waves) >= 64:
                    break
        if len(all_waves) >= 64:
            break
    
    # Take top 64 waves
    all_waves = all_waves[:64]
    
    # Write the header
    lines = []
    lines.append("/* Zoom pedal firmware-mined single-cycle waves */")
    lines.append("/* Sources: %s */" % ", ".join(sources))
    lines.append("#ifndef MINED_ZOOM_H")
    lines.append("#define MINED_ZOOM_H")
    lines.append("#define ZWT_SIZE 2048u")
    lines.append("#define ZWT_COUNT %d" % len(all_waves))
    lines.append("static const int16_t ZOOM_WAVES[ZWT_COUNT][ZWT_SIZE] = {")
    for vals in all_waves:
        row = "  { "
        for k, v in enumerate(vals):
            if k and k % 16 == 0:
                row += "\n    "
            row += "%d, " % v
        row += "},"
        lines.append(row)
    lines.append("};")
    lines.append("#endif")
    
    outpath = os.path.join(out_dir, "mined_zoom.h")
    with open(outpath, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"\nWrote {len(all_waves)} waves -> {outpath}")

if __name__ == "__main__":
    main()
