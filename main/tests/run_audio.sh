#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Audio-level regression: build the host, render the deterministic sonic scene,
# and compare the WAV's SHA-256 against the stored golden hash. Catches DSP /
# voice-engine changes that the MIDI-only text logs cannot.
set -e
cd "$(dirname "$0")/.."
mkdir -p tests/golden
tests/run_host.sh tests/scripts/riptide-audio.txt > /dev/null
H=$(sha256sum build/host/dinghy.wav | cut -d' ' -f1)
G=tests/golden/audio.sha256
if [ -f "$G" ]; then
    if [ "$H" = "$(cat "$G")" ]; then
        echo "AUDIO_OK $H"
    else
        echo "AUDIO_MISMATCH got=$H want=$(cat "$G")" >&2
        exit 1
    fi
else
    printf '%s\n' "$H" > "$G"
    echo "AUDIO_GOLDEN stored $H"
fi
