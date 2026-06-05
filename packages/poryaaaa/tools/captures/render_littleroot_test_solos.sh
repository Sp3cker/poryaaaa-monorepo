#!/usr/bin/env bash
set -euo pipefail

# Render poryaaaa v2 reference WAVs for:
#   mus_littleroot_test.mid: -E -R50 -G_littleroot_test -V099
#
# Mapping:
#   -G_littleroot_test -> renderer voicegroup argument "littleroot_test"
#   -R50               -> --reverb 50
#   -V099              -> --song-volume 99
#   -E                 -> no separate poryaaaa_render flag; the MIDI is the input
#
# Defaults target the local hearth-test checkout.  Override with env vars
# or positional args as needed:
#   PORYAAAA_RENDERER=build-v2/poryaaaa_render \
#   SAMPLE_RATE=44100 DURATION_SECONDS=30 \
#   tools/captures/render_littleroot_test_solos.sh /path/to/project

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
	echo "Usage: $0 [project_root] [midi_path] [out_dir]"
	echo
	echo "Env overrides:"
	echo "  PORYAAAA_RENDERER, PROJECT_ROOT, MIDI_PATH, OUT_DIR"
	echo "  VOICEGROUP, SAMPLE_RATE, DURATION_SECONDS, SONG_VOLUME, REVERB, POLYPHONY"
	echo "  CHANNELS='full psg directsound ch1 ch2 wave noise fifo-a fifo-b'"
	exit 0
fi

PROJECT_ROOT="${1:-${PROJECT_ROOT:-/Users/spencer/dev/hearth-test}}"
MIDI_PATH="${2:-${MIDI_PATH:-$PROJECT_ROOT/sound/songs/midi/mus_littleroot_test.mid}}"
OUT_DIR="${3:-${OUT_DIR:-tools/captures/poryaaaa-render-solos/littleroot-test}}"

RENDERER="${PORYAAAA_RENDERER:-build-v2/poryaaaa_render}"
VOICEGROUP="${VOICEGROUP:-littleroot_test}"
SAMPLE_RATE="${SAMPLE_RATE:-44100}"
DURATION_SECONDS="${DURATION_SECONDS:-30}"
SONG_VOLUME="${SONG_VOLUME:-99}"
REVERB="${REVERB:-50}"
POLYPHONY="${POLYPHONY:-5}"

CHANNELS=(${CHANNELS:-full psg directsound ch1 ch2 wave noise fifo-a fifo-b})

if [[ ! -x "$RENDERER" ]]; then
	echo "Renderer not executable: $RENDERER" >&2
	echo "Build full v2 first, for example:" >&2
	echo "  cmake -B build-v2 -DM4A_DRIVER_V2=ON -DHW_AUDIO_V2=ON" >&2
	echo "  cmake --build build-v2 --target poryaaaa_render" >&2
	exit 1
fi

if [[ ! -d "$PROJECT_ROOT" ]]; then
	echo "Project root not found: $PROJECT_ROOT" >&2
	exit 1
fi

if [[ ! -f "$MIDI_PATH" ]]; then
	echo "MIDI not found: $MIDI_PATH" >&2
	exit 1
fi

mkdir -p "$OUT_DIR"

echo "Renderer:     $RENDERER"
echo "Project root: $PROJECT_ROOT"
echo "Voicegroup:   $VOICEGROUP"
echo "MIDI:         $MIDI_PATH"
echo "Output dir:   $OUT_DIR"
echo "Sample rate:  $SAMPLE_RATE"
echo "Duration:     $DURATION_SECONDS s when loop markers are present"
echo "Channels:     ${CHANNELS[*]}"
echo

for channel in "${CHANNELS[@]}"; do
	out="$OUT_DIR/littleroot_test_${channel}.wav"
	echo "Rendering $channel -> $out"
	"$RENDERER" "$PROJECT_ROOT" "$VOICEGROUP" \
		--midi "$MIDI_PATH" \
		--output "$out" \
		--sample-rate "$SAMPLE_RATE" \
		--song-volume "$SONG_VOLUME" \
		--reverb "$REVERB" \
		--polyphony "$POLYPHONY" \
		--tail 0 \
		--fadeout 0 \
		--total-duration-seconds "$DURATION_SECONDS" \
		--solo "$channel"
done

echo
echo "Done. WAVs written to: $OUT_DIR"

# Sanity check: solo isolation + per-channel shape stats
#
# With the default SOUNDBIAS bias=0x200, hw_mix_render's bias_offset is 0,
# so the only nonlinearity in the mix-bus stage is the per-side ±1 clip.
# That means in unclipped regions:
#   psg          == ch1   + ch2 + wave + noise   (sample-exact)
#   directsound  == fifo-a + fifo-b              (sample-exact)
# In regions where the full mix saturates, individual solos add up to a
# pre-clip value and the full mix is clipped — divergence is expected
# there.  We report max-abs-diff + RMS and let the caller interpret.
#
# We also report per-channel shape statistics — both raw (min/max/DC/
# zero-crossings/RMS) and AC-normalized (mean-subtracted RMS) — so
# parity comparisons against mGBA can pick up DC-leak / level / shape
# divergences that AC-only metrics would hide.  poryaaaa's hw_psg
# currently synthesises dipolar PSG channels, while mGBA's GBA-mode
# `GBAudioSamplePSG` uses dcOffset=0 with unsigned channel samples,
# so the per-channel WAVs differ in DC level and absolute amplitude
# even when the AC content is correct.  See plan §12 "PSG unipolar
# parity rework" blocking gate.
if command -v python3 >/dev/null 2>&1; then
	echo
	echo "Sanity check: solo isolation + per-channel shape stats..."
	python3 - "$OUT_DIR" <<'PYEOF'
