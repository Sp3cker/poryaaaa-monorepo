#!/usr/bin/env python3
"""
Align two WAV files via cross-correlation, RMS-normalize, and emit per-band
spectral metrics. Intended for diffing poryaaaa renders against mGBA captures
of the same MIDI.

Usage:
    align_and_metrics.py <poryaaaa.wav> <mgba.wav> <out_dir>

Outputs to <out_dir>:
    aligned_poryaaaa.wav  trimmed/aligned/normalized
    aligned_mgba.wav      resampled/trimmed/aligned/normalized
    diff.wav              sample-wise (poryaaaa - mgba)
    report.txt            offset, RMS levels, per-band dB deltas
"""

import sys
from math import gcd
from pathlib import Path

import numpy as np
from scipy.io import wavfile
from scipy.signal import correlate, correlation_lags, resample_poly

TARGET_RATE = 48000
RMS_TARGET = 0.1

BAND_EDGES_HZ = [
    20, 60, 120, 250, 500, 1000, 2000, 4000, 8000, 12000, 16000, 22000
]


def load_wav(path):
    rate, data = wavfile.read(path)
    if data.dtype == np.int16:
        data = data.astype(np.float32) / 32768.0
    elif data.dtype == np.int32:
        data = data.astype(np.float32) / 2147483648.0
    elif data.dtype == np.uint8:
        data = (data.astype(np.float32) - 128.0) / 128.0
    else:
        data = data.astype(np.float32)
    if data.ndim == 1:
        data = np.column_stack([data, data])
    elif data.shape[1] > 2:
        data = data[:, :2]
    return rate, data


