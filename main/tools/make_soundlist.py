#!/usr/bin/env python3
"""Append the complete sound catalog (patches, drum kits, wave libraries) to README.md.

Parses the actual firmware catalogs (factory_patchbanks.h, factory_drum_kits.h)
so the list is always exactly what the firmware ships. Run from main/:
    python3 tools/make_soundlist.py
"""
import re, pathlib, sys

root = pathlib.Path("firmware/gen")
out = pathlib.Path("..") / "README.md"

def arr(text, name):
    m = re.search(name + r"\[[^\]]*\]\s*=\s*\{(.*?)\};", text, re.S)
    if not m:
        sys.exit("array not found: " + name)
    return re.findall(r'"([^"]*)"', m.group(1))

def top_groups(block):
    groups, i = [], 0
    while True:
        i = block.find("{", i)
        if i < 0:
            break
        depth, j = 0, i
        while True:
            c = block[j]
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    groups.append(block[i + 1:j])
                    i = j + 1
                    break
            j += 1
    return groups

patch = (root / "factory_patchbanks.h").read_text()
m = re.search(r"PATCHBANK_COUNT\[[^\]]*\]\s*=\s*\{(.*?)\};", patch, re.S)
tokens = [x.strip() for x in m.group(1).split(",")]
defs = {}
for h in ("fm_patches_tx81z.h", "fm_patches_fb01.h", "factory_rompler.h"):
    t = (root / h).read_text()
    for mm in re.finditer(r"#define\s+(\w+)\s+(\d+)", t):
        defs[mm.group(1)] = int(mm.group(2))
counts = []
for x in tokens:
    if x.lstrip("-").isdigit():
        counts.append(int(x))
    elif x in defs:
        counts.append(defs[x])
    else:
        sys.exit("unknown count token: " + x)
bnames = arr(patch, "PATCHBANK_NAME")
m = re.search(r"PATCH_NAME3\[[^\]]*\]\[[^\]]*\]\s*=\s*\{(.*?)\};", patch, re.S)
groups = top_groups(m.group(1))
if len(groups) != len(bnames):
    sys.exit("bank name/group mismatch: %d vs %d" % (len(groups), len(bnames)))

drums = (root / "factory_drum_kits.h").read_text()
dnames = arr(drums, "DRUM_KIT_NAMES")
dcats = arr(drums, "DRUM_KIT_CATS") if "DRUM_KIT_CATS" in drums else [""] * len(dnames)
DRUM_PARTS = ["KICK", "SNARE", "CLAP", "CHH", "OHH"]

WT, MWT, ZWT = 256, 64, 64

L = []
L.append("")
L.append("## Catalog of sounds")
L.append("")
L.append("Every sound below is selectable in the firmware: browse them in the")
L.append("**Soundbook** (`GLO+HOME`), or pick patches on the SOUNDS page / CC 93.")
L.append("")
L.append("### Patch banks (%d patches)" % sum(counts))
L.append("")
for b, (bn, cnt) in enumerate(zip(bnames, counts)):
    names = [n for n in groups[b].split('","') if n]
    names = [g.strip('"') for g in re.findall(r'"((?:[^"\\\\]|\\\\.)*)"', groups[b])]
    L.append("**%s** — %d patches" % (bn, cnt))
    for n in names[:cnt]:
        L.append("- %s" % n)
    L.append("")
L.append("### Drum kits (%d kits, 5 samples each)" % len(dnames))
L.append("")
for k, (dn, dc) in enumerate(zip(dnames, dcats)):
    tag = (" — " + dc) if dc else ""
    L.append("**%s**%s" % (dn, tag))
    for part in DRUM_PARTS:
        L.append("- %s" % part)
    L.append("")
L.append("### Wave libraries")
L.append("")
L.append("- **WAVELIB** — %d single-cycle waves (Factory/FunkBox/Linn)" % WT)
L.append("- **EPROM BÖHM-DD16** — %d mined waves" % MWT)
L.append("- **ZOOM MS-60B/70CDR** — %d mined waves" % ZWT)
L.append("")

text = out.read_text(encoding="utf-8")
if "## Catalog of sounds" in text:
    # replace the old generated section
    head = text.split("## Catalog of sounds")[0].rstrip() + "\n"
    text = head + "\n".join(L) + "\n"
else:
    text = text.rstrip() + "\n" + "\n".join(L) + "\n"
out.write_text(text, encoding="utf-8")
print("README updated: %d lines of catalog" % len(L))
