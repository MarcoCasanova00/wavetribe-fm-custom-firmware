# Wavetribe

An Electribe-style groovebox firmware for the M-VAVE FM-1, built on Dinghy, with the
KORG iELECTRIBE factory preset catalog and (where possible) authentic sounds.

## Layout

- **`main/`** — the firmware (C source, tools, docs, tests, the real `.fwsc` build).
- **`gh-pages/`** — the browser emulator (same firmware compiled to WebAssembly), deployed to GitHub Pages.

## Play with it

Serve `gh-pages/` and open `index.html`:
```sh
python3 -m http.server 8000 -d gh-pages
```

Build the firmware and the web emulator from `main/` (see `main/README.md` and
`main/docs/ELECTRIBE-PORT.md`).
