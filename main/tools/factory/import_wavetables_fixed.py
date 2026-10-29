#!/usr/bin/env python3
"""Fixed wavetable import: extracts ACTUAL single cycles, resamples to 2048 samples."""
import os, struct, sys, zipfile, math, wave
from pathlib import Path

WT_SIZE = 2048
TARGET_WAVES = 256

def find_zero_crossings(signal):
    """Find zero crossing indices (positive-going)."""
    crossings = []
    for i in range(1, len(signal)):
        if signal[i-1] <= 0 < signal[i]:  # positive-going zero crossing
            crossings.append(i)
    return crossings

def extract_single_cycles(signal, min_len=32, max_len=8192):
    """Extract individual cycles between positive-going zero crossings."""
    crossings = find_zero_crossings(signal)
    cycles = []
    for i in range(len(crossings)-1):
        start, end = crossings[i], crossings[i+1]
        length = end - start
        if min_len <= length <= max_len:
            cycle = signal[start:end]
            # Normalize to zero mean and reasonable amplitude
            mean = sum(cycle)/len(cycle)
            cycle = [v - mean for v in cycle]
            max_val = max(abs(v) for v in cycle)
            if max_val > 100:  # skip near-silent
                cycles.append(cycle)
    return cycles

def resample_cycle(cycle, target_len=2048):
    """Resample a cycle to target length using linear interpolation."""
    src_len = len(cycle)
    if src_len == target_len:
        return cycle
    result = []
    for i in range(target_len):
        src_pos = i * (src_len - 1) / (target_len - 1)
        idx = int(src_pos)
        frac = src_pos - idx
        if idx + 1 < src_len:
            v = cycle[idx] * (1 - frac) + cycle[idx + 1] * frac
        else:
            v = cycle[idx]
        result.append(v)
    # Normalize amplitude to use full 16-bit range
    max_val = max(abs(v) for v in result)
    if max_val > 0:
        scale = 28000.0 / max_val
        result = [int(v * scale) for v in result]
    return result

