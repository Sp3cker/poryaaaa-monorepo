#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
decomp=""
song=""
output=""
rom=""
elf=""
nm_tool="${ARM_NONE_EABI_NM:-arm-none-eabi-nm}"
readelf_tool="${ARM_NONE_EABI_READELF:-arm-none-eabi-readelf}"
as_tool="${ARM_NONE_EABI_AS:-arm-none-eabi-as}"
ld_tool="${ARM_NONE_EABI_LD:-arm-none-eabi-ld}"
objcopy_tool="${ARM_NONE_EABI_OBJCOPY:-arm-none-eabi-objcopy}"

# Prints the required configured-song ROM arguments.
usage() {
    cat >&2 <<'EOF'
Usage: build_song_rom.sh --decomp DIR --song NAME --output FILE [options]

Options:
  --rom FILE    Source GBA ROM (default: DECOMP/pokeemerald-hearth.gba)
  --elf FILE    Matching ELF (default: DECOMP/pokeemerald-hearth.elf)
EOF
}

while (($#)); do
    case "$1" in
        --decomp) decomp="$2"; shift 2 ;;
        --song) song="$2"; shift 2 ;;
        --output) output="$2"; shift 2 ;;
        --rom) rom="$2"; shift 2 ;;
        --elf) elf="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

if [[ -z "$decomp" || -z "$song" || -z "$output" ]]; then
    usage
    exit 2
fi

rom="${rom:-$decomp/pokeemerald-hearth.gba}"
elf="${elf:-$decomp/pokeemerald-hearth.elf}"
song_table="$decomp/sound/song_table.inc"
for required in "$rom" "$elf" "$song_table" "$script_dir/audio_fixture_boot.s"; do
    if [[ ! -f "$required" ]]; then
        echo "Missing required file: $required" >&2
        exit 1
    fi
done
if [[ "$output" == "$rom" ]]; then
    echo "Output must not overwrite the source ROM: $rom" >&2
    exit 1
fi
for tool in "$nm_tool" "$readelf_tool" "$as_tool" "$ld_tool" "$objcopy_tool"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing ARM tool: $tool" >&2
        exit 1
    fi
done

song_id="$(awk -v target="$song" '
    $1 == "song" {
        name = $2
        sub(/,$/, "", name)
        if (name == target) {
            print count
            exit
        }
        count++
    }
' "$song_table")"
if [[ -z "$song_id" ]]; then
    echo "Song not found in $song_table: $song" >&2
    exit 1
fi

temp_dir="$(mktemp -d "${TMPDIR:-/tmp}/poryaaaa-song-rom.XXXXXX")"
trap 'rm -rf "$temp_dir"' EXIT
"$nm_tool" -n "$elf" > "$temp_dir/symbols"

# Resolves one address from the matching decomp ELF.
lookup_symbol() {
    local name="$1"
    local address
    address="$(awk -v symbol="$name" '
        $NF == symbol && address == "" { address = "0x" $1 }
        END { print address }
    ' "$temp_dir/symbols")"
    if [[ -z "$address" ]]; then
        echo "ELF symbol not found: $name" >&2
        exit 1
    fi
    printf '%s' "$address"
}

ag_main="$(lookup_symbol AgbMain)"
ag_main_size="$($readelf_tool -sW "$elf" | awk '
    $NF == "AgbMain" && size == "" { size = $3 }
    END { print size }
')"
if [[ -z "$ag_main_size" ]]; then
    echo "Could not determine AgbMain size from $elf" >&2
    exit 1
fi

# Marks a resolved function pointer for Thumb interworking calls.
thumb_address() {
    printf '0x%X' "$(($(lookup_symbol "$1") | 1))"
}

"$as_tool" -mcpu=arm7tdmi "$script_dir/audio_fixture_boot.s" -o "$temp_dir/boot.o"
"$ld_tool" -Ttext="$ag_main" -e fixture_boot \
    --defsym="FIXTURE_SONG_ID=$song_id" \
    --defsym="M4A_SOUND_INIT=$(thumb_address m4aSoundInit)" \
    --defsym="M4A_SONG_NUM_START=$(thumb_address m4aSongNumStart)" \
    --defsym="M4A_SOUND_VSYNC=$(thumb_address m4aSoundVSync)" \
    --defsym="M4A_SOUND_MAIN=$(thumb_address m4aSoundMain)" \
    "$temp_dir/boot.o" -o "$temp_dir/boot.elf"
"$objcopy_tool" -O binary --only-section=.text "$temp_dir/boot.elf" "$temp_dir/boot.bin"

boot_size="$(wc -c < "$temp_dir/boot.bin" | tr -d ' ')"
if ((boot_size > ag_main_size)); then
    echo "Audio bootstrap is $boot_size bytes but AgbMain is only $ag_main_size bytes" >&2
    exit 1
fi

cp "$rom" "$output"
dd if="$temp_dir/boot.bin" of="$output" bs=1 seek="$((ag_main - 0x08000000))" conv=notrunc status=none

echo "Built $output: $song (song $song_id), $boot_size-byte audio-only AgbMain"
