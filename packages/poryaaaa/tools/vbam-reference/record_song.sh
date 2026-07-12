#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
package_dir="$(cd "$script_dir/../.." && pwd)"
builder="$package_dir/tools/mgba-reference/build_song_rom.sh"
recorder="$package_dir/build/vbam_mp2k_reference"
decomp=""
song=""
core=""
output=""
manifest=""
forwarded=()

# Prints the audio-only VBA-M capture interface.
usage() {
    cat >&2 <<'EOF'
Usage: record_song.sh --decomp DIR --song NAME --core FILE --output FILE [options]

Options:
  --recorder FILE          vbam_mp2k_reference executable
  --manifest FILE          Capture manifest (default: OUTPUT.manifest.txt)
  --duration-seconds S     Forwarded to the recorder
  --solo LIST              Forwarded to the recorder
  --mute LIST              Forwarded to the recorder
  --interpolation VALUE    enabled or disabled
  --filtering N            0 through 10
EOF
}

while (($#)); do
    case "$1" in
        --decomp) decomp="$2"; shift 2 ;;
        --song) song="$2"; shift 2 ;;
        --core) core="$2"; shift 2 ;;
        --output) output="$2"; shift 2 ;;
        --manifest) manifest="$2"; shift 2 ;;
        --recorder) recorder="$2"; shift 2 ;;
        --duration-seconds|--solo|--mute|--interpolation|--filtering)
            forwarded+=("$1" "$2"); shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

if [[ -z "$decomp" || -z "$song" || -z "$core" || -z "$output" ]]; then
    usage
    exit 2
fi
for required in "$builder" "$recorder" "$core"; do
    if [[ ! -f "$required" ]]; then
        echo "Missing required file: $required" >&2
        exit 1
    fi
done
manifest="${manifest:-$output.manifest.txt}"
fixture_dir="$(mktemp -d "${TMPDIR:-/tmp}/poryaaaa-vbam-capture.XXXXXX")"
trap 'rm -rf "$fixture_dir"' EXIT
fixture_rom="$fixture_dir/$song-audio-only.gba"

"$builder" --decomp "$decomp" --song "$song" --output "$fixture_rom"
"$recorder" --core "$core" --rom "$fixture_rom" --output "$output" \
    --manifest "$manifest" "${forwarded[@]}"
