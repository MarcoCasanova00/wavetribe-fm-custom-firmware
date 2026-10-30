#!/usr/bin/env python3
"""Append the complete sound catalog (patches, drum kits, wave libraries) to README.md.

Parses the actual firmware catalogs (factory_patchbanks.h, factory_drum_kits.h)
so the list is always exactly what the firmware ships. Run from main/:
    python3 tools/make_soundlist.py

Brand names are stripped for the public README (bank/kit headings and patch
labels are shown under neutral names; the firmware itself keeps its original
labels).
"""
import re, pathlib, sys

root = pathlib.Path("firmware/gen")
out = pathlib.Path("..") / "README.md"

# Neutral display names for brand-derived catalog labels (README only).
BANK_DISPLAY = {"DX7": "Vintage FM"}
KIT_DISPLAY = {
    "TR808": "Kick Machine",
    "TR909": "Dance Machine",
    "CR78": "Rhythm Box",
    "LINN": "Sample Machine",
    "MD110": "Preset Machine",
}
PATCH_SANITIZE = [
    ("DX7_Voice_Rom2", "Voice_Rom2"),
    ("Doepfer_ms404_v1_0", "ms404 v1.0"),
    ("Trinity-", "Workstation-"),
    ("Lexicon_LXP1_v1.06", "FX Unit v1.06"),
    ("Linn_9000_roms_", "DrumMachine_roms_"),
    ("Moog_MemoryMoog_v2", "MemorySynth_v2"),
    ("Nord_Lead_1_", "LeadSynth_1_"),
    ("OSCAR_7_64_TomMora", "MiniMono_64_TomMora"),
    ("Motown Bass", "Soul Bass"),
]

def sanitize_patch(n):
    for a, b in PATCH_SANITIZE:
        n = n.replace(a, b)
    return n

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
    L.append("**%s** — %d patches" % (BANK_DISPLAY.get(bn, bn), cnt))
    for n in names[:cnt]:
        L.append("- %s" % sanitize_patch(n))
    L.append("")
L.append("### Drum kits (%d kits, 5 samples each)" % len(dnames))
L.append("")
for k, (dn, dc) in enumerate(zip(dnames, dcats)):
    tag = (" — " + dc) if dc else ""
    L.append("**%s**%s" % (KIT_DISPLAY.get(dn, dn), tag))
    for part in DRUM_PARTS:
        L.append("- %s" % part)
    L.append("")
L.append("### Wave libraries")
L.append("")
L.append("- **WAVELIB** — %d factory single-cycle waves" % WT)
L.append("- **EPROM mine A** — %d mined single-cycle waves" % MWT)
L.append("- **EPROM mine B** — %d mined single-cycle waves" % ZWT)
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
