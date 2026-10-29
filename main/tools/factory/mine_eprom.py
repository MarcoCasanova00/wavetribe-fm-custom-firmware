#!/usr/bin/env python3
"""Scan any raw EPROM/binary for likely single-cycle wavetable regions."""
import os, struct, sys, math
if len(sys.argv) < 3:
    print("usage: mine_eprom.py IN.bin OUT.h [LIMIT]")
    sys.exit(1)
inp = sys.argv[1]
out = sys.argv[2]
limit = int(sys.argv[3]) if len(sys.argv) > 3 else 16
d = open(inp, "rb").read()
cands = []
for off in range(0, len(d) - 2048, 512):
    chunk = d[off:off+2048]
    if len(chunk) < 2048: break
    vals = struct.unpack("<%dh" % (WT := 2048//2), chunk)
    mn, mx = min(vals), max(vals)
    mean = sum(vals)/len(vals)
    rms = math.sqrt(sum(v*v for v in vals)/len(vals))
    if abs(mean) >= 200: continue
    if mn > -800 or mx < 800: continue
    inc = sum(1 for a,b in zip(vals,vals[1:]) if b>a)
    if inc > len(vals)*0.9: continue
    cands.append((off,rms,mn,mx,mean,vals))
cands.sort(reverse=True,key=lambda x:x[1])
picked=[]; seen=set()
for off,rms,mn,mx,mean,vals in cands:
    if len(picked) >= limit: break
    key = sum(vals[:16])
    if key in seen: continue
    seen.add(key)
    picked.append((off,mins:=mn,rms,vals))
os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
with open(out,"w") as f:
    f.write("/* EPROM auto-extract: %s */\n" % os.path.basename(inp))
    f.write("#ifndef MINED_WAVES_H\n#define MINED_WAVES_H\n")
    f.write("#define MWT_SIZE 2048u\n")
    f.write("#define MWT_COUNT %uu\n" % len(picked))
    f.write("static const int16_t MINED'),(' WT_DATA[MWT_COUNT][MWT_SIZE] = {\n")
    for (off,mn,rms,vals) in picked:
        f.write("  { ")
        for k,v in enumerate(vals):
            if k and k%16==0: f.write("\n    ")
            f.write("%d, " % v)
        f.write("},\n")
    f.write("};\n#endif\n")
print("mined %d waves -> %s" % (len(picked),out))
