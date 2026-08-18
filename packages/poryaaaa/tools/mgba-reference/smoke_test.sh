#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
package_dir="$(cd "$script_dir/../.." && pwd)"
decomp="${1:-${HEARTH_TEST_ROOT:-/Users/spencer/dev/hearth-test}}"
recorder="${2:-$package_dir/build/mgba_mp2k_reference}"

if [[ ! -x "$recorder" || ! -f "$decomp/pokeemerald-hearth.gba" \
    || ! -f "$decomp/pokeemerald-hearth.elf" ]]; then
    echo "SKIP: mGBA recorder or external hearth-test ROM/ELF is unavailable"
    exit 0
fi

# Reads one little-endian uint32 from the recorder's canonical WAV header.
read_le32() {
    local bytes
    read -r -a bytes <<<"$(od -An -v -tu1 -j "$2" -N 4 "$1")"
    printf '%u' "$((bytes[0] + (bytes[1] << 8) + (bytes[2] << 16) + (bytes[3] << 24)))"
}

# Guards the ROM-selected frontend rate and requested capture duration.
assert_wav_geometry() {
    local wav="$1"
    local expected_frames="$2"
    local sample_rate data_bytes frames
    sample_rate="$(read_le32 "$wav" 24)"
    data_bytes="$(read_le32 "$wav" 40)"
    frames="$((data_bytes / 4))"
    if [[ "$sample_rate" -ne 65536 || "$frames" -ne "$expected_frames" ]]; then
        echo "FAIL: expected 65536 Hz and $expected_frames frames, got ${sample_rate} Hz and $frames frames: $wav" >&2
        exit 1
    fi
}

square_1="$(mktemp "${TMPDIR:-/tmp}/poryaaaa-mgba-square-1.XXXXXX")"
square_2="$(mktemp "${TMPDIR:-/tmp}/poryaaaa-mgba-square-2.XXXXXX")"
song="$(mktemp "${TMPDIR:-/tmp}/poryaaaa-mgba-se-pc-on.XXXXXX")"
door="$(mktemp "${TMPDIR:-/tmp}/poryaaaa-mgba-se-door.XXXXXX")"
slateport_no_psg="$(mktemp "${TMPDIR:-/tmp}/poryaaaa-mgba-slateport-no-psg.XXXXXX")"
slateport_no_directsound="$(mktemp "${TMPDIR:-/tmp}/poryaaaa-mgba-slateport-no-directsound.XXXXXX")"
trap 'rm -f "$square_1" "$square_2" "$song" "$door" "$slateport_no_psg" "$slateport_no_directsound"' EXIT

"$script_dir/record_voice.sh" \
    --recorder "$recorder" \
    --decomp "$decomp" \
    --voicegroup vs_wild \
    --voice 1 \
    --note 60 \
    --duration-seconds 0.5 \
    --output "$square_1"

"$script_dir/record_voice.sh" \
    --recorder "$recorder" \
    --decomp "$decomp" \
    --voicegroup vs_wild \
    --voice 4 \
    --note 60 \
    --duration-seconds 0.5 \
    --output "$square_2"

"$script_dir/record_voice.sh" \
    --recorder "$recorder" \
    --decomp "$decomp" \
    --song se_pc_on \
    --duration-seconds 1.25 \
    --output "$song"

"$script_dir/record_voice.sh" \
    --recorder "$recorder" \
    --decomp "$decomp" \
    --song se_door \
    --duration-seconds 1.25 \
    --output "$door"

"$script_dir/record_voice.sh" \
    --recorder "$recorder" \
    --decomp "$decomp" \
    --song mus_slateport \
    --mute psg \
    --duration-seconds 2 \
    --output "$slateport_no_psg"

"$script_dir/record_voice.sh" \
    --recorder "$recorder" \
    --decomp "$decomp" \
    --song mus_slateport \
    --mute directsound \
    --duration-seconds 2 \
    --output "$slateport_no_directsound"

if [[ ! -s "$square_1" || ! -s "$square_2" || ! -s "$song" || ! -s "$door" \
    || ! -s "$slateport_no_psg" || ! -s "$slateport_no_directsound" \
    || $(stat -f %z "$square_1") -le 44 || $(stat -f %z "$square_2") -le 44 \
    || $(stat -f %z "$song") -le 44 || $(stat -f %z "$door") -le 44 \
    || $(stat -f %z "$slateport_no_psg") -le 44 \
    || $(stat -f %z "$slateport_no_directsound") -le 44 ]]; then
    echo "FAIL: a reference WAV has no PCM payload" >&2
    exit 1
fi
assert_wav_geometry "$square_1" 32768
assert_wav_geometry "$square_2" 32768
assert_wav_geometry "$song" 81920
assert_wav_geometry "$door" 81920
assert_wav_geometry "$slateport_no_psg" 131072
assert_wav_geometry "$slateport_no_directsound" 131072
if cmp -s "$square_1" "$square_2"; then
    echo "FAIL: distinct PSG voices produced identical mGBA WAVs" >&2
    exit 1
fi
if cmp -s "$slateport_no_psg" "$slateport_no_directsound"; then
    echo "FAIL: PSG and DirectSound mute masks produced identical mus_slateport WAVs" >&2
    exit 1
fi

echo "PASS: MP2K voices, isolated songs, and mus_slateport channel mutes produced distinct mGBA WAVs"
