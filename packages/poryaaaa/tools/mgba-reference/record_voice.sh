#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
package_dir="$(cd "$script_dir/../.." && pwd)"
recorder="$package_dir/build/mgba_mp2k_reference"
decomp=""
rom=""
elf=""
voicegroup=""
voice_index=""
song=""
output=""
nm_tool="${ARM_NONE_EABI_NM:-arm-none-eabi-nm}"
forwarded=()

usage() {
    cat >&2 <<'EOF'
Usage: record_voice.sh --decomp DIR --voicegroup NAME --voice INDEX --output FILE [options]
   or: record_voice.sh --decomp DIR --song NAME --output FILE [options]

Options:
  --recorder FILE          mgba_mp2k_reference executable
  --rom FILE               Built GBA ROM (default: DECOMP/pokeemerald-hearth.gba)
  --elf FILE               Matching ELF (default: DECOMP/pokeemerald-hearth.elf)
  --nm FILE                ARM nm executable (default: arm-none-eabi-nm)
  --duration-seconds S     Forwarded to the recorder
  --boot-timeout-seconds S Forwarded to the recorder
  --note N                 Forwarded to the recorder
  --velocity N             Forwarded to the recorder
  --volume N               Forwarded to the recorder
  --pan N                  Forwarded to the recorder
  --fixture-address ADDR   Forwarded to the recorder
EOF
}

while (($#)); do
    case "$1" in
        --decomp) decomp="$2"; shift 2 ;;
        --recorder) recorder="$2"; shift 2 ;;
        --rom) rom="$2"; shift 2 ;;
        --elf) elf="$2"; shift 2 ;;
        --voicegroup) voicegroup="$2"; shift 2 ;;
        --voice) voice_index="$2"; shift 2 ;;
        --song) song="$2"; shift 2 ;;
        --output) output="$2"; shift 2 ;;
        --nm) nm_tool="$2"; shift 2 ;;
        --duration-seconds|--boot-timeout-seconds|--note|--velocity|--volume|--pan|--fixture-address|--solo|--mute)
            forwarded+=("$1" "$2")
            shift 2
            ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

if [[ -z "$decomp" || -z "$output" ]]; then
    usage
    exit 2
fi
if [[ -n "$song" && ( -n "$voicegroup" || -n "$voice_index" ) ]]; then
    echo "Choose either --song or --voicegroup/--voice" >&2
    exit 2
fi
if [[ -z "$song" && ( -z "$voicegroup" || -z "$voice_index" ) ]]; then
    usage
    exit 2
fi
if [[ -z "$song" ]]; then
    if [[ "$voicegroup" != voicegroup_* ]]; then
        voicegroup="voicegroup_$voicegroup"
    fi
    if [[ ! "$voice_index" =~ ^[0-9]+$ ]]; then
        echo "Voice index must be a non-negative integer: $voice_index" >&2
        exit 2
    fi
fi

rom="${rom:-$decomp/pokeemerald-hearth.gba}"
elf="${elf:-$decomp/pokeemerald-hearth.elf}"
for required in "$recorder" "$rom" "$elf"; do
    if [[ ! -f "$required" ]]; then
        echo "Missing required file: $required" >&2
        exit 1
    fi
done
if [[ ! -x "$recorder" ]]; then
    echo "Recorder is not executable: $recorder" >&2
    exit 1
fi
if ! command -v "$nm_tool" >/dev/null 2>&1; then
    echo "Missing ARM nm tool: $nm_tool" >&2
    exit 1
fi

symbols="$($nm_tool -n "$elf")"
lookup_symbol() {
    local name="$1"
    local address
    address="$(awk -v symbol="$name" '$NF == symbol { print "0x" $1; exit }' <<<"$symbols")"
    if [[ -z "$address" ]]; then
        echo "ELF symbol not found: $name" >&2
        exit 1
    fi
    printf '%s' "$address"
}

sound_info="$(lookup_symbol gSoundInfo)"

if [[ -n "$song" ]]; then
    song_table="$decomp/sound/song_table.inc"
    if [[ ! -f "$song_table" ]]; then
        echo "Missing song table: $song_table" >&2
        exit 1
    fi
    song_metadata="$(awk -v target="$song" '
        $1 == "song" {
            name = $2
            player = $3
            sub(/,$/, "", name)
            sub(/,$/, "", player)
            if (name == target) {
                print count, player
                exit
            }
            count++
        }
    ' "$song_table")"
    if [[ -z "$song_metadata" ]]; then
        echo "Song not found in $song_table: $song" >&2
        exit 1
    fi
    read -r song_id music_player <<<"$song_metadata"
    player_suffix="${music_player#MUSIC_PLAYER_}"
    song_address="$(lookup_symbol "$song")"
    mplay_info="$(lookup_symbol "gMPlayInfo_$player_suffix")"
    fixture_dir="$(mktemp -d "${TMPDIR:-/tmp}/poryaaaa-song-capture.XXXXXX")"
    trap 'rm -rf "$fixture_dir"' EXIT
    fixture_rom="$fixture_dir/fixture.gba"

    "$script_dir/build_song_rom.sh" \
        --decomp "$decomp" \
        --song "$song" \
        --rom "$rom" \
        --elf "$elf" \
        --output "$fixture_rom"

    echo "Recording $song (song $song_id, $music_player) through audio-only ROM + mGBA" >&2
    "$recorder" \
        --rom "$fixture_rom" \
        --output "$output" \
        --boot-song \
        --song-address "$song_address" \
        --mplay-info "$mplay_info" \
        --sound-info "$sound_info" \
        "${forwarded[@]}"
    exit
fi

mplay_start="$(lookup_symbol MPlayStart)"
mplay_info="$(lookup_symbol gMPlayInfo_BGM)"
voicegroup_address="$(lookup_symbol "$voicegroup")"
voice_address="$(printf '0x%08X' "$((voicegroup_address + voice_index * 12))")"

echo "Recording $voicegroup[$voice_index] at $voice_address through ROM MP2K + mGBA" >&2
exec "$recorder" \
    --rom "$rom" \
    --output "$output" \
    --mplay-start "$mplay_start" \
    --mplay-info "$mplay_info" \
    --sound-info "$sound_info" \
    --voice-address "$voice_address" \
    "${forwarded[@]}"