def extract_from_audio_file(path, limit_per_file=8):
    """Extract single cycles from a WAV/AIFF file."""
    try:
        w = wave.open(path)
        if w.getnchannels() != 1:
            w.close()
            return []
        frames = w.readframes(w.getnframes())
        w.close()
        if w.getsampwidth() == 2:
            data = struct.unpack("<%dh" % (len(frames)//2), frames)
        else:
            return []
        cycles = extract_single_cycles(list(data))
        # Pick the best cycles (by RMS/energy)
        cycles = [resample_cycle(c) for c in cycles]
        cycles.sort(key=lambda c: sum(v*v for v in c), reverse=True)
        return cycles[:limit_per_file]
    except Exception:
        return []

def extract_from_zip(zip_path, limit_per_zip=16):
    """Extract from ZIP files containing WAV/AIFF/BIN files."""
    cycles = []
    try:
        zf = zipfile.ZipFile(zip_path)
        for name in zf.namelist():
            if not name.lower().endswith(('.wav', '.aiff', '.aif', '.bin')):
                continue
            try:
                data = zf.read(name)
                if name.lower().endswith('.wav'):
                    # Parse WAV
                    import io
                    w = wave.open(io.BytesIO(data))
                    if w.getnchannels() != 1:
                        continue
                    frames = w.readframes(w.getnframes())
                    w.close()
                    if w.getsampwidth() != 2:
                        continue
                    signal = struct.unpack("<%dh" % (len(frames)//2), frames)
                elif name.lower().endswith(('.bin',)):
                    # Raw 16-bit PCM
                    if len(data) % 2 != 0:
                        continue
                    signal = list(struct.unpack("<%dh" % (len(data)//2), data))
                else:
                    continue
                cycles = extract_single_cycles(list(signal))
                cycles = [resample_cycle(c) for c in cycles]
                for c in cycles:
                    if len(cycles) >= TARGET_WAVES:
                        break
                    cycles.append(c)
            except Exception:
                continue
    except Exception:
        pass
    return cycles

def extract_absynth_waves(path, limit=8):
    """Extract single cycles from Absynth wave files."""
    try:
        data = open(path, "rb").read()
        # Find "wave" chunks
        cycles = []
        i = 0
        while True:
            j = data.find(b"wave", i)
            if j < 0: break
            try:
                be = struct.unpack(">I", data[j+4:j+8])[0]
            except: break
            if 4000 <= be <= 5000:
                payload = data[j+8:j+8+be]
                if len(payload) >= 48 + 4096:
                    pcm_data = payload[48:48+4096]
                    if len(pcm_data) >= 4096:
                        signal = struct.unpack("<%dh" % 2048, pcm_data[:4096])
                        cycles.extend(extract_single_cycles(list(signal)))
            i = j + 1
        cycles = [resample_cycle(c) for c in cycles]
        cycles.sort(key=lambda c: sum(v*v for v in c), reverse=True)
        return cycles[:limit]
    except Exception:
        return []

def extract_linn_cycles(path, limit=16):
    """Extract from Linn 9000 raw bin files - try multiple chunk sizes."""
    try:
        data = open(path, "rb").read()
        cycles = []
        # Try different chunk sizes around plausible cycle lengths
        for chunk_size in [256, 512, 1024, 2048, 4096]:
            for off in range(0, len(data) - chunk_size, chunk_size):
                chunk = data[off:off+chunk_size]
                if len(chunk) < 4: continue
                try:
                    signal = struct.unpack("<%dh" % (chunk_size//2), chunk)
                    cycles.extend(extract_single_cycles(list(signal)))
                except: pass
        cycles = [resample_cycle(c) for c in cycles]
        cycles.sort(key=lambda c: sum(v*v for v in c), reverse=True)
        return cycles[:limit]
    except Exception:
        return []

def main():
    if len(sys.argv) < 3:
        print("usage: import_wavetables_fixed.py OUT.h [sources...]")
        sys.exit(1)
    
    out_path = sys.argv[1]
    sources = sys.argv[2:]
    
    all_cycles = []
    seen = set()
    
    for src in sources:
        print(f"Processing: {src}")
        cycles = []
        if os.path.isdir(src):
            for fn in sorted(os.listdir(src)):
                if fn.lower().endswith('.wav'):
                    cycles.extend(extract_from_audio_file(os.path.join(src, fn), 4))
        elif src.lower().endswith('.zip'):
            cycles.extend(extract_from_zip(src))
        elif src.lower().endswith('.wav') or src.lower().endswith('.aiff') or src.lower().endswith('.aif'):
            cycles.extend(extract_from_audio_file(src, 8))
        elif os.path.basename(src).startswith('Absynth') or 'absynth' in src.lower():
            cycles.extend(extract_absynth_waves(src))
        elif 'linn' in src.lower() or '9000' in src.lower():
            cycles.extend(extract_linn_cycles(src))
        elif os.path.isfile(src):
            cycles.extend(extract_from_audio_file(src, 8))
        
        # Deduplicate and add
        for c in cycles:
            key = tuple(c[::32])  # coarse fingerprint
            if key not in seen:
                seen.add(key)
                all_cycles.append(c)
                if len(all_cycles) >= TARGET_WAVES:
                    break
        if len(all_cycles) >= TARGET_WAVES:
            break
    
    all_cycles = all_cycles[:TARGET_WAVES]
    print(f"Total cycles extracted: {len(all_cycles)}")
    
    # Write C header
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "w") as f:
        f.write(f"/* Fixed single-cycle wavetables: {len(all_cycles)} waves */\n")
        f.write("#ifndef FACTORY_WAVETABLES_H\n#define FACTORY_WAVETABLES_H\n")
        f.write(f"#define WT_SIZE     2048u\n")
        f.write(f"#define WT_COUNT    {len(all_cycles)}u\n")
        f.write("static const int16_t WT_DATA[WT_COUNT][WT_SIZE] = {\n")
        for cycle in all_cycles:
            f.write("  { ")
            for i, v in enumerate(cycle):
                if i and i % 16 == 0:
                    f.write("\n    ")
                f.write("%d, " % v)
            f.write("},\n")
        f.write("};\n#endif\n")
    print(f"Wrote {out_path}")

if __name__ == "__main__":
    main()