import sys, os, wave, array

out_dir = sys.argv[1]

def load(name):
	path = os.path.join(out_dir, f"littleroot_test_{name}.wav")
	if not os.path.exists(path):
		return None
	with wave.open(path, "rb") as w:
		# All renders are 16-bit signed stereo at SAMPLE_RATE.
		assert w.getsampwidth() == 2, f"expected 16-bit, got {w.getsampwidth()}"
		return array.array("h", w.readframes(w.getnframes()))

def isolation_report(label, target_name, source_names):
	target = load(target_name)
	if target is None:
		print(f"  {label}: skipped ({target_name}.wav missing — was it in $CHANNELS?)")
		return True
	sources = []
	for n in source_names:
		s = load(n)
		if s is None:
			print(f"  {label}: skipped ({n}.wav missing — was it in $CHANNELS?)")
			return True
		sources.append(s)
	if any(len(s) != len(target) for s in sources):
		print(f"  {label}: FAIL — length mismatch")
		return False

	max_diff = 0
	sumsq    = 0
	close    = 0
	clipped  = 0
	n        = len(target)
	for vals in zip(target, *sources):
		t = vals[0]
		s = 0
		for v in vals[1:]:
			s += v
		d = t - s
		ad = -d if d < 0 else d
		if ad > max_diff:
			max_diff = ad
		sumsq += d * d
		if ad <= 2:
			close += 1
		if t >= 32700 or t <= -32700:
			clipped += 1

	rms     = (sumsq / n) ** 0.5
	src_str = " + ".join(source_names)
	pct     = 100.0 * close / n
	print(f"  {label}: {target_name} vs {src_str}")
	print(f"    samples              = {n}")
	print(f"    max abs diff         = {max_diff:5d}   (full-scale ±32767)")
	print(f"    RMS abs diff         = {rms:8.2f}")
	print(f"    samples within ±2    = {close} ({pct:.2f}%)")
	print(f"    target near clip     = {clipped} (|sample| ≥ 32700)")
	return True

# Per-channel raw + AC-normalized shape stats.  Stereo interleaved:
# [L, R, L, R, ...].  Split per side so DC offsets per side are
# visible (mGBA showed DC L=0.019 R=0.0075 on the same song —
# different DC per side because PSG pan masks differ).
def shape_report(name):
	frames = load(name)
	if frames is None:
		return
	n = len(frames) // 2
	def stats(side_iter, n):
		# Single pass: track min, max, sum (for DC mean), sum_sq (raw RMS),
		# sum_sq_ac (AC RMS via Welford-ish).  Two passes is simpler:
		# first sum/min/max, second pass for mean-subtracted RMS.
		mn = 32767
		mx = -32768
		ssum = 0
		raw_sumsq = 0
		zc = 0
		prev_pos = None
		prev_neg = None
		samples = list(side_iter)
		for v in samples:
			if v < mn: mn = v
			if v > mx: mx = v
			ssum += v
			raw_sumsq += v * v
		mean = ssum / n
		ac_sumsq = 0.0
		# Zero-crossing count (sign flips ignoring zero-runs).
		last_sign = 0
		for v in samples:
			d = v - mean
			ac_sumsq += d * d
			cur_sign = 1 if v > 0 else (-1 if v < 0 else 0)
			if cur_sign and last_sign and cur_sign != last_sign:
				zc += 1
			if cur_sign:
				last_sign = cur_sign
		raw_rms = (raw_sumsq / n) ** 0.5
		ac_rms  = (ac_sumsq  / n) ** 0.5
		return mn, mx, mean, raw_rms, ac_rms, zc

	def left_iter():
		for i in range(0, len(frames), 2):
			yield frames[i]
	def right_iter():
		for i in range(1, len(frames), 2):
			yield frames[i]
	mnL, mxL, dcL, rrmsL, acrmsL, zcL = stats(left_iter(), n)
	mnR, mxR, dcR, rrmsR, acrmsR, zcR = stats(right_iter(), n)
	# Normalize DC to fraction of full-scale for easy mGBA comparison
	# (mGBA's expert reported DC offsets as fractions: L=0.019, R=0.0075).
	dcL_norm = dcL / 32767.0
	dcR_norm = dcR / 32767.0
	print(f"  {name:<12s} L: min={mnL:6d} max={mxL:6d} DC={dcL:8.1f} ({dcL_norm:+.5f}) raw_rms={rrmsL:7.1f} ac_rms={acrmsL:7.1f} zc={zcL}")
	print(f"  {' ':<12s} R: min={mnR:6d} max={mxR:6d} DC={dcR:8.1f} ({dcR_norm:+.5f}) raw_rms={rrmsR:7.1f} ac_rms={acrmsR:7.1f} zc={zcR}")

ok  = isolation_report("PSG group       ", "psg",         ["ch1", "ch2", "wave", "noise"])
ok2 = isolation_report("DirectSound grp ", "directsound", ["fifo-a", "fifo-b"])
print()
print("Per-channel shape (raw DAC-domain min/max/DC/RMS, AC-normalized RMS, zero crossings):")
for name in ("full", "psg", "directsound", "ch1", "ch2", "wave", "noise", "fifo-a", "fifo-b"):
	shape_report(name)
sys.exit(0 if (ok and ok2) else 1)
PYEOF
else
	echo "Skipping sanity check (python3 not found)."
fi
