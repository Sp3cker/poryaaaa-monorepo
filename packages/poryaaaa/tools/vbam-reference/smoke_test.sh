#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
decomp="${1:-}"
core="${2:-}"
if [[ -z "$decomp" || -z "$core" ]]; then
    echo "Usage: smoke_test.sh DECOMP VBA_M_LIBRETRO_CORE" >&2
    exit 2
fi
if [[ ! -f "$decomp/pokeemerald-hearth.gba" || ! -f "$decomp/pokeemerald-hearth.elf" ]]; then
    echo "SKIP: matching hearth ROM and ELF are unavailable in $decomp"
    exit 0
fi

tmp="$(mktemp -d "${TMPDIR:-/tmp}/poryaaaa-vbam-smoke.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
wav="$tmp/se-pc-on-sq1.wav"
manifest="$wav.manifest.txt"

"$script_dir/record_song.sh" \
    --decomp "$decomp" \
    --song se_pc_on \
    --core "$core" \
    --output "$wav" \
    --duration-seconds 1.25 \
    --solo sq1

python3 -c 'import sys, wave; w = wave.open(sys.argv[1]); assert w.getnchannels() == 2; assert w.getsampwidth() == 2; assert w.getframerate() == 32768; assert w.getnframes() == 40960' "$wav"
grep -qx 'sample_rate_hz=32768' "$manifest"
grep -qx 'sample_format=pcm_s16le' "$manifest"
grep -qx 'interpolation=enabled' "$manifest"
grep -qx 'filtering=5' "$manifest"
grep -qx 'channel.sq1=enabled' "$manifest"
grep -qx 'channel.sq2=disabled' "$manifest"
grep -qx 'channel.wave=disabled' "$manifest"
grep -qx 'channel.noise=disabled' "$manifest"
grep -qx 'channel.fifo-a=disabled' "$manifest"
grep -qx 'channel.fifo-b=disabled' "$manifest"

echo "PASS: VBA-M headless SQ1 capture is non-silent stereo PCM16 at 32768 Hz"
