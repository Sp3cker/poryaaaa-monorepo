#!/usr/bin/env python3
"""Compare aligned PCM16 WAV windows without hiding polarity or level."""

import argparse
import json
import math
import struct
import sys
import wave
from pathlib import Path


class ComparisonError(Exception):
    """Represent an input or analysis failure suitable for a CLI diagnostic."""


def parse_args():
    """Describe the narrow waveform comparison interface used by automation."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path, help="reference PCM16 WAV")
    parser.add_argument("candidate", type=Path, help="candidate PCM16 WAV")
    parser.add_argument(
        "--reference-start",
        type=int,
        default=0,
        metavar="SAMPLES",
        help="first reference sample in the comparison window (default: 0)",
    )
    parser.add_argument(
        "--reference-length",
        type=int,
        metavar="SAMPLES",
        help="comparison window length (default: longest window valid at +max-lag)",
    )
    parser.add_argument(
        "--candidate-start",
        type=int,
        metavar="SAMPLES",
        help="candidate window anchor (default: same sample as --reference-start)",
    )
    parser.add_argument(
        "--max-lag",
        type=int,
        default=0,
        metavar="SAMPLES",
        help="search candidate offsets in [-SAMPLES, +SAMPLES] (default: 0)",
    )
    parser.add_argument(
        "--min-correlation",
        type=float,
        metavar="VALUE",
        help="fail when signed normalized correlation is below VALUE",
    )
    parser.add_argument(
        "--max-residual-percent",
        type=float,
        metavar="PERCENT",
        help="fail when gain-fitted residual exceeds PERCENT",
    )
    return parser.parse_args()


def read_pcm16_mono(path):
    """Read PCM16 WAV frames and fold stereo to the arithmetic mean."""
    try:
        with wave.open(str(path), "rb") as wav:
            channels = wav.getnchannels()
            sample_rate = wav.getframerate()
            sample_width = wav.getsampwidth()
            compression = wav.getcomptype()
            frame_count = wav.getnframes()
            frames = wav.readframes(frame_count)
    except (OSError, EOFError, wave.Error) as error:
        raise ComparisonError(f"cannot read {path}: {error}") from error

    if channels not in (1, 2):
        raise ComparisonError(f"{path}: expected mono or stereo WAV, got {channels} channels")
    if sample_width != 2 or compression != "NONE":
        raise ComparisonError(f"{path}: expected uncompressed PCM16 WAV")

    sample_count = frame_count * channels
    expected_bytes = sample_count * sample_width
    if len(frames) != expected_bytes:
        raise ComparisonError(f"{path}: truncated PCM data")

    samples = struct.unpack(f"<{sample_count}h", frames)
    if channels == 1:
        mono = [float(sample) for sample in samples]
    else:
        mono = [
            (samples[index] + samples[index + 1]) / 2.0
            for index in range(0, sample_count, 2)
        ]

    return {
        "channels": channels,
        "sample_rate": sample_rate,
        "frame_count": frame_count,
        "mono": mono,
    }


def centered(samples):
    """Remove only the selected window's mean for shape comparison."""
    mean = sum(samples) / len(samples)
    return [sample - mean for sample in samples], mean


def rms(samples):
    """Measure unnormalized PCM amplitude for level and residual reporting."""
    return math.sqrt(sum(sample * sample for sample in samples) / len(samples))


def db_from_ratio(ratio):
    """Return a strict-JSON dB value, using null for negative infinity."""
    return None if ratio == 0.0 else 20.0 * math.log10(ratio)


def analyze_lag(reference, candidate):
    """Calculate shape metrics for one already aligned pair of windows."""
    reference_centered, reference_mean = centered(reference)
    candidate_centered, candidate_mean = centered(candidate)
    reference_energy = sum(sample * sample for sample in reference_centered)
    candidate_energy = sum(sample * sample for sample in candidate_centered)
    if reference_energy == 0.0:
        raise ComparisonError("reference window has no AC energy")
    if candidate_energy == 0.0:
        raise ComparisonError("candidate window has no AC energy")

    cross_energy = sum(
        reference_sample * candidate_sample
        for reference_sample, candidate_sample in zip(reference_centered, candidate_centered)
    )
    correlation = cross_energy / math.sqrt(reference_energy * candidate_energy)
    correlation = max(-1.0, min(1.0, correlation))
    gain = cross_energy / reference_energy
    residual = [
        candidate_sample - gain * reference_sample
        for reference_sample, candidate_sample in zip(reference_centered, candidate_centered)
    ]
    candidate_shape_rms = math.sqrt(candidate_energy / len(candidate_centered))
    residual_ratio = rms(residual) / candidate_shape_rms

    return {
        "correlation": correlation,
        "abs_correlation": abs(correlation),
        "polarity": "same" if correlation > 0.0 else "inverted" if correlation < 0.0 else "indeterminate",
        "gain_candidate_from_reference": gain,
        "gain_fitted_residual_percent": residual_ratio * 100.0,
        "gain_fitted_residual_db": db_from_ratio(residual_ratio),
        "reference_dc_pcm16": reference_mean,
        "candidate_dc_pcm16": candidate_mean,
    }


