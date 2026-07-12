#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
package_dir="$(cd "$script_dir/../.." && pwd)"
decomp="${HEARTH_TEST_ROOT:-/Users/spencer/dev/hearth-test}"
output_dir="$package_dir/build/directsound-fixture-comparison"
duration_seconds="1.25"
renderer="${PORYAAAA_RENDER:-$package_dir/build/poryaaaa_render}"
voicegroup="voicegroup_rg_poke_center"
voice="4"
song_voice="4"
voicegroup_file="rg_poke_center"
sample_symbol="DirectSoundWaveData_sd90_classical_detuned_ep1_high"
sample_file="sd90_classical_detuned_ep1_high.bin"

# Describes the one-command DirectSound fixture capture and comparison loop.
usage() {
    cat >&2 <<'EOF'
Usage: capture_directsound_fixture.sh [options]

Options:
  --decomp DIR             Source decomp (default: /Users/spencer/dev/hearth-test)
  --output-dir DIR         Generated artifact directory
  --duration-seconds S     Capture duration (default: 1.25)
  --renderer FILE          poryaaaa_render executable
  --voicegroup NAME        ROM voicegroup symbol
  --voice INDEX            Tone index in the ROM voicegroup
  --song-voice INDEX       Original se_pc_on program to replace
  --voicegroup-file NAME   Source voicegroup filename without .inc
  --sample-symbol NAME     DirectSound WaveData symbol
  --sample-file NAME       DirectSound .bin filename
EOF
}

while (($#)); do
    case "$1" in
        --decomp) decomp="$2"; shift 2 ;;
        --output-dir) output_dir="$2"; shift 2 ;;
        --duration-seconds) duration_seconds="$2"; shift 2 ;;
        --renderer) renderer="$2"; shift 2 ;;
        --voicegroup) voicegroup="$2"; shift 2 ;;
        --voice) voice="$2"; shift 2 ;;
        --song-voice) song_voice="$2"; shift 2 ;;
        --voicegroup-file) voicegroup_file="$2"; shift 2 ;;
        --sample-symbol) sample_symbol="$2"; shift 2 ;;
        --sample-file) sample_file="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

for required in \
    "$renderer" \
    "$package_dir/build/mgba_mp2k_reference" \
    "$decomp/pokeemerald-hearth.gba" \
    "$decomp/pokeemerald-hearth.elf"; do
    if [[ ! -f "$required" ]]; then
        echo "Missing DirectSound fixture input: $required" >&2
        exit 1
    fi
done

mkdir -p "$output_dir"
output_dir="$(cd "$output_dir" && pwd)"

python3 "$script_dir/prepare_directsound_fixture.py" \
    --decomp "$decomp" \
    --output-dir "$output_dir" \
    --voicegroup "$voicegroup" \
    --voice "$voice" \
    --song-voice "$song_voice" \
    --voicegroup-file "$voicegroup_file" \
    --sample-symbol "$sample_symbol" \
    --sample-file "$sample_file"

"$script_dir/build_song_rom.sh" \
    --decomp "$decomp" \
    --song se_pc_on \
    --rom "$output_dir/fixture-base.gba" \
    --elf "$decomp/pokeemerald-hearth.elf" \
    --output "$output_dir/fixture.gba"

"$script_dir/record_voice.sh" \
    --decomp "$decomp" \
    --song se_pc_on \
    --rom "$output_dir/fixture.gba" \
    --require-max-chans 12 \
    --duration-seconds "$duration_seconds" \
    --output "$output_dir/mgba.wav"

"$renderer" "$output_dir/project" directsound_fixture \
    --midi "$output_dir/fixture.mid" \
    --output "$output_dir/poryaaaa.wav" \
    --sample-rate 65536 \
    --song-volume 100 \
    --reverb 50 \
    --polyphony 12 \
    --tail 0 \
    --fadeout 0 \
    --total-duration-seconds "$duration_seconds"

set +e
python3 "$script_dir/align_directsound.py" \
    "$output_dir/mgba.wav" \
    "$output_dir/poryaaaa.wav" \
    --output "$output_dir/comparison.json"
comparison_status=$?
set -e

# Append runtime provenance without obscuring the fixture builder's source manifest.
{
    printf 'capture_manifest_version=1\n'
    printf 'duration_seconds=%s\n' "$duration_seconds"
    printf 'sample_rate_hz=65536\n'
    printf 'fixture_voice_count=1\n'
    printf 'fixture_voice_type=voice_directsound\n'
    printf 'pcm_voice_limit_expected=12\n'
    printf 'mgba_max_chans_observed=12\n'
    printf 'poryaaaa_polyphony_requested=12\n'
    printf 'renderer=%s\n' "$renderer"
    printf 'renderer.sha256=%s\n' "$(shasum -a 256 "$renderer" | awk '{print $1}')"
    printf 'fixture_rom.sha256=%s\n' "$(shasum -a 256 "$output_dir/fixture.gba" | awk '{print $1}')"
    printf 'mgba_wav.sha256=%s\n' "$(shasum -a 256 "$output_dir/mgba.wav" | awk '{print $1}')"
    printf 'poryaaaa_wav.sha256=%s\n' "$(shasum -a 256 "$output_dir/poryaaaa.wav" | awk '{print $1}')"
    printf 'comparison_exit_status=%s\n' "$comparison_status"
} >"$output_dir/capture_manifest.txt"

echo "DirectSound fixture artifacts written to $output_dir"
exit "$comparison_status"
