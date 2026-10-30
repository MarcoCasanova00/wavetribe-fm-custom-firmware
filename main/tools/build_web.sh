#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the Wavetribe browser emulator into the repo's docs/ directory.
# Finds emcc via $EMCC, PATH, or $EMSDK (default ~/emsdk).
#
#   ./tools/build_web.sh
set -e
cd "$(dirname "$0")/.."

EMCC="${EMCC:-}"
if [ -z "$EMCC" ]; then
  if command -v emcc >/dev/null 2>&1; then
    EMCC=emcc
  else
    EMSDK="${EMSDK:-$HOME/emsdk}"
    for c in "$EMSDK/upstream/emscripten/emcc" "$HOME/emsdk/upstream/emscripten/emcc"; do
      if [ -x "$c" ]; then EMCC="$c"; break; fi
    done
  fi
fi
[ -n "$EMCC" ] || { echo "build_web: emcc not found (set EMCC or EMSDK, or add Emscripten to PATH)"; exit 1; }

OUT="$(pwd)/../docs"

# dinghy.c includes platform.c which includes tests/host.c (under DINGHY_WEB);
# host.c provides the web_* EXPORTED_KEEPALIVE functions.
"$EMCC" -O2 -Wall -Wno-unused-function -DDINGHY_WEB -DAPP_VER="\"${APP_VER:-dev}\"" \
  -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -Itests \
  -sMODULARIZE=1 -sEXPORT_NAME='createRiptide' -sENVIRONMENT=web \
  -sALLOW_MEMORY_GROWTH=1 -sNO_EXIT_RUNTIME=1 --no-entry \
  -sEXPORTED_FUNCTIONS='["_web_init","_web_run","_web_fb_ptr","_web_fb_words","_web_leds_lo","_web_leds_hi","_web_glow_lo","_web_glow_hi","_web_key","_web_btn","_web_enc","_web_midi","_web_audio","_web_snap_bytes","_web_snap_count","_web_snap_export","_web_snap_import","_malloc","_free"]' \
  -sEXPORTED_RUNTIME_METHODS='["ccall","cwrap","HEAPU8","HEAPU16"]' \
  firmware/src/dinghy.c firmware/src/fm_engine.c -o "$OUT/riptide.js"

cp site/index.html site/main.js site/worklet.js site/GUIDE.md "$OUT/"
mkdir -p "$OUT/fonts"
cp site/fonts/* "$OUT/fonts/"

echo "web: $OUT/ (index.html, main.js, worklet.js, riptide.js, riptide.wasm, fonts/)"