def compare(reference_data, candidate_data, start, length, max_lag, candidate_start=None):
    """Select the strongest-correlating valid lag and retain raw level metrics."""
    if reference_data["sample_rate"] != candidate_data["sample_rate"]:
        raise ComparisonError(
            "sample-rate mismatch: "
            f"{reference_data['sample_rate']} Hz reference, "
            f"{candidate_data['sample_rate']} Hz candidate"
        )
    if start < 0:
        raise ComparisonError("--reference-start must be nonnegative")
    if max_lag < 0:
        raise ComparisonError("--max-lag must be nonnegative")
    if candidate_start is not None and candidate_start < 0:
        raise ComparisonError("--candidate-start must be nonnegative")

    reference_samples = reference_data["mono"]
    candidate_samples = candidate_data["mono"]
    candidate_anchor = start if candidate_start is None else candidate_start
    if length is None:
        length = min(
            len(reference_samples) - start,
            len(candidate_samples) - candidate_anchor - max_lag,
        )
    if length <= 0:
        raise ComparisonError("--reference-length must be positive")
    if start + length > len(reference_samples):
        raise ComparisonError("reference window exceeds the reference WAV")

    reference_window = reference_samples[start : start + length]
    best = None
    for anchor_lag in range(-max_lag, max_lag + 1):
        window_start = candidate_anchor + anchor_lag
        candidate_end = window_start + length
        if window_start < 0 or candidate_end > len(candidate_samples):
            continue
        candidate_window = candidate_samples[window_start:candidate_end]
        try:
            shape = analyze_lag(reference_window, candidate_window)
        except ComparisonError as error:
            if str(error) == "candidate window has no AC energy":
                continue
            raise

        rank = (shape["correlation"], -abs(anchor_lag), -anchor_lag)
        if best is None or rank > best["rank"]:
            best = {
                "rank": rank,
                "lag": window_start - start,
                "candidate_start": window_start,
                "candidate_window": candidate_window,
                "shape": shape,
            }

    if best is None:
        raise ComparisonError("no valid candidate window with AC energy in the requested lag range")

    reference_raw_rms = rms(reference_window)
    candidate_raw_rms = rms(best["candidate_window"])
    if reference_raw_rms == 0.0:
        raise ComparisonError("reference window has zero raw RMS")
    raw_rms_ratio = candidate_raw_rms / reference_raw_rms

    return {
        "sample_rate": reference_data["sample_rate"],
        "reference_channels": reference_data["channels"],
        "candidate_channels": candidate_data["channels"],
        "reference_start_sample": start,
        "candidate_search_anchor_sample": candidate_anchor,
        "candidate_start_sample": best["candidate_start"],
        "length_samples": length,
        "best_lag_samples": best["lag"],
        "shape": best["shape"],
        "level": {
            "reference_raw_rms_pcm16": reference_raw_rms,
            "candidate_raw_rms_pcm16": candidate_raw_rms,
            "raw_rms_ratio_candidate_to_reference": raw_rms_ratio,
            "raw_rms_db_candidate_to_reference": db_from_ratio(raw_rms_ratio),
        },
    }


def apply_thresholds(result, min_correlation, max_residual_percent):
    """Turn optional shape thresholds into an automation-friendly gate result."""
    failures = []
    shape = result["shape"]
    if min_correlation is not None and shape["correlation"] < min_correlation:
        failures.append(
            f"correlation {shape['correlation']:.9g} is below {min_correlation:.9g}"
        )
    if max_residual_percent is not None and shape["gain_fitted_residual_percent"] > max_residual_percent:
        failures.append(
            "gain-fitted residual "
            f"{shape['gain_fitted_residual_percent']:.9g}% exceeds {max_residual_percent:.9g}%"
        )

    result["thresholds"] = {
        "min_correlation": min_correlation,
        "max_residual_percent": max_residual_percent,
    }
    result["passed"] = not failures
    result["failures"] = failures
    return not failures


def main():
    """Run the comparison and emit one strict JSON document."""
    args = parse_args()
    try:
        if args.min_correlation is not None and (
            not math.isfinite(args.min_correlation)
            or not 0.0 <= args.min_correlation <= 1.0
        ):
            raise ComparisonError("--min-correlation must be finite and between 0 and 1")
        if args.max_residual_percent is not None and (
            not math.isfinite(args.max_residual_percent)
            or args.max_residual_percent < 0.0
        ):
            raise ComparisonError("--max-residual-percent must be finite and nonnegative")

        reference_data = read_pcm16_mono(args.reference)
        candidate_data = read_pcm16_mono(args.candidate)
        result = compare(
            reference_data,
            candidate_data,
            args.reference_start,
            args.reference_length,
            args.max_lag,
            args.candidate_start,
        )
        result["reference"] = str(args.reference)
        result["candidate"] = str(args.candidate)
        passed = apply_thresholds(
            result,
            args.min_correlation,
            args.max_residual_percent,
        )
        print(json.dumps(result, indent=2, sort_keys=True, allow_nan=False))
        return 0 if passed else 1
    except ComparisonError as error:
        print(f"waveform_compare: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
