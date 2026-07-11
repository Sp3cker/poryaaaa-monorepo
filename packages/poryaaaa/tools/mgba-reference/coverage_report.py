#!/usr/bin/env python3
"""Report onset-anchored mono waveform coverage for capture-pair directories."""

import argparse
import json
import math
import sys
from pathlib import Path

from waveform_compare import ComparisonError, apply_thresholds, compare, read_pcm16_mono


# This is about -54 dBFS: above PCM quantization and quiet frontend settling noise.
ONSET_THRESHOLD_PCM16 = 64.0


def parse_args():
    """Describe the batch coverage interface used by reference capture runs."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "pair_directories",
        nargs="+",
        type=Path,
        metavar="PAIR_DIRECTORY",
        help="directory containing mgba.wav, poryaaaa.wav, and manifest.txt",
    )
    parser.add_argument(
        "--length",
        type=int,
        default=512,
        metavar="SAMPLES",
        help="onset comparison window length (default: 512)",
    )
    parser.add_argument(
        "--max-lag",
        type=int,
        default=64,
        metavar="SAMPLES",
        help="local candidate search around its detected onset (default: 64)",
    )
    parser.add_argument(
        "--scan-span",
        type=int,
        default=4096,
        metavar="SAMPLES",
        help="relative onset span searched for the strongest shape match (default: 4096)",
    )
    parser.add_argument(
        "--scan-step",
        type=int,
        default=128,
        metavar="SAMPLES",
        help="distance between scanned comparison windows (default: 128)",
    )
    parser.add_argument(
        "--min-correlation",
        type=float,
        default=0.999,
        metavar="VALUE",
        help="minimum signed correlation for a passing pair (default: 0.999)",
    )
    parser.add_argument(
        "--max-residual-percent",
        type=float,
        default=5.0,
        metavar="PERCENT",
        help="maximum gain-fitted residual for a passing pair (default: 5)",
    )
    return parser.parse_args()


def read_manifest(path):
    """Read the capture manifest fields needed to identify a coverage result."""
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise ComparisonError(f"cannot read {path}: {error}") from error

    manifest = {}
    for line in lines:
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        manifest[key] = value

    missing = [key for key in ("song", "solo") if not manifest.get(key)]
    if missing:
        raise ComparisonError(f"{path}: missing manifest field(s): {', '.join(missing)}")
    return manifest


def find_onset(samples, path):
    """Find the first mono sample that clears the fixed audible activity gate."""
    for index, sample in enumerate(samples):
        if abs(sample) >= ONSET_THRESHOLD_PCM16:
            return index
    raise ComparisonError(
        f"{path}: no sample reaches the {ONSET_THRESHOLD_PCM16:g} PCM16 onset threshold"
    )


def find_comparison(
    reference,
    candidate,
    reference_onset,
    candidate_onset,
    length,
    max_lag,
    scan_span,
    scan_step,
    min_correlation,
    max_residual_percent,
):
    """Select the earliest audible same-relative-time window that passes."""
    selected = None
    for offset in range(0, scan_span + 1, scan_step):
        reference_start = reference_onset + offset
        candidate_start = candidate_onset + offset
        if reference_start + length > len(reference["mono"]):
            break
        try:
            comparison = compare(
                reference,
                candidate,
                reference_start,
                length,
                max_lag,
                candidate_start,
            )
        except ComparisonError:
            continue
        if (
            comparison["level"]["reference_raw_rms_pcm16"] < ONSET_THRESHOLD_PCM16
            or comparison["level"]["candidate_raw_rms_pcm16"] < ONSET_THRESHOLD_PCM16
        ):
            continue
        apply_thresholds(comparison, min_correlation, max_residual_percent)
        if comparison["passed"]:
            return offset, comparison
        rank = (
            comparison["shape"]["correlation"],
            -comparison["shape"]["gain_fitted_residual_percent"],
        )
        if selected is None or rank > selected[0]:
            selected = (rank, offset, comparison)

    if selected is None:
        raise ComparisonError("no audible comparison window fits inside the requested scan span")
    return selected[1], selected[2]


def report_pair(
    pair_directory,
    length,
    max_lag,
    scan_span,
    scan_step,
    min_correlation,
    max_residual_percent,
):
    """Analyze one capture pair and reduce the comparator result to coverage metrics."""
    reference_path = pair_directory / "mgba.wav"
    candidate_path = pair_directory / "poryaaaa.wav"
    manifest_path = pair_directory / "manifest.txt"
    manifest = read_manifest(manifest_path)
    reference = read_pcm16_mono(reference_path)
    candidate = read_pcm16_mono(candidate_path)
    reference_onset = find_onset(reference["mono"], reference_path)
    candidate_onset = find_onset(candidate["mono"], candidate_path)

    selected_offset, comparison = find_comparison(
        reference,
        candidate,
        reference_onset,
        candidate_onset,
        length,
        max_lag,
        scan_span,
        scan_step,
        min_correlation,
        max_residual_percent,
    )
    shape = comparison["shape"]
    level = comparison["level"]

    return {
        "pair_directory": str(pair_directory),
        "coverage_scope": "selected_local_wave_window",
        "selection_policy": "earliest_passing_audible_window",
        "song": manifest["song"],
        "solo": manifest["solo"],
        "sample_rate_hz": comparison["sample_rate"],
        "mono_fold": "arithmetic_mean",
        "anchors": {
            "onset_threshold_pcm16": ONSET_THRESHOLD_PCM16,
            "mgba_onset_sample": reference_onset,
            "poryaaaa_onset_sample": candidate_onset,
            "selected_offset_from_onsets_samples": selected_offset,
            "poryaaaa_selected_sample": comparison["candidate_start_sample"],
            "selected_lag_from_relative_poryaaaa_window_samples": (
                comparison["candidate_start_sample"] - (candidate_onset + selected_offset)
            ),
        },
        "window": {
            "length_samples": comparison["length_samples"],
            "max_lag_samples": max_lag,
            "scan_span_samples": scan_span,
            "scan_step_samples": scan_step,
        },
        "correlation": shape["correlation"],
        "absolute_correlation": shape["abs_correlation"],
        "polarity": shape["polarity"],
        "gain_fitted_residual_percent": shape["gain_fitted_residual_percent"],
        "level_comparable": comparison["passed"],
        "raw_rms_db_poryaaaa_to_mgba": (
            level["raw_rms_db_candidate_to_reference"] if comparison["passed"] else None
        ),
        "thresholds": comparison["thresholds"],
        "passed": comparison["passed"],
        "failures": comparison["failures"],
    }


def validate_args(args):
    """Reject invalid numeric gates before processing any pair directories."""
    if args.length <= 0:
        raise ComparisonError("--length must be positive")
    if args.max_lag < 0:
        raise ComparisonError("--max-lag must be nonnegative")
    if args.scan_span < 0:
        raise ComparisonError("--scan-span must be nonnegative")
    if args.scan_step <= 0:
        raise ComparisonError("--scan-step must be positive")
    if not math.isfinite(args.min_correlation) or not 0.0 <= args.min_correlation <= 1.0:
        raise ComparisonError("--min-correlation must be finite and between 0 and 1")
    if not math.isfinite(args.max_residual_percent) or args.max_residual_percent < 0.0:
        raise ComparisonError("--max-residual-percent must be finite and nonnegative")


def main():
    """Emit one strict JSON object per input pair and return a batch gate status."""
    args = parse_args()
    try:
        validate_args(args)
        passed = True
        for pair_directory in args.pair_directories:
            report = report_pair(
                pair_directory,
                args.length,
                args.max_lag,
                args.scan_span,
                args.scan_step,
                args.min_correlation,
                args.max_residual_percent,
            )
            print(json.dumps(report, sort_keys=True, allow_nan=False))
            passed = passed and report["passed"]
        return 0 if passed else 1
    except ComparisonError as error:
        print(f"coverage_report: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
