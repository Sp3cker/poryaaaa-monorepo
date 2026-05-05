#!/usr/bin/env bash
# Compare a poryaaaa render against an mGBA capture of the same MIDI.
#
# Usage:
#   compare.sh <song_basename>
#       Looks for <basename>_poryaaaa.wav and <basename>_mgba.wav in
#       tools/captures/ and writes results to tools/captures/<basename>_diff/.
#   compare.sh <poryaaaa.wav> <mgba.wav> [out_dir]
#
# Outputs (in <out_dir>):
#   aligned_poryaaaa.wav   - trimmed/aligned/RMS-normalized
#   aligned_mgba.wav       - resampled to 48 kHz, trimmed/aligned/normalized
#   diff.wav               - sample-wise difference, listenable
#   spectrum_poryaaaa.png  - log-frequency spectrogram
#   spectrum_mgba.png      - log-frequency spectrogram
#   spectrum_diff.png      - spectrogram of the diff signal
#   report.txt             - alignment offset, RMS levels, per-band dB deltas

set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
VENV_PY="$HERE/.venv/bin/python"

if [ ! -x "$VENV_PY" ]; then
    echo "error: missing venv at $HERE/.venv" >&2
    echo "       run: python3 -m venv $HERE/.venv && $HERE/.venv/bin/pip install numpy scipy" >&2
    exit 1
fi
if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "error: ffmpeg not on PATH" >&2
    exit 1
fi

if [ $# -eq 1 ]; then
    base=$1
    p="$HERE/captures/${base}_poryaaaa.wav"
    m="$HERE/captures/${base}_mgba.wav"
    out="$HERE/captures/${base}_diff"
elif [ $# -ge 2 ]; then
    p=$1; m=$2
    if [ $# -ge 3 ]; then
        out=$3
    else
        out=$(dirname "$p")/diff_$(basename "$p" .wav)_vs_$(basename "$m" .wav)
    fi
else
    echo "usage: $0 <song_basename>" >&2
    echo "       $0 <poryaaaa.wav> <mgba.wav> [out_dir]" >&2
    exit 2
fi

[ -f "$p" ] || { echo "missing: $p" >&2; exit 1; }
[ -f "$m" ] || { echo "missing: $m" >&2; exit 1; }

mkdir -p "$out"

"$VENV_PY" "$HERE/align_and_metrics.py" "$p" "$m" "$out"

ff=(-y -hide_banner -loglevel error)
spec_filter="showspectrumpic=s=1920x540:mode=combined:scale=log:legend=enabled"

ffmpeg "${ff[@]}" -i "$out/aligned_poryaaaa.wav" -lavfi "$spec_filter" "$out/spectrum_poryaaaa.png"
ffmpeg "${ff[@]}" -i "$out/aligned_mgba.wav"     -lavfi "$spec_filter" "$out/spectrum_mgba.png"
ffmpeg "${ff[@]}" -i "$out/diff.wav"             -lavfi "$spec_filter" "$out/spectrum_diff.png"

echo
echo "Outputs in: $out"
ls -la "$out"
