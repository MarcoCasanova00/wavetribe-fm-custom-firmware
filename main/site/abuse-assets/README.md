# Abuse — assets & skin pack

Extracted from **Abuse** (DOS, 1995), a dark 2D side-scrolling action game by
**Crack dot Com** (published by Electronic Arts). The Abuse source (including
the `.SPE` "Speckle" codec in `src/imlib/specs.cpp`) was released into the
public domain; this extract is for reference and skinning only, not a
re-extraction of any protected content beyond the freely-shared palettes.

## How these were obtained
- `Abuse_DOS_EN.zip` in the home folder contains the game data (`ART/*.SPE`,
  `LEVELS/*.SPE`, etc.).
- REA MCP opened `ABUSE.EXE` (DOS MZ). Its deep-analysis providers (Ghidra /
  Hopper / IDA / pwntools) are not installed in this environment, so the `.SPE`
  container was decoded directly from the open-source engine's documented layout:
  - header magic `SPEC1.0`, then `uint16` entry count
  - each directory entry: `[type:1][len:1][name:len incl \0][flags:1][size:4 LE][offset:4 LE]`
  - entry types: `2` = Palette, `4` = Image, `5/6` = Fore/Back Tile,
    `7/21` = Character/Character2, etc.
- Palettes (type 2) are 256×3 RGB bytes — dumped to the previews below.

## Files
- `previews/*.png` — 256-colour palette strips (full width = all 256 colours).
- `abuse_global_palette.json` — the global 256-colour palette (from `TITLE.SPE`).
- `abuse-skin.css` — a ready CSS variable set for a web skin in the Abuse look.

## Using the skin
- **Firmware screen:** the `ABUSE` palette has been added to the firmware's
  on-screen palette list (HOME-hold → COLOR → ABUSE) — dark olive base with the
  signature abuse magenta glow, applied to the LFO / Envelope / wave pages and
  their new waveforms & envelope plots.
- **Web panel:** drop `abuse-skin.css` variables into `site/index.html` `:root`
  to retheme the emulator panel.
