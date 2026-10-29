#!/usr/bin/env python3
"""Hybrid wavetable extractor: Absynth banks + Linn + generic EPROM scans."""
import os, struct, sys, zipfile, math

NTABLES = 160
WT_SIZE = 2048
HDR = 48

def extract_absynth(path):
    d = open(path, "rb").read()
    out = []
    i = 0
    while True:
        j = d.find(b"wave", i)
        if j < 0: break
        try: be = struct.unpack(">I", d[j+4:j+8])[0]
        except Exception: break
        if 4000 <= be <= 4200:
            payload = d[j+8:j+8+be]
            if len(payload) >= HDR + WT_SIZE*2:
                ln = payload[12] & 0xff
                name = payload[13:13+ln] if (1 <= ln <= 32) else b""
                name = name.split(b"\x00")[0].decode("ascii","replace").strip() or "wave"
                pcm = struct.unpack("<%dh" % WT_SIZE, payload[HDR:HDR+WT_SIZE*2])
                out.append((os.path.basename(path), name, pcm))
        i = j + 1
    return out

def extract_linn(path):
    d = open(path, "rb").read()
    n = len(d) // WT_SIZE
    out = []
    base = os.path.splitext(os.path.basename(path))[0]
    for i in range(n):
        chunk = d[i*WT_SIZE:(i+1)*WT_SIZE]
        chunk = chunk + b"\x00"*(WT_SIZE-len(chunk))
        pcm = tuple((u - 128) * 256 for u in chunk)
        out.append(("Linn 9000", "%s %d" % (base, i), pcm))
    return out

def extract_wav_single(directory, limit=32):
    import wave, os
    out=[]
    tried=0
    for fn in sorted(os.listdir(directory)):
        if len(out)>=limit: break
        if not (fn.lower().endswith(".wav")): continue
        path=os.path.join(directory,fn)
        try:
            w=wave.open(path)
            if w.getnchannels()!=1: w.close(); continue
            n=w.getnframes();
            if n<WT_SIZE:
                w.close()
                continue
            fr=w.readframes(WT_SIZE); w.close()
            pcm=struct.unpack("<%dh"%WT_SIZE,fr)
            out.append(("FunkBox wav", fn[:-4][:28], tuple(pcm)))
        except Exception:
            continue
    return out

