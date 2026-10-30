#!/usr/bin/env python3
"""Render the Frankentribe logo (Abuse ANT enemy silhouette) to PNG.

Reads the same 24x26 1bpp raster the firmware ships in abuse_skin.h, scales
it, and paints it in slime green on the firmware's ink background. Output:
docs/logo.png (repo root's docs/ is the GitHub Pages publish directory).
"""
import os, sys
try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow is required")

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "docs", "logo.png")

# abuse_mascot[26][3] from firmware/src/abuse_skin.h
ROWS = [
    0x0ff0, 0x0ff0, 0x3f0cf0, 0x3f0cf0, 0x3cf0f0, 0x3cf0f0,
    0x3ffcfc, 0x3ffcfc, 0x3ffcff, 0x3ffcff, 0x3ffcff, 0x3ffcff,
    0xfffffc, 0xfffffc, 0xfffffc, 0xfffffc, 0xcffffc, 0xcffffc,
    0xf3fff0, 0xf3fff0, 0x0f3fc0, 0x0f3fc0, 0x03f000, 0x03f000,
    0x000000, 0x000000,
]
W, H = 24, 26
SCALE = 8
INK = (20, 14, 38, 255)
SLIME = (86, 220, 118, 255)

img = Image.new("RGBA", (W * SCALE, H * SCALE), INK)
px = img.load()
for y in range(H):
    bits = ROWS[y] << (24 - W)          # left-align the 24 bits
    for x in range(W):
        if bits & (1 << (23 - x)):
            for dy in range(SCALE):
                for dx in range(SCALE):
                    px[x * SCALE + dx, y * SCALE + dy] = SLIME

os.makedirs(os.path.dirname(OUT), exist_ok=True)
img.save(OUT)
print("wrote %s (%dx%d)" % (OUT, W * SCALE, H * SCALE))
