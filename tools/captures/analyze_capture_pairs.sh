#!/usr/bin/env bash
set -euo pipefail

# Quick capture-pair analysis scaffold for mGBA vs poryaaaa WAVs.
#
# This first pass reports container format plus ffmpeg volumedetect
# mean/max levels for each matching channel.  It compensates for leading
# capture silence non-destructively at analysis time, then measures a
# fixed post-trim window.  It intentionally does not claim parity:
# sample rate and exact alignment still differ.  Use this to catch
# obvious level/routing/silence problems before spectral diffs.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

MGBA_DIR="${MGBA_DIR:-tools/captures/mgba-headless-channel-mute/reference-wavs/mgba-ss1-tap30}"
PORY_DIR="${PORY_DIR:-tools/captures/poryaaaa-render-solos/littleroot-test}"
OUT_DIR="${OUT_DIR:-tools/captures/analysis/littleroot-test}"

CHANNELS=(${CHANNELS:-full psg directsound ch1 ch2 wave noise fifo-a fifo-b})
SILENCE_THRESHOLD="${SILENCE_THRESHOLD:--50dB}"
SILENCE_MIN_DURATION="${SILENCE_MIN_DURATION:-0.01}"
COMPARE_SECONDS="${COMPARE_SECONDS:-30}"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
	echo "Usage: $0"
	echo
	echo "Env overrides:"
	echo "  MGBA_DIR=$MGBA_DIR"
	echo "  PORY_DIR=$PORY_DIR"
	echo "  OUT_DIR=$OUT_DIR"
	echo "  CHANNELS='${CHANNELS[*]}'"
	echo "  SILENCE_THRESHOLD=$SILENCE_THRESHOLD"
	echo "  SILENCE_MIN_DURATION=$SILENCE_MIN_DURATION"
	echo "  COMPARE_SECONDS=$COMPARE_SECONDS"
	exit 0
fi

mkdir -p "$OUT_DIR"
REPORT="$OUT_DIR/levels.tsv"

channel_to_mgba_file() {
	local ch="$1"
	case "$ch" in
		full)        echo "$MGBA_DIR/full.wav" ;;
		psg)         echo "$MGBA_DIR/psg.wav" ;;
		directsound) echo "$MGBA_DIR/directsound.wav" ;;
		ch1)         echo "$MGBA_DIR/ch1.wav" ;;
		ch2)         echo "$MGBA_DIR/ch2.wav" ;;
		wave)        echo "$MGBA_DIR/ch3.wav" ;;
		noise)       echo "$MGBA_DIR/ch4.wav" ;;
		fifo-a)      echo "$MGBA_DIR/fifo-a.wav" ;;
		fifo-b)      echo "$MGBA_DIR/fifo-b.wav" ;;
		*)           return 1 ;;
	esac
}

channel_to_pory_file() {
	local ch="$1"
	echo "$PORY_DIR/littleroot_test_${ch}.wav"
}

probe_field() {
	local file="$1"
	local field="$2"
	ffprobe -v error -show_entries "stream=$field" -of default=nw=1:nk=1 "$file"
}

volume_field() {
	local file="$1"
	local field="$2"
	local start="${3:-0}"
	ffmpeg -hide_banner -nostats -ss "$start" -t "$COMPARE_SECONDS" -i "$file" -af volumedetect -f null - 2>&1 \
		| awk -v key="${field}_volume:" '$0 ~ key { print $(NF-1); found=1 } END { if (!found) print "nan" }'
}

leading_silence_end() {
	local file="$1"
	local end
	end="$(ffmpeg -hide_banner -nostats -i "$file" \
		-af "silencedetect=noise=${SILENCE_THRESHOLD}:d=${SILENCE_MIN_DURATION}" \
		-f null - 2>&1 \
		| awk '/silence_end:/ { print $5; exit }')"
	if [[ -z "$end" ]]; then
		echo "0"
	else
		echo "$end"
	fi
}

