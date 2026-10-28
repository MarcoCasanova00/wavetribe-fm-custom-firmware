# RIPTIDE — the Electribe → FM-1 port

RIPTIDE is a groovebox firmware for the **M-VAVE FM-1**, built on
[Dinghy](https://github.com/zvenson/dinghy) (the smallest FM-1 firmware to build on).
It ports what we could from the **KORG iELECTRIBE 1.5.1** iOS app, reverse-engineered
from its `.ipa` (KORG_iELECTRIBE_1.5.1_ios_4.2.ipa) with the REA toolchain.

The repo is laid out as two side-by-side pieces:

- **`main/`** — the firmware: C source, tools, docs, tests, and the real `.fwsc` build.
- **`gh-pages/`** — the browser emulator (the same firmware compiled to WebAssembly),
  deployed to GitHub Pages.

## What we took from the IPA and what we built

| From the IPA (reverse-engineered) | What RIPTIDE does with it |
| --- | --- |
| 160 pattern files (29,320 B each) | **`tools/electribe/make_catalog.py`** reads the name/author/genre header fields → `firmware/gen/electribe_catalog.h` (the 97-preset UI catalog). **`tools/electribe/make_patterns.py`** decodes the 8×64 step-gate grid + 64-byte accent array (validated across all 160 files) → `firmware/gen/electribe_patterns.h`, so the firmware plays **real KORG factory grooves**, not hand-written demos |
| `drumdata.bin` (726 KB) | **`tools/electribe/extract_drums.py`** segments it into **14 authentic one-shot drum hits** (16-bit BE PCM, confirmed by autocorrelation: kick ≈120 Hz only under big-endian) → `assets/drums/*.wav` + `inventory.json`. **`tools/electribe/make_drum_bank.py`** embeds the kick/snare/clap/closed-hat/open-hat as `firmware/gen/electribe_drums.h` — the firmware plays the **authentic Electribe drums** |
| `iELECTRIBE_patternSet.plist`, `initPattern.dat` | named as sources; step/note layout decoded (see above) |
| 436 PNG graphics | **not** embedded (iOS @2x/@3x art); the UI re-creates the Electribe look with the 240×240 vector canvas |
| `lcdFont_iPad.png`, `lcdIcon*.png` | inspected to understand the LCD iconography |

## The firmware (firmware/src/dinghy.c)

* **8 parts** — KICK, SNARE, CLAP, CHH, OHH (**authentic Electribe one-shots**) + BASS, CHORD, LEAD (3 polyphonic FM voices).
* **Step sequencer** — up to 64 steps (page 16/32/48/64), per-part gate + note lanes, a playing-step highlight, swing, accent. Gate patterns load from the real KORG factory presets.
* **Electribe-style screen** — amber-on-black, header (name/genre/tempo), the 8-row × 16-step grid, transport footer.
* **Part parameters** — A (decay/muffle), B (pitch), D (level) per drum; FM voice parameters on the tonal parts; tempo/swing/accent on the GLO page.
* **Persistence** — the working pattern + settings saved to flash when quiet.
* Runs unchanged on the real FM-1 (`./build.sh` → `build/dinghy.fwsc`), the host simulator, and the browser (wasm).

## Controls

| Input | Meaning |
| --- | --- |
| `ENC_SELECT` | select the part (KICK…LEAD) |
| `ENC_PRESETS` | select the pattern (catalog) |
| `ENC_ALGORITHM` | pattern length page (16/32/48/64) |
| `KNOB1..4` | selected part's A/B/C/D |
| `GLO` | global page (knob1 tempo, knob2 swing, knob3 accent, knob4 page) |
| `PLAY` | start / stop |
| `REC` (hold) | write mode: keys 0–15 toggle steps of the part |
| `HOME` (hold) | mute mode: keys 0–7 toggle part mute |
| keys | play the part live (drum = hit, tonal = note) |

## Web emulator (gh-pages)

`gh-pages/` is a static page that runs the **same firmware compiled to WebAssembly**
(Emscripten) in the browser, with the LCD rendered to a canvas and sound via WebAudio.

* `gh/` `tools/build_web.sh` builds `gh-pages/riptide.js` + `gh-pages/riptide.wasm` from the C source.
* `.github/workflows/pages.yml` rebuilds and deploys `gh-pages/` to GitHub Pages on every push to `main` (or on demand). Enable Pages → "GitHub Actions" in the repo settings.
* To test locally: `python3 -m http.server 8000 -d gh-pages` and open `http://localhost:8000`.

The emulator is wired through `tests/host.c`'s `DINGHY_WEB` mode (`web_init`, `web_run`,
`web_fb_ptr`, `web_audio`, `web_key`, `web_btn`, `web_enc`, `web_midi`).

## Extracting the assets (from the extracted IPA)

```sh
# 1. catalog (names/authors/genres)
python3 tools/electribe/make_catalog.py /path/to/iELECTRIBE.app
# 2. the 14 authentic drum one-shots -> assets/drums/
python3 tools/electribe/extract_drums.py /path/to/iELECTRIBE.app
# 3. embed the drum bank into the firmware header
python3 tools/electribe/make_drum_bank.py
# 4. import real KORG factory grooves
python3 tools/electribe/make_patterns.py /path/to/iELECTRIBE.app
```

## Development loop

```sh
tests/run_host.sh tests/scripts/riptide-demo.txt   # screen PNGs + a WAV on this machine
./tools/build_web.sh                              # the browser emulator (gh-pages/)
./build.sh                                        # the .fwsc for the real FM-1
```

## Legal & attribution

* Reverse-engineering of the IPA was done for interoperability/documentation under the
  GPL-3.0 port's scoping; the app and its assets remain © 2011 KORG Inc.
* The extracted drum one-shots (`assets/drums/`) and the factory pattern grids are
  **reference assets** for this port; they are cited and described, and the firmware
  builds (the embedded sample bank) ship under the GPL-3.0 port's own terms as
  described above.
* Firmware platform (the "plumbing") © 2026 Leo Kuroshita (Hügelton Instruments), Dinghy
  by zvenson, SLOOP by isod89 — GPL-3.0. RIPTIDE (the app) as above.
