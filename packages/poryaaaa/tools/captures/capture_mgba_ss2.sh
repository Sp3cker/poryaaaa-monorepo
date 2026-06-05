#!/usr/bin/env bash
set -euo pipefail

# Capture mGBA reference WAVs from a Pokémon Emerald savestate using the
# patched headless build (see mgba-headless-channel-mute/README.md).
#
# Captures one full mix + 8 solo isolations (psg group, directsound group,
# ch1-4, fifo-a, fifo-b) into mgba-ss2/.  Channel names match the ones
# poryaaaa_render --solo accepts so analyze_capture_pairs.sh can pair
# them up.
#
# Defaults capture from the user's standing setup at:
#   /Users/spencer/dev/pokemon-hearth/pokeemerald-hearth.{gba,ss2}
# Override via env vars or args.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
	echo "Usage: $0 [savestate] [rom] [out_dir]"
	echo
	echo "Env overrides: HEADLESS, SAVESTATE, ROM, OUT_DIR, DURATION_SECONDS, SAMPLE_RATE"
	echo "               CHANNELS='full psg directsound ch1 ch2 ch3 ch4 fifo-a fifo-b'"
	exit 0
fi

HEADLESS="${HEADLESS:-tools/captures/mgba-headless-channel-mute/build/mgba-headless}"
SAVESTATE="${1:-${SAVESTATE:-/Users/spencer/dev/pokemon-hearth/pokeemerald-hearth.ss2}}"
ROM="${2:-${ROM:-/Users/spencer/dev/pokemon-hearth/pokeemerald-hearth.gba}}"
OUT_DIR="${3:-${OUT_DIR:-tools/captures/mgba-headless-channel-mute/reference-wavs/mgba-ss2}}"
DURATION_SECONDS="${DURATION_SECONDS:-35}"
SAMPLE_RATE="${SAMPLE_RATE:-44100}"

CHANNELS=(${CHANNELS:-full psg directsound ch1 ch2 ch3 ch4 fifo-a fifo-b})

if [[ ! -x "$HEADLESS" ]]; then
	echo "Headless mGBA not built: $HEADLESS" >&2
	echo "Build with the recipe in tools/captures/mgba-headless-channel-mute/README.md" >&2
	exit 1
fi
if [[ ! -f "$SAVESTATE" ]]; then
	echo "Savestate not found: $SAVESTATE" >&2
	exit 1
fi
if [[ ! -f "$ROM" ]]; then
	echo "ROM not found: $ROM" >&2
	exit 1
fi
mkdir -p "$OUT_DIR"

echo "Headless:    $HEADLESS"
echo "Savestate:   $SAVESTATE"
echo "ROM:         $ROM"
echo "Output dir:  $OUT_DIR"
echo "Duration:    ${DURATION_SECONDS}s per channel"
echo "Sample rate: ${SAMPLE_RATE} Hz"
echo "Channels:    ${CHANNELS[*]}"
echo

for channel in "${CHANNELS[@]}"; do
	out="$OUT_DIR/${channel}.wav"
	echo "Capturing $channel -> $out"
	args=(-t "$SAVESTATE" --audio-out "$out" --sample-rate "$SAMPLE_RATE" --duration-seconds "$DURATION_SECONDS")
	if [[ "$channel" != "full" ]]; then
		args+=(--solo "$channel")
	fi
	args+=("$ROM")
	"$HEADLESS" "${args[@]}" >/dev/null 2>&1
	if [[ ! -s "$out" ]]; then
		echo "  ERROR: $out is empty or missing" >&2
		exit 1
	fi
done

echo
echo "Done. WAVs written to: $OUT_DIR"
ls -la "$OUT_DIR"
