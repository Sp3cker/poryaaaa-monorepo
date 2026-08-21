#!/usr/bin/env bash
# Autoresearch benchmark entrypoint: renders one sustained pure-sine DirectSound
# voice through the poryaaaa M4A driver + hw_audio mixer chain and measures how
# close the captured output is to a pure sine wave. Lower (more negative)
# thd_db is a purer sine.
set -euo pipefail
cd "$(dirname "$0")/packages/poryaaaa"

cmake --build build --target sine_purity_capture >/dev/null

exec ./build/sine_purity_capture \
    --output "$PWD/build/sine_purity_capture.wav" \
    --seconds 3 \
    --sample-rate 32768 \
    --block 256 \
    --pcm-mixer ipatix \
    --polyphony 15 \
    --sample-hz 18157 \
    --table-len 284
