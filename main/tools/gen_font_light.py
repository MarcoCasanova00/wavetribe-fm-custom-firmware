#!/usr/bin/env python3
"""Lighten the firmware's body font (FONT_S) in place.

The font data stores 4-bit alpha nibbles per pixel. "Lighten" thins the
strokes by pulling the anti-aliased edges in: every nibble drops by 2 (floor
0), so faint rim pixels vanish and shapes read narrower/lighter. Metrics
(advance/width/offset) are unchanged, so no layout reflows.

Reads and rewrites firmware/gen/felucca_font.h (only the FONT_S_DATA array).
"""
import os, re, sys

ROOT = os.path.join(os.path.dirname(__file__), "..")
SRC = os.path.normpath(os.path.join(ROOT, "firmware/gen/felucca_font.h"))

text = open(SRC).read()
pat = re.compile(r"(static const uint8_t FONT_S_DATA\[\d+\]\s*=\s*\{)(.*?)(\};)", re.S)
m = pat.search(text)
if not m:
    sys.exit("FONT_S_DATA not found")

vals = [int(t, 0) for t in re.findall(r"0[xX][0-9A-Fa-f]+|\d+", m.group(2))]
new = []
for v in vals:
    hi = (v >> 4) & 15
    lo = v & 15
    hi = 14 if hi >= 12 else (6 if hi >= 6 else (1 if hi else 0))
    lo = 14 if lo >= 12 else (6 if lo >= 6 else (1 if lo else 0))
    new.append((hi << 4) | lo)

def block(items):
    lines = []
    for i in range(0, len(items), 16):
        lines.append("    " + ", ".join("0x%02x" % x for x in items[i:i + 16]) + ",\n")
    return "".join(lines)

replacement = m.group(1) + "\n" + block(new) + m.group(3)
open(SRC, "w").write(text[:m.start()] + replacement + text[m.end():])
print("lightened FONT_S_DATA (%d bytes -> %d bytes)" % (len(vals), len(new)))
