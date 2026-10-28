# Wavetribe

[![License](https://img.shields.io/github/license/MarcoCasanova00/wavetribe-fm-custom-firmware?color=GPL)](LICENSE)
[![Firmware](https://img.shields.io/badge/firmware-RIPTIDE%200.2-orange)]()

An Electribe-style groovebox firmware for the M-VAVE FM-1, built on Dinghy, with the
KORG iELECTRIBE factory preset catalog and (where possible) authentic sounds.

**GitHub:** https://github.com/MarcoCasanova00/wavetribe-fm-custom-firmware

## Layout

- **`main/`** — the firmware (C source, tools, docs, tests, the real `.fwsc` build).
- **`gh-pages/`** — the browser emulator (same firmware compiled to WebAssembly), deployed to GitHub Pages.

## Web Emulator (GitHub Pages)

The browser build lives in `gh-pages/`. To publish it to GitHub Pages:

1. Go to **Settings → Pages** → **Build and deployment**.
2. Source: **Deploy from a branch** → branch `main`, folder `/gh-pages`.
3. Save.

The site will be live at `https://MarcoCasanova00.github.io/wavetribe-fm-custom-firmware/`
within 1–2 minutes. Or serve `gh-pages/` locally first to verify:

```sh
python3 -m http.server 8000 -d gh-pages
```

## Build

Build the firmware and the web emulator from `main/` (see `main/README.md` and
`main/docs/ELECTRIBE-PORT.md`).

## License

This project is free software: you can redistribute it and/or modify it under the
terms of the **GNU General Public License as published by the Free Software
Foundation, version 3** (or, at your option, any later version). The full license
text is in `main/LICENSE`; `main/LICENSING.md` details credits and third-party parts.

- The firmware platform ("the plumbing") is Copyright (C) 2026 Leo Kuroshita
  (Hügelton Instruments), Dinghy by zvenson, SLOOP by isod89 — GPL-3.0.
- RIPTIDE (the app, the tools, this port) is provided under the same GPL-3.0 terms.
- KORG iELECTRIBE and its assets remain © 2011 KORG Inc. This project is an
  independent, reverse-engineered port for interoperability and documentation; it is
  not affiliated with or endorsed by KORG.