def resample(data, rate_in, rate_out):
    if rate_in == rate_out:
        return data
    g = gcd(rate_in, rate_out)
    return resample_poly(data, rate_out // g, rate_in // g, axis=0).astype(np.float32)


def find_lag(a_mono, b_mono, rate, template_sec=10.0):
    """Locate the offset in `b` where `a` begins.

    Uses the first `template_sec` of `a` as a unique template and slides it
    across `b` to find the peak. This is more robust than full-signal
    cross-correlation when either signal contains repeating loops, which
    produce multiple near-equal correlation peaks.

    Returns (lag, peak_strength, runner_up_ratio):
        lag                 - offset in samples (a[0:] aligns with b[lag:])
        peak_strength       - normalized peak (1.0 = perfect template match)
        runner_up_ratio     - second-best peak / best peak (lower is more
                              confident; >0.9 means alignment is ambiguous)
    """
    template_n = min(int(template_sec * rate), len(a_mono))
    template = a_mono[:template_n]
    template = template - template.mean()
    template_norm = float(np.sqrt(np.sum(template ** 2)))
    if template_norm < 1e-9:
        return 0, 0.0, 1.0

    haystack = b_mono - b_mono.mean()
    corr = correlate(haystack, template, mode='valid', method='fft')

    # Normalize each correlation point by the local L2 norm of haystack so
    # different-amplitude regions of b don't bias the peak.
    cumsq = np.concatenate(([0.0], np.cumsum(haystack ** 2)))
    seg_norm = np.sqrt(np.maximum(cumsq[template_n:] - cumsq[:-template_n], 0.0))
    seg_norm = np.maximum(seg_norm, 1e-9)
    norm_corr = corr / (seg_norm * template_norm)

    best_idx = int(np.argmax(norm_corr))
    peak = float(norm_corr[best_idx])

    # Runner-up: best peak outside ±0.5 s of the winner
    guard = int(0.5 * rate)
    masked = norm_corr.copy()
    lo = max(0, best_idx - guard)
    hi = min(len(masked), best_idx + guard + 1)
    masked[lo:hi] = -np.inf
    runner_up = float(masked.max()) if np.isfinite(masked.max()) else 0.0
    runner_up_ratio = runner_up / peak if peak > 1e-9 else 1.0

    # Sign convention: a[0:] aligns with b[best_idx:]. Convert to the trim
    # convention used by trim_to_common: positive lag trims head of a,
    # negative trims head of b. Here a leads if best_idx == 0; b leads if
    # best_idx > 0. So lag = -best_idx (b has best_idx samples of preroll).
    return -best_idx, peak, runner_up_ratio


def trim_to_common(a, b, lag):
    if lag > 0:
        a = a[lag:]
    elif lag < 0:
        b = b[-lag:]
    n = min(len(a), len(b))
    return a[:n], b[:n]


def rms(x):
    return float(np.sqrt(np.mean(x.astype(np.float64) ** 2)))


def rms_normalize(x, target):
    r = rms(x)
    if r < 1e-9:
        return x
    return (x * (target / r)).astype(np.float32)


def band_rms_db(stereo, rate, edges):
    """Welch-style band power on mono mix; returns list of (lo, hi, dB)."""
    mono = stereo.mean(axis=1).astype(np.float64)
    n = len(mono)
    if n < 2:
        return [(lo, hi, float('-inf')) for lo, hi in zip(edges[:-1], edges[1:])]
    win = np.hanning(n)
    spec = np.fft.rfft(mono * win)
    freqs = np.fft.rfftfreq(n, 1.0 / rate)
    power = (np.abs(spec) ** 2) / (np.sum(win ** 2) * rate)
    out = []
    for lo, hi in zip(edges[:-1], edges[1:]):
        mask = (freqs >= lo) & (freqs < hi)
        if not mask.any():
            out.append((lo, hi, float('-inf')))
            continue
        band_power = np.sum(power[mask]) * (rate / n)
        out.append((lo, hi, 10.0 * np.log10(band_power + 1e-20)))
    return out


def main():
    if len(sys.argv) != 4:
        print(__doc__, file=sys.stderr)
        sys.exit(2)
    poryaaaa_path, mgba_path, out_dir = sys.argv[1:]
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)

    print(f"Loading poryaaaa: {poryaaaa_path}")
    p_rate, p = load_wav(poryaaaa_path)
    print(f"  rate={p_rate} samples={len(p)} channels={p.shape[1]}")

    print(f"Loading mGBA:     {mgba_path}")
    m_rate, m = load_wav(mgba_path)
    print(f"  rate={m_rate} samples={len(m)} channels={m.shape[1]}")

    if p_rate != TARGET_RATE:
        print(f"Resampling poryaaaa {p_rate} -> {TARGET_RATE}")
        p = resample(p, p_rate, TARGET_RATE)
    if m_rate != TARGET_RATE:
        print(f"Resampling mGBA     {m_rate} -> {TARGET_RATE}")
        m = resample(m, m_rate, TARGET_RATE)

    print("Cross-correlating for time alignment (mono template, 10 s)...")
    lag, peak, runner_up = find_lag(p.mean(axis=1), m.mean(axis=1), TARGET_RATE)
    offset_ms = 1000.0 * lag / TARGET_RATE
    leader = "poryaaaa leads" if lag > 0 else "mGBA leads" if lag < 0 else "exact"
    print(f"  lag={lag} samples ({offset_ms:+.1f} ms; {leader})")
    if peak < 0.25:
        verdict = "NO MATCH (likely tempo drift or different content)"
    elif runner_up >= 0.95:
        verdict = "AMBIGUOUS (multiple candidate offsets)"
    else:
        verdict = "confident"
    print(f"  peak={peak:.4f}  runner-up/peak={runner_up:.3f}  ({verdict})")

    p_a, m_a = trim_to_common(p, m, lag)
    duration = len(p_a) / TARGET_RATE
    print(f"  aligned length: {len(p_a)} samples ({duration:.2f} s)")

    p_rms_raw = rms(p_a)
    m_rms_raw = rms(m_a)
    p_n = rms_normalize(p_a, RMS_TARGET)
    m_n = rms_normalize(m_a, RMS_TARGET)

    diff = (p_n - m_n).astype(np.float32)
    diff_rms = rms(diff)

    aligned_p = out / 'aligned_poryaaaa.wav'
    aligned_m = out / 'aligned_mgba.wav'
    diff_p = out / 'diff.wav'
    wavfile.write(str(aligned_p), TARGET_RATE, np.clip(p_n, -1.0, 1.0).astype(np.float32))
    wavfile.write(str(aligned_m), TARGET_RATE, np.clip(m_n, -1.0, 1.0).astype(np.float32))
    wavfile.write(str(diff_p),    TARGET_RATE, np.clip(diff, -1.0, 1.0).astype(np.float32))

    p_bands = band_rms_db(p_n, TARGET_RATE, BAND_EDGES_HZ)
    m_bands = band_rms_db(m_n, TARGET_RATE, BAND_EDGES_HZ)

    lines = []

    def w(s=''):
        lines.append(s)
        print(s)

    w('=== poryaaaa vs mGBA spectral diff ===')
    w(f'poryaaaa: {poryaaaa_path}')
    w(f'mGBA:     {mgba_path}')
    w('')
    ratio_db = 20 * np.log10(p_rms_raw / max(m_rms_raw, 1e-12))
    w(f'Alignment lag:    {lag} samples ({offset_ms:+.1f} ms; {leader})')
    w(f'Alignment peak:   {peak:.4f}  runner-up/peak={runner_up:.3f}  ({verdict})')
    w(f'Aligned length:   {duration:.3f} s')
    w(f'Raw RMS:          poryaaaa={p_rms_raw:.4f}  mGBA={m_rms_raw:.4f}  '
      f'(p/m {ratio_db:+.2f} dB)')
    w(f'Diff RMS (post-normalize): {diff_rms:.4f} '
      f'({20*np.log10(diff_rms+1e-12):+.2f} dBFS)')
    w('')
    w(f'Per-band RMS (signals normalized to {RMS_TARGET:.2f} RMS):')
    w(f'{"Band Hz":>14}  {"poryaaaa dB":>12}  {"mGBA dB":>12}  {"Δ (p-m) dB":>12}')
    for (lo, hi, pdb), (_, _, mdb) in zip(p_bands, m_bands):
        delta = pdb - mdb
        w(f'  {lo:>5.0f}-{hi:<5.0f}  {pdb:>12.2f}  {mdb:>12.2f}  {delta:>+12.2f}')

    (out / 'report.txt').write_text('\n'.join(lines) + '\n')
    print(f'\nWrote {out}/{aligned_p.name}, {aligned_m.name}, {diff_p.name}, report.txt')


if __name__ == '__main__':
    main()