def extract_generic(path, limit=8):
    try:
        if zipfile.is_zipfile(path):
            z = zipfile.ZipFile(path)
            targets = [n for n in z.namelist() if n.lower().endswith(".bin") and z.getinfo(n).file_size >= WT_SIZE*2]
        else:
            targets = [path]; z = None
    except Exception:
        return []
    out = []
    seen = set()
    for key in targets:
        try:
            d = z.read(key) if z else open(key,"rb").read()
        except Exception:
            continue
        cands = []
        for off in range(0, len(d)-WT_SIZE, WT_SIZE):
            chunk = d[off:off+WT_SIZE]
            vals = struct.unpack("<%dh"%(WT_SIZE//2), chunk)
            mn, mx = min(vals), max(vals)
            mean = sum(vals) / len(vals)
            rms = (sum(v*v for v in vals)/len(vals))**0.5
            # avoid ramps/code: must be zero-centred, dynamic, not monotonic code
            if abs(mean) >= 200: continue
            if mn > -500 or mx < 500: continue
            # rough monotonicity: reject arrays sorted nearly increasing
            inc = sum(1 for a,b in zip(vals,vals[1:]) if b>a)
            if inc > len(vals)*0.9: continue
            if b"\x00"*8 in chunk: continue
            cands.append((rms, off, vals))
        cands.sort(reverse=True)
        taken=0
        for rms, off, vals in cands:
            if taken >= limit: break
            key = bytes(str(vals[:32]).encode()) # crude de-dup
            if key in seen: continue
            seen.add(key)
            name = os.path.basename(key if not isinstance(key,str) else key).split("\\")[-1] if False else os.path.basename(path)
            if z: name = "%s - %s @%06x" % (os.path.basename(path), os.path.basename(key.replace("\\","/")), off) if False else os.path.basename(path)
            label = "%s @%06x" % (os.path.basename(path), off)
            out.append((os.path.basename(path), label, tuple(vals)))
            taken += 1
    return out

def main():
    out = sys.argv[1]
    abs_dir = sys.argv[2] if len(sys.argv) > 2 else "/home/maltomare/new absynth banks"
    linn_dir = sys.argv[3] if len(sys.argv) > 3 else "/home/maltomare/eeproms/Linn 9000 roms 2"
    rom_files = sys.argv[4:] or [
        "/home/maltomare/eeproms/KorgTrinity.zip",
        "/home/maltomare/eeproms/Nord Lead 1 (27c4001 or 27c040 EPROM) (2.70 is final version_ 2002-11-05).zip",
        "/home/maltomare/eeproms/DX7_Voice_Rom2.zip",
        "/home/maltomare/eeproms/Oberheim - DX v3.01.zip",
        "/home/maltomare/eeproms/Roland D-50 1.9 PA decoder v1.9.zip",
        "/home/maltomare/eeproms/Sequential Circuits (SCI) Prophet VS v1.2 [High and Low].zip",
    ]

    waves = []
    # 1. Absynth banks
    banks = sorted(f for f in os.listdir(abs_dir) if not f.endswith("Zone.Identifier"))
    for bank in banks:
        if len(waves) >= NTABLES: break
        try:
            for b, name, pcm in extract_absynth(os.path.join(abs_dir, bank)):
                if len(waves) >= NTABLES: break
                waves.append((b, name, pcm))
        except OSError:
            continue

    # 2. Linn raw 8-bit waves
    if os.path.isdir(linn_dir):
        for fn in sorted(os.path.join(linn_dir, f) for f in os.listdir(linn_dir) if f.endswith(".bin")):
            if len(waves) >= NTABLES: break
            try:
                for bank, name, pcm in extract_linn(fn):
                    if len(waves) >= NTABLES: break
                    waves.append((bank, name, pcm))
            except OSError:
                continue

    # 3. FunkBox single-cycle extracts
    fb_dir="/tmp/ipa_funkbox/Payload/FunkBox Drum Machine.app"
    if os.path.isdir(fb_dir) and len(waves) < NTABLES:
        for bank,name,pcm in extract_wav_single(fb_dir, limit=32):
            if len(waves) >= NTABLES: break
            waves.append((bank,name,pcm))

    # 4. Generic EPROM scan
    for rf in rom_files:
        if len(waves) >= NTABLES: break
        if not os.path.exists(rf): continue
        for items in [extract_generic(rf, limit=4)]:
            for bank, name, pcm in items:
                if len(waves) >= NTABLES: break
                waves.append((bank, name, pcm))

    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as f:
        f.write("/* GENERATED by tools/factory/import_wavetables.py - do not edit by hand. */\n")
        f.write("#ifndef FACTORY_WAVETABLES_H\n#define FACTORY_WAVETABLES_H\n\n")
        f.write("#define WT_SIZE     2048u\n")
        f.write("#define WT_SHIFT   21u\n")
        f.write("#define WT_COUNT     %uu\n\n" % len(waves))
        for idx,(bank,name,_) in enumerate(waves):
            f.write("/* WT%02u: %s / %s */\n" % (idx, bank[:40], name[:28]))
        f.write("\nstatic const int16_t WT_DATA[WT_COUNT][WT_SIZE] = {")
        for idx,(bank,name,pcm) in enumerate(waves):
            f.write("\n  { ")
            for k, v in enumerate(pcm):
                if k and k % 16 == 0:
                    f.write("\n    ")
                f.write("%d, " % v)
            f.write("},")
        f.write("\n};\n\n#endif /* FACTORY_WAVETABLES_H */\n")
    print("wrote %s with %d waves" % (out, len(waves)))

if __name__ == "__main__":
    main()
