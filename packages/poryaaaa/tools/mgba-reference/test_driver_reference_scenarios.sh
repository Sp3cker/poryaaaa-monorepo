#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
package_dir="$(cd "$script_dir/../.." && pwd)"
recorder="${1:-$package_dir/build/mgba_mp2k_reference}"
wrapper="$script_dir/record_voice.sh"

if [[ ! -x "$recorder" ]]; then
    echo "SKIP: mgba_mp2k_reference is unavailable"
    exit 0
fi

temp_dir="$(mktemp -d "${TMPDIR:-/tmp}/poryaaaa-driver-reference.XXXXXX")"
trap 'rm -rf "$temp_dir"' EXIT

decomp="$temp_dir/decomp"
mkdir -p "$decomp"
touch "$decomp/pokeemerald-hearth.gba" "$decomp/pokeemerald-hearth.elf"

fake_nm="$temp_dir/fake-nm"
cat >"$fake_nm" <<'EOF'
#!/usr/bin/env bash
cat <<'SYMBOLS'
02000000 T gSoundInfo
02000004 T MPlayStart
02000008 T m4aMPlayAllStop
0200000C T gMPlayInfo_BGM
02010000 T voicegroup_chaos
SYMBOLS
EOF
chmod +x "$fake_nm"

fake_recorder="$temp_dir/fake-recorder"
cat >"$fake_recorder" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' "$@" >"${DRIVER_REFERENCE_ARGS:?}"
EOF
chmod +x "$fake_recorder"

expect_track() {
    local scenario="$1"
    local expected="$2"
    local actual
    actual="$("$recorder" --dump-driver-track "$scenario")"
    if [[ "$actual" != "$expected" ]]; then
        echo "FAIL: $scenario bytecode was $actual" >&2
        exit 1
    fi
}

expect_span() {
    local scenario="$1"
    local expected="$2"
    local actual
    actual="$("$recorder" --dump-driver-span "$scenario")"
    if [[ "$actual" != "$expected" ]]; then
        echo "FAIL: $scenario span was $actual" >&2
        exit 1
    fi
}

expect_family() {
    local type="$1"
    local expected="$2"
    local actual
    actual="$("$recorder" --dump-driver-family "$type")"
    if [[ "$actual" != "$expected" ]]; then
        echo "FAIL: type $type dispatched as $actual" >&2
        exit 1
    fi
}

expect_track start BC00BB3CBD00BE7FBF40CF3C7FB0B24DF00302
expect_track envelope BC00BB4BBD00BE7FBF40CF3C7F86B24DF00302
expect_track pitch BC00BB4BBD00BE7FBF40CF3C7F82C05082B250F00302
expect_track volume-pan BC00BB4BBD00BE7FBF40CF3C7F82BE20BF7F82B252F00302
expect_track retrigger BC00BB4BBD00BE7FBF40CF3C7F82CE3C81CF3C7F82B254F00302
expect_track release BC00BB4BBD00BE7FBF40CF3C7F82CE3C84B250F00302

expect_span start 562688
expect_span envelope 1967616
expect_span pitch 1405440
expect_span volume-pan 1405440
expect_span retrigger 1686528
expect_span release 1967616

expect_family 0x00 "directsound 48"
expect_family 0x01 "sq1 1"
expect_family 0x02 "sq2 2"
expect_family 0x03 "psw 4"
expect_family 0x0B "psw 4"

if "$recorder" --dump-driver-track unknown >/dev/null 2>&1; then
    echo "FAIL: unknown recorder scenario succeeded" >&2
    exit 1
fi
for type in 0x08 0x09 0x0A 0x10; do
    if "$recorder" --dump-driver-family "$type" >/dev/null 2>&1; then
        echo "FAIL: unsupported recorder type $type succeeded" >&2
        exit 1
    fi
done

option_value() {
    local wanted="$1"
    local path="$2"
    local previous=""
    local value
    while IFS= read -r value; do
        if [[ "$previous" == "$wanted" ]]; then
            printf '%s' "$value"
            return 0
        fi
        previous="$value"
    done <"$path"
    return 1
}

for scenario in start envelope pitch volume-pan retrigger release; do
    args_path="$temp_dir/$scenario.args"
    DRIVER_REFERENCE_ARGS="$args_path" "$wrapper" \
        --recorder "$fake_recorder" \
        --decomp "$decomp" \
        --nm "$fake_nm" \
        --voicegroup chaos \
        --voice 86 \
        --capture-stage native \
        --trace-output "$temp_dir/$scenario.trace" \
        --native-output-prefix "$temp_dir/$scenario-native" \
        --scenario "$scenario" >/dev/null

    if [[ "$(option_value --scenario "$args_path")" != "$scenario" ]] || \
        [[ "$(option_value --voicegroup-symbol "$args_path")" != "voicegroup_chaos" ]] || \
        [[ "$(option_value --voice-index "$args_path")" != "86" ]] || \
        [[ "$(option_value --elf "$args_path")" != "$decomp/pokeemerald-hearth.elf" ]]; then
        echo "FAIL: wrapper did not forward driver identity for $scenario" >&2
        exit 1
    fi
done

if "$wrapper" \
    --recorder "$fake_recorder" \
    --decomp "$decomp" \
    --nm "$fake_nm" \
    --voicegroup chaos \
    --voice 86 \
    --capture-stage native \
    --trace-output "$temp_dir/unknown.trace" \
    --native-output-prefix "$temp_dir/unknown-native" \
    --scenario unknown >/dev/null 2>&1; then
    echo "FAIL: unknown wrapper scenario succeeded" >&2
    exit 1
fi

if "$wrapper" --scenario >/dev/null 2>&1; then
    echo "FAIL: malformed wrapper scenario succeeded" >&2
    exit 1
fi
if "$wrapper" --psw-scenario start >/dev/null 2>&1; then
    echo "FAIL: retired PSW wrapper option succeeded" >&2
    exit 1
fi

echo "PASS: fixed driver reference controls, family dispatch, and rejection are exact"