write_row() {
	local channel="$1"
	local source="$2"
	local file="$3"

	if [[ ! -f "$file" ]]; then
		printf "%s\t%s\tMISSING\t\t\t\t\t%s\n" "$channel" "$source" "$file" >> "$REPORT"
		return
	fi

		local sr channels duration trim mean max
	sr="$(probe_field "$file" sample_rate)"
	channels="$(probe_field "$file" channels)"
	duration="$(probe_field "$file" duration)"
	trim="$(leading_silence_end "$file")"
	trim="$(awk -v t="$trim" -v d="$duration" 'BEGIN { if (t >= d - 0.001) print "0"; else print t }')"
	mean="$(volume_field "$file" mean "$trim")"
	max="$(volume_field "$file" max "$trim")"
	printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" \
		"$channel" "$source" "$sr" "$channels" "$duration" "$trim" "$mean" "$max" "$file" >> "$REPORT"
}

printf "channel\tsource\tsample_rate\tchannels\tduration_s\ttrim_start_s\tmean_db\tmax_db\tfile\n" > "$REPORT"

for channel in "${CHANNELS[@]}"; do
	write_row "$channel" "mgba" "$(channel_to_mgba_file "$channel")"
	write_row "$channel" "poryaaaa" "$(channel_to_pory_file "$channel")"
done

echo "Wrote $REPORT"
echo
column -t -s $'\t' "$REPORT"

echo
echo "poryaaaa - mGBA gain deltas:"
awk -F '\t' '
		NR == 1 { next }
		$2 == "mgba" {
			mean[$1] = $7
			max[$1] = $8
			next
		}
		$2 == "poryaaaa" {
			if (($1 in mean) && mean[$1] != "nan" && max[$1] != "nan" && $7 != "nan" && $8 != "nan") {
				printf "  %-12s mean %+5.1f dB   max %+5.1f dB\n",
				       $1, ($7 - mean[$1]), ($8 - max[$1])
			}
		}
	' "$REPORT"

# Sample-domain analysis: raw min/max/DC/RMS/zero-crossings + AC-normalized
# RMS, on the post-silence-trim window of length COMPARE_SECONDS.
#
# Why this exists: ffmpeg volumedetect reports mean/max in dB, which
# AC-couples internally and hides DC offsets.  Per the PSG-unipolar
# parity blocker, mGBA's GBA-mode PSG channels are unsigned (dcOffset=0)
# and DC leaks through to the full mix; poryaaaa's PSG synth is dipolar.
# The visible signature is per-channel + full-mix DC offset mismatch
# that ffmpeg metrics don't catch.  This block reports raw DC explicitly
# so the divergence is visible, plus AC-normalized RMS so phase/harmonic
# parity is comparable independently of DC.
#
# Different sample rates between mGBA (65536 Hz) and poryaaaa (configurable)
# are handled by reporting rate-normalized zero-crossings (per second).
# Other stats (min/max/DC/RMS) are sample-domain and rate-independent.
if command -v python3 >/dev/null 2>&1; then
	echo
	echo "Sample-domain stats (raw min/max/DC/RMS + AC-normalized RMS + zero-crossings/sec, on post-trim window):"
	echo
	python3 - "$REPORT" "$COMPARE_SECONDS" <<'PYEOF'
import sys, os, wave, array, math

report_path     = sys.argv[1]
compare_seconds = float(sys.argv[2])

# Parse the TSV the bash side just wrote.
rows = []
with open(report_path) as f:
	header = f.readline().rstrip("\n").split("\t")
	for line in f:
		parts = line.rstrip("\n").split("\t")
		if len(parts) != len(header):
			continue
		rows.append(dict(zip(header, parts)))

# Group by (channel, source) for easy lookup.
def get(ch, src):
	for r in rows:
		if r["channel"] == ch and r["source"] == src:
			return r
	return None

def load_window(path, trim_s, dur_s):
	if not path or not os.path.exists(path):
		return None, None, None
	with wave.open(path, "rb") as w:
		sr = w.getframerate()
		nch = w.getnchannels()
		sw = w.getsampwidth()
		if sw != 2:
			# Bail on non-16-bit; mGBA + poryaaaa both write s16.
			return None, None, None
		start = int(trim_s * sr)
		end   = int((trim_s + dur_s) * sr)
		w.setpos(min(start, w.getnframes()))
		nframes_read = max(0, min(end, w.getnframes()) - start)
		raw = w.readframes(nframes_read)
		samples = array.array("h", raw)
		# Stereo deinterleave.
		if nch == 2:
			L = samples[0::2]
			R = samples[1::2]
		else:
			L = samples
			R = samples
		return L, R, sr

