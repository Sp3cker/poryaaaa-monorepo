#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
package_dir="$(cd "$script_dir/../.." && pwd)"
reference_capture="${REFERENCE_CAPTURE:-$script_dir/record_voice.sh}"
renderer="${PORYAAAA_RENDER:-$package_dir/build/poryaaaa_render}"
decomp="${HEARTH_TEST_ROOT:-}"
song=""
solo=""
duration_seconds="1.25"
output_dir=""

usage() {
    cat >&2 <<'EOF'
Usage: capture_song_pair.sh --decomp DIR --song NAME [options]

Options:
  --output-dir DIR         Output directory (default: build/mgba-reference-pairs/NAME)
  --duration-seconds S     Requested capture duration (default: 1.25)
  --solo NAME              Solo the same GBA hardware channel/group in both tools
  --reference-capture FILE record_voice.sh-compatible wrapper
  --renderer FILE          poryaaaa_render executable
EOF
}

while (($#)); do
    case "$1" in
        --decomp) decomp="$2"; shift 2 ;;
        --song) song="$2"; shift 2 ;;
        --solo) solo="$2"; shift 2 ;;
        --duration-seconds) duration_seconds="$2"; shift 2 ;;
        --output-dir) output_dir="$2"; shift 2 ;;
        --reference-capture) reference_capture="$2"; shift 2 ;;
        --renderer) renderer="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

if [[ -z "$decomp" || -z "$song" ]]; then
    usage
    exit 2
fi
if [[ ! "$song" =~ ^[A-Za-z0-9_]+$ ]]; then
    echo "Song name must contain only letters, digits, and underscores: $song" >&2
    exit 2
fi
if [[ -n "$solo" && ! "$solo" =~ ^[A-Za-z0-9_-]+$ ]]; then
    echo "Solo name must contain only letters, digits, underscores, and hyphens: $solo" >&2
    exit 2
fi
if [[ ! "$duration_seconds" =~ ^[0-9]+([.][0-9]+)?$ ]] || ! awk -v value="$duration_seconds" 'BEGIN { exit !(value > 0) }'; then
    echo "Duration must be a positive number: $duration_seconds" >&2
    exit 2
fi
if [[ ! -d "$decomp" ]]; then
    echo "Missing external decomp directory: $decomp" >&2
    exit 1
fi

decomp="$(cd "$decomp" && pwd)"
midi_dir="$decomp/sound/songs/midi"
midi="$midi_dir/$song.mid"
midi_cfg="$midi_dir/midi.cfg"
rom="$decomp/pokeemerald-hearth.gba"
elf="$decomp/pokeemerald-hearth.elf"

for required in "$midi" "$midi_cfg" "$rom" "$elf" "$reference_capture" "$renderer"; do
    if [[ ! -f "$required" ]]; then
        echo "Missing required capture input: $required" >&2
        exit 1
    fi
done
if [[ ! -x "$reference_capture" ]]; then
    echo "Reference capture wrapper is not executable: $reference_capture" >&2
    exit 1
fi
if [[ ! -x "$renderer" ]]; then
    echo "poryaaaa renderer is not executable: $renderer" >&2
    exit 1
fi

config_entry="$(awk -F: -v target="$song.mid" '
    {
        name = $1
        gsub(/^[[:space:]]+|[[:space:]]+$/, "", name)
        if (name == target) {
            print $0
            exit
        }
    }
' "$midi_cfg")"
if [[ -z "$config_entry" ]]; then
    echo "Song is missing from $midi_cfg: $song.mid" >&2
    exit 1
fi

config_options="${config_entry#*:}"
voicegroup=""
song_volume="127"
reverb=""
for option in $config_options; do
    case "$option" in
        -G_*) voicegroup="${option#-G_}" ;;
        -V[0-9]*) song_volume="$((10#${option#-V}))" ;;
        -R[0-9]*) reverb="$((10#${option#-R}))" ;;
    esac
done
if [[ -z "$voicegroup" || -z "$reverb" ]]; then
    echo "midi.cfg entry must define -G_ and -R settings: $config_entry" >&2
    exit 1
fi

capture_name="$song"
if [[ -n "$solo" ]]; then
    capture_name+="-solo-$solo"
fi
output_dir="${output_dir:-$package_dir/build/mgba-reference-pairs/$capture_name}"
mkdir -p "$output_dir"
output_dir="$(cd "$output_dir" && pwd)"
reference_wav="$output_dir/mgba.wav"
poryaaaa_wav="$output_dir/poryaaaa.wav"
manifest="$output_dir/manifest.txt"
rm -f "$reference_wav" "$poryaaaa_wav" "$manifest"

reference_command=(
    "$reference_capture"
    --decomp "$decomp"
    --song "$song"
    --duration-seconds "$duration_seconds"
    --output "$reference_wav"
)
poryaaaa_command=(
    "$renderer" "$decomp" "$voicegroup"
    --midi "$midi"
    --output "$poryaaaa_wav"
    --sample-rate 65536
    --song-volume "$song_volume"
    --reverb "$reverb"
    --tail 0
    --fadeout 0
    --total-duration-seconds "$duration_seconds"
)
if [[ -n "$solo" ]]; then
    reference_command+=(--solo "$solo")
    poryaaaa_command+=(--solo "$solo")
fi

printf 'Capturing mGBA reference for %s...\n' "$song" >&2
"${reference_command[@]}"
printf 'Capturing poryaaaa render for %s...\n' "$song" >&2
"${poryaaaa_command[@]}"

sha256_file() {
    shasum -a 256 "$1" | awk '{ print $1 }'
}

