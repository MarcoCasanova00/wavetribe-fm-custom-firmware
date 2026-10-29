# Frankentribe OS

[![License](https://img.shields.io/github/license/MarcoCasanova00/wavetribe-fm-custom-firmware?color=GPL)](LICENSE)
[![Web Emulator](https://img.shields.io/badge/web-emulator%20live-blue)](https://MarcoCasanova00.github.io/wavetribe-fm-custom-firmware/)

A groovebox firmware for the M-VAVE FM-1, built on Dinghy, with a factory
preset catalog and authentic drum sounds.

**GitHub:** https://github.com/MarcoCasanova00/wavetribe-fm-custom-firmware
**Play in browser:** https://MarcoCasanova00.github.io/wavetribe-fm-custom-firmware/

**Quick start guide:** see [GUIDE.md](GUIDE.md) (v0.1)

## Layout

- **`main/`** — the firmware (C source, tools, docs, tests, the real `.fwsc` build).
- **`docs/`** — the browser emulator (same firmware compiled to WebAssembly), deployed to GitHub Pages.

## Web Emulator (GitHub Pages) ✅ Live!

The browser build lives in **`/docs/`** and is deployed to GitHub Pages.

**Live site:** https://MarcoCasanova00.github.io/wavetribe-fm-custom-firmware/

Press **Switch on** and the whole FM-1 panel comes up: the 240×240 screen, eight knobs,
the fourteen buttons and the 27-key keybed, with the firmware's audio running in an
`AudioWorklet`. The screen, LEDs and sound are produced by the same code the device runs.

Keyboard: `Z`–`'` and `Q`–`O` play the keys, `1`–`=` are the panel buttons, `[` `]` are
OCT− / OCT+, the arrows turn SELECT and PAGE, and the knobs respond to drag or wheel.

(Serve `docs/` locally first to verify — browsers block `file://` for WebAssembly and
AudioWorklet:)

```sh
python3 -m http.server 8000 -d docs
```

## Build

Build the firmware and the web emulator from `main/` (see `main/README.md` and
`main/docs/PORT.md`).

## License

This project is free software: you can redistribute it and/or modify it under the
terms of the **GNU General Public License as published by the Free Software
Foundation, version 3** (or, at your option, any later version). The full license
text is in `main/LICENSE`; `main/LICENSING.md` details credits and third-party parts.

- The firmware platform ("the plumbing") is Copyright (C) 2026 Leo Kuroshita
  (Hügelton Instruments), Dinghy by zvenson, SLOOP by isod89 — GPL-3.0.
- RIPTIDE (the app, the tools) is provided under the same GPL-3.0 terms.
- Factory metadata and samples remain © 2011 vendor. This project is an
  independent, imported port for interoperability; it is
  not affiliated with or endorsed by the vendor.