def stats(side, sr):
	n = len(side)
	if n == 0:
		return None
	mn = 32767; mx = -32768
	ssum = 0; raw_ss = 0
	for v in side:
		if v < mn: mn = v
		if v > mx: mx = v
		ssum  += v
		raw_ss += v * v
	mean = ssum / n
	ac_ss = 0.0; zc = 0; last_sign = 0
	for v in side:
		d = v - mean
		ac_ss += d * d
		cs = 1 if v > 0 else (-1 if v < 0 else 0)
		if cs and last_sign and cs != last_sign:
			zc += 1
		if cs:
			last_sign = cs
	return {
		"min":     mn,
		"max":     mx,
		"dc":      mean,
		"dc_norm": mean / 32767.0,
		"raw_rms": math.sqrt(raw_ss / n),
		"ac_rms":  math.sqrt(ac_ss / n),
		"zc_per_s": zc * sr / n,
	}

CHANNELS = []
for r in rows:
	if r["channel"] not in CHANNELS:
		CHANNELS.append(r["channel"])

header_fmt = "  {:<12s} {:<3s} {:<8s} | {:>7s}  {:>7s}  {:>9s}  ({:>8s})  {:>8s}  {:>8s}  {:>7s}"
print(header_fmt.format(
	"channel", "sd", "source",
	"min", "max", "DC", "DCnorm", "raw_rms", "ac_rms", "zc/s"
))
print("  " + "-" * 100)

def fmt_row(ch, side, src, st):
	if st is None:
		return f"  {ch:<12s} {side:<3s} {src:<8s} | (missing)"
	return f"  {ch:<12s} {side:<3s} {src:<8s} | {st['min']:7d}  {st['max']:7d}  {st['dc']:9.1f} ({st['dc_norm']:+8.5f})  {st['raw_rms']:8.1f}  {st['ac_rms']:8.1f}  {st['zc_per_s']:7.1f}"

for ch in CHANNELS:
	mr = get(ch, "mgba")
	pr = get(ch, "poryaaaa")
	if mr is None or pr is None:
		continue
	# Trim is in seconds; load_window applies it.
	mL, mR, msr = load_window(mr.get("file"), float(mr.get("trim_start_s") or 0),
	                          compare_seconds)
	pL, pR, psr = load_window(pr.get("file"), float(pr.get("trim_start_s") or 0),
	                          compare_seconds)
	mL_s = stats(mL, msr) if mL is not None and msr else None
	mR_s = stats(mR, msr) if mR is not None and msr else None
	pL_s = stats(pL, psr) if pL is not None and psr else None
	pR_s = stats(pR, psr) if pR is not None and psr else None
	for side, m_s, p_s in (("L", mL_s, pL_s), ("R", mR_s, pR_s)):
		print(fmt_row(ch, side, "mgba", m_s))
		print(fmt_row(ch, side, "pory", p_s))
		if m_s and p_s:
			# Delta row: poryaaaa - mGBA on the metrics that matter most.
			d_dc      = p_s["dc"] - m_s["dc"]
			d_dc_norm = p_s["dc_norm"] - m_s["dc_norm"]
			d_raw     = p_s["raw_rms"] - m_s["raw_rms"]
			d_ac      = p_s["ac_rms"]  - m_s["ac_rms"]
			d_zc      = p_s["zc_per_s"] - m_s["zc_per_s"]
			# Highlight DC mismatch >= 0.5% of full scale: that's the
			# PSG-unipolar parity signature.
			flag = " <-- DC drift" if abs(d_dc_norm) >= 0.005 else ""
			print(f"  {'':12s} {side:<3s} Δ        | {'':7s}  {'':7s}  {d_dc:9.1f} ({d_dc_norm:+8.5f})  {d_raw:+8.1f}  {d_ac:+8.1f}  {d_zc:+7.1f}{flag}")
	print()
PYEOF
else
	echo
	echo "Skipping sample-domain stats (python3 not found)."
fi
