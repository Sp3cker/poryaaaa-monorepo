#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
capture="$script_dir/capture_directsound_fixture.sh"
recorder="$script_dir/mgba_mp2k_reference.c"
wrapper="$script_dir/record_voice.sh"

# The comparison is invalid unless both renderers expose all 12 MP2K PCM slots.
grep -Eq '^[[:space:]]+--require-max-chans 12 \\$' "$capture"
grep -Eq '^[[:space:]]+--polyphony 12 \\$' "$capture"
grep -Fq "printf 'pcm_voice_limit_expected=12\\n'" "$capture"
grep -Fq "printf 'mgba_max_chans_observed=12\\n'" "$capture"
grep -Fq "printf 'poryaaaa_polyphony_requested=12\\n'" "$capture"
grep -Fq -- '--require-max-chans|--solo|--mute)' "$wrapper"
grep -Fq -- 'strcmp(name, "--require-max-chans")' "$recorder"
grep -Fq -- 'MP2K maxChans mismatch' "$recorder"

echo "PASS: DirectSound fixture requires and records 12 PCM voices on both engines"
