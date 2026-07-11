#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

write_fake_wav() {
    local output="$1"
    printf 'RIFF\050\000\000\000WAVEfmt \020\000\000\000\001\000\002\000\000\000\001\000\000\000\004\000\004\000\020\000data\004\000\000\000\000\000\000\000' >"$output"
}

run_fake_reference() {
    local output=""
    printf '%s\n' "$@" >"$FAKE_LOG_DIR/reference.args"
    while (($#)); do
        if [[ "$1" == --output ]]; then
            output="$2"
            break
        fi
        shift
    done
    write_fake_wav "$output"
}

run_fake_renderer() {
    local output=""
    printf '%s\n' "$@" >"$FAKE_LOG_DIR/renderer.args"
    while (($#)); do
        if [[ "$1" == --output ]]; then
            output="$2"
            break
        fi
        shift
    done
    write_fake_wav "$output"
}

case "$(basename "$0")" in
    fake-reference) run_fake_reference "$@"; exit 0 ;;
    fake-renderer) run_fake_renderer "$@"; exit 0 ;;
esac

tmp="$(mktemp -d "${TMPDIR:-/tmp}/poryaaaa-capture-pair-test.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
decomp="$tmp/decomp"
output="$tmp/output"
mkdir -p "$decomp/sound/songs/midi" "$output"
printf 'rom' >"$decomp/pokeemerald-hearth.gba"
printf 'elf' >"$decomp/pokeemerald-hearth.elf"
printf 'midi' >"$decomp/sound/songs/midi/se_door.mid"
printf 'midi' >"$decomp/sound/songs/midi/se_pc_on.mid"
printf 'midi' >"$decomp/sound/songs/midi/se_truck_stop.mid"
printf 'se_door.mid: -E -R50 -G_rs_sfx_1 -V080 -P5\nse_pc_on.mid: -E -R50 -G_rs_sfx_1 -V100 -P5\nse_truck_stop.mid: -E -R50 -G_rs_sfx_2 -P4\n' \
    >"$decomp/sound/songs/midi/midi.cfg"
ln -s "$script_dir/test_capture_song_pair.sh" "$tmp/fake-reference"
ln -s "$script_dir/test_capture_song_pair.sh" "$tmp/fake-renderer"

FAKE_LOG_DIR="$tmp" "$script_dir/capture_song_pair.sh" \
    --decomp "$decomp" \
    --song se_door \
    --solo noise \
    --duration-seconds 1.25 \
    --output-dir "$output" \
    --reference-capture "$tmp/fake-reference" \
    --renderer "$tmp/fake-renderer"

for expected in "$output/mgba.wav" "$output/poryaaaa.wav" "$output/manifest.txt"; do
    [[ -s "$expected" ]] || { echo "Missing test output: $expected" >&2; exit 1; }
done
grep -qx -- '--song' "$tmp/reference.args"
grep -qx -- 'se_door' "$tmp/reference.args"
grep -qx -- '--duration-seconds' "$tmp/reference.args"
grep -qx -- '--solo' "$tmp/reference.args"
grep -qx -- 'noise' "$tmp/reference.args"
grep -qx -- 'rs_sfx_1' "$tmp/renderer.args"
grep -qx -- '--sample-rate' "$tmp/renderer.args"
grep -qx -- '65536' "$tmp/renderer.args"
grep -qx -- '--song-volume' "$tmp/renderer.args"
grep -qx -- '80' "$tmp/renderer.args"
grep -qx -- '--reverb' "$tmp/renderer.args"
grep -qx -- '50' "$tmp/renderer.args"
grep -qx -- 'song=se_door' "$output/manifest.txt"
grep -qx -- 'solo=noise' "$output/manifest.txt"
grep -qx -- 'voicegroup=rs_sfx_1' "$output/manifest.txt"
grep -qx -- 'song_volume=80' "$output/manifest.txt"
grep -qx -- 'reverb=50' "$output/manifest.txt"
grep -qx -- 'reference.wav.sample_rate_hz=65536' "$output/manifest.txt"
grep -qx -- 'poryaaaa.wav.sample_rate_hz=65536' "$output/manifest.txt"
grep -Eq '^reference\.wav\.sha256=[0-9a-f]{64}$' "$output/manifest.txt"
grep -Eq '^poryaaaa\.wav\.sha256=[0-9a-f]{64}$' "$output/manifest.txt"

pc_output="$tmp/pc-output"
FAKE_LOG_DIR="$tmp" "$script_dir/capture_song_pair.sh" \
    --decomp "$decomp" \
    --song se_pc_on \
    --output-dir "$pc_output" \
    --reference-capture "$tmp/fake-reference" \
    --renderer "$tmp/fake-renderer"
grep -qx -- 'song=se_pc_on' "$pc_output/manifest.txt"
grep -qx -- 'song_volume=100' "$pc_output/manifest.txt"
grep -qx -- 'solo=full' "$pc_output/manifest.txt"
if grep -qx -- '--solo' "$tmp/reference.args" || grep -qx -- '--solo' "$tmp/renderer.args"; then
    echo "Unexpected --solo argument in full-mix capture" >&2
    exit 1
fi

default_volume_output="$tmp/default-volume-output"
FAKE_LOG_DIR="$tmp" "$script_dir/capture_song_pair.sh" \
    --decomp "$decomp" \
    --song se_truck_stop \
    --output-dir "$default_volume_output" \
    --reference-capture "$tmp/fake-reference" \
    --renderer "$tmp/fake-renderer"
grep -qx -- 'song=se_truck_stop' "$default_volume_output/manifest.txt"
grep -qx -- 'song_volume=127' "$default_volume_output/manifest.txt"
grep -qx -- '127' "$tmp/renderer.args"

echo "PASS: paired capture uses midi.cfg settings and records WAV provenance"
