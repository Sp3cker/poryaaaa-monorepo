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
if cmp -s "$square_1" "$square_2"; then
    echo "FAIL: distinct PSG voices produced identical mGBA WAVs" >&2
    exit 1
fi
if cmp -s "$slateport_no_psg" "$slateport_no_directsound"; then
    echo "FAIL: PSG and DirectSound mute masks produced identical mus_slateport WAVs" >&2
    exit 1
fi

echo "PASS: MP2K voices, isolated songs, and mus_slateport channel mutes produced distinct mGBA WAVs"
