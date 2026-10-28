# Wavetribe

[![License](https://img.shields.io/github/license/MarcoCasanova00/wavetribe-fm-custom-firmware?color=GPL)](LICENSE)
[![Firmware](https://img.shields.io/badge/firmware-RIPTIDE%200.2-orange)]()

An groovy groovebox firmware for the M-VAVE FM-1, built on Dinghy, with the
the source app factory preset catalog and (where possible) authentic sounds.

**GitHub:** https://github.com/MarcoCasanova00/wavetribe-fm-custom-firmware

## Layout

- **`main/`** — the firmware (C source, tools, docs, tests, the real `.fwsc` build).
- **`gh-pages/`** — the browser emulator (same firmware compiled to WebAssembly), deployed to GitHub Pages.

## Web Emulator (GitHub Pages) ✅ Live!

The browser build lives in **`/docs/`** and is deployed to GitHub Pages.

**Live site:** https://MarcoCasanova00.github.io/wavetribe-fm-custom-firmware/

(Serve `docs/` locally first to verify — browsers block `file://` for WebAssembly:)

```sh
python3 -m http.server 8000 -d docs
```

## Build

Build the firmware and the web emulator from `main/` (see `main/README.md` and
`main/docs/the app-PORT.md`).

## License

This project is free software: you can redistribute it and/or modify it under the
terms of the **GNU General Public License as published by the Free Software
Foundation, version 3** (or, at your option, any later version). The full license
text is in `main/LICENSE`; `main/LICENSING.md` details credits and third-party parts.

- The firmware platform ("the plumbing") is Copyright (C) 2026 Leo Kuroshita
  (Hügelton Instruments), Dinghy by zvenson, SLOOP by isod89 — GPL-3.0.
- RIPTIDE (the app, the tools, this port) is provided under the same GPL-3.0 terms.
- the source app and its assets remain © 2011 vendor This project is an
  independent, importing port for interoperability and documentation; it is
  not affiliated with or endorsed by the vendor.