read_le16() {
    local bytes
    read -r -a bytes <<<"$(od -An -v -tu1 -j "$2" -N 2 "$1")"
    printf '%u' "$((bytes[0] + (bytes[1] << 8)))"
}

read_le32() {
    local bytes
    read -r -a bytes <<<"$(od -An -v -tu1 -j "$2" -N 4 "$1")"
    printf '%u' "$((bytes[0] + (bytes[1] << 8) + (bytes[2] << 16) + (bytes[3] << 24)))"
}

# Emit the canonical PCM WAV fields written by both capture tools.
wav_metadata() {
    local prefix="$1"
    local wav="$2"
    local riff wave fmt data audio_format channels sample_rate bits data_bytes bytes_per_frame frames duration
    riff="$(dd if="$wav" bs=1 count=4 2>/dev/null)"
    wave="$(dd if="$wav" bs=1 skip=8 count=4 2>/dev/null)"
    fmt="$(dd if="$wav" bs=1 skip=12 count=4 2>/dev/null)"
    data="$(dd if="$wav" bs=1 skip=36 count=4 2>/dev/null)"
    if [[ "$riff" != RIFF || "$wave" != WAVE || "$fmt" != "fmt " || "$data" != data ]]; then
        echo "Unsupported WAV layout: $wav" >&2
        exit 1
    fi
    audio_format="$(read_le16 "$wav" 20)"
    channels="$(read_le16 "$wav" 22)"
    sample_rate="$(read_le32 "$wav" 24)"
    bits="$(read_le16 "$wav" 34)"
    data_bytes="$(read_le32 "$wav" 40)"
    if [[ "$audio_format" -ne 1 || "$channels" -le 0 || "$bits" -le 0 ]]; then
        echo "Expected integer PCM WAV: $wav" >&2
        exit 1
    fi
    if [[ "$sample_rate" -ne 65536 ]]; then
        echo "Expected 65536 Hz WAV, got $sample_rate Hz: $wav" >&2
        exit 1
    fi
    bytes_per_frame="$((channels * bits / 8))"
    frames="$((data_bytes / bytes_per_frame))"
    duration="$(awk -v frames="$frames" -v rate="$sample_rate" 'BEGIN { printf "%.9f", frames / rate }')"
    printf '%s.wav.path=%s\n' "$prefix" "$wav"
    printf '%s.wav.sha256=%s\n' "$prefix" "$(sha256_file "$wav")"
    printf '%s.wav.audio_format=pcm_s%ule\n' "$prefix" "$bits"
    printf '%s.wav.channels=%u\n' "$prefix" "$channels"
    printf '%s.wav.sample_rate_hz=%u\n' "$prefix" "$sample_rate"
    printf '%s.wav.frames=%u\n' "$prefix" "$frames"
    printf '%s.wav.duration_seconds=%s\n' "$prefix" "$duration"
}

for wav in "$reference_wav" "$poryaaaa_wav"; do
    if [[ ! -s "$wav" ]]; then
        echo "Capture did not produce a WAV: $wav" >&2
        exit 1
    fi
done

quote_command() {
    local arg
    for arg in "$@"; do
        printf '%q ' "$arg"
    done
}

decomp_git_head="unavailable"
decomp_git_dirty="unavailable"
if git -C "$decomp" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    decomp_git_head="$(git -C "$decomp" rev-parse HEAD)"
    if [[ -n "$(git -C "$decomp" status --porcelain --untracked-files=normal)" ]]; then
        decomp_git_dirty="true"
    else
        decomp_git_dirty="false"
    fi
fi

{
    printf 'manifest_version=1\n'
    printf 'song=%s\n' "$song"
    printf 'solo=%s\n' "${solo:-full}"
    printf 'requested_duration_seconds=%s\n' "$duration_seconds"
    printf 'sample_rate_hz=65536\n'
    printf 'voicegroup=%s\n' "$voicegroup"
    printf 'song_volume=%s\n' "$song_volume"
    printf 'reverb=%s\n' "$reverb"
    printf 'midi_cfg.entry=%s\n' "$config_entry"
    printf 'decomp.path=%s\n' "$decomp"
    printf 'decomp.git_head=%s\n' "$decomp_git_head"
    printf 'decomp.git_dirty=%s\n' "$decomp_git_dirty"
    printf 'rom.path=%s\n' "$rom"
    printf 'rom.sha256=%s\n' "$(sha256_file "$rom")"
    printf 'elf.path=%s\n' "$elf"
    printf 'elf.sha256=%s\n' "$(sha256_file "$elf")"
    printf 'midi.path=%s\n' "$midi"
    printf 'midi.sha256=%s\n' "$(sha256_file "$midi")"
    printf 'midi_cfg.path=%s\n' "$midi_cfg"
    printf 'midi_cfg.sha256=%s\n' "$(sha256_file "$midi_cfg")"
    printf 'reference_capture.path=%s\n' "$reference_capture"
    printf 'reference_capture.sha256=%s\n' "$(sha256_file "$reference_capture")"
    printf 'renderer.path=%s\n' "$renderer"
    printf 'renderer.sha256=%s\n' "$(sha256_file "$renderer")"
    printf 'reference.command=%s\n' "$(quote_command "${reference_command[@]}")"
    printf 'poryaaaa.command=%s\n' "$(quote_command "${poryaaaa_command[@]}")"
    wav_metadata reference "$reference_wav"
    wav_metadata poryaaaa "$poryaaaa_wav"
} >"$manifest"

printf 'Capture pair written to %s\n' "$output_dir"
