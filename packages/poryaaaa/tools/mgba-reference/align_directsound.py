#!/usr/bin/env python3
"""Sample-align two PCM16 WAVs and compare matching DirectSound windows."""

import argparse
import json
import math
import struct
import wave
from pathlib import Path


class AlignmentError(Exception):
    """Represent a capture pair that cannot be compared safely."""


def parse_args():
    """Describe the alignment gate used by the DirectSound workflow."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--window-length", type=int, default=2048)
    parser.add_argument("--search-radius", type=int, default=8192)
    parser.add_argument("--onset-threshold", type=float, default=0.01)
    parser.add_argument("--lag-tolerance", type=int, default=1)
    parser.add_argument("--min-correlation", type=float, default=0.98)
    parser.add_argument("--max-residual-percent", type=float, default=15.0)
    parser.add_argument("--max-level-db", type=float, default=0.25)
    return parser.parse_args()


def read_wav(path):
    """Read PCM16 mono/stereo data while retaining both output sides."""
    with wave.open(str(path), "rb") as wav:
        if wav.getsampwidth() != 2 or wav.getcomptype() != "NONE":
            raise AlignmentError(f"expected PCM16 WAV: {path}")
        channels = wav.getnchannels()
        if channels not in (1, 2):
            raise AlignmentError(f"expected mono or stereo WAV: {path}")
        rate = wav.getframerate()
        frames = wav.getnframes()
        values = struct.unpack(f"<{frames * channels}h", wav.readframes(frames))
    sides = [list(map(float, values[channel::channels])) for channel in range(channels)]
    mono = sides[0] if channels == 1 else [(left + right) / 2.0 for left, right in zip(*sides)]
    return {"rate": rate, "frames": frames, "channels": channels, "sides": sides, "mono": mono}


def rms(values):
    """Measure raw amplitude in PCM16 units."""
    return math.sqrt(sum(value * value for value in values) / len(values))


def centered(values):
    """Remove one window's DC component for shape comparison."""
    mean = sum(values) / len(values)
    return [value - mean for value in values], mean


def compare_window(reference, candidate):
    """Report signed correlation, level, fitted gain, and null residual."""
    ref_centered, ref_dc = centered(reference)
    cand_centered, cand_dc = centered(candidate)
    ref_energy = sum(value * value for value in ref_centered)
    cand_energy = sum(value * value for value in cand_centered)
    if ref_energy == 0 or cand_energy == 0:
        raise AlignmentError("comparison window contains no AC energy")
    cross = sum(a * b for a, b in zip(ref_centered, cand_centered))
    correlation = cross / math.sqrt(ref_energy * cand_energy)
    gain = cross / ref_energy
    residual = [b - gain * a for a, b in zip(ref_centered, cand_centered)]
    ref_rms = rms(reference)
    cand_rms = rms(candidate)
    level_ratio = cand_rms / ref_rms
    return {
        "correlation": correlation,
        "gain_candidate_from_reference": gain,
        "gain_fitted_residual_percent": rms(residual) / math.sqrt(cand_energy / len(candidate)) * 100.0,
        "level_db_candidate_to_reference": 20.0 * math.log10(level_ratio),
        "reference_rms_pcm16": ref_rms,
        "candidate_rms_pcm16": cand_rms,
        "reference_dc_pcm16": ref_dc,
        "candidate_dc_pcm16": cand_dc,
        "reference_peak_pcm16": max(abs(value) for value in reference),
        "candidate_peak_pcm16": max(abs(value) for value in candidate),
        "reference_clipped_samples": sum(abs(value) >= 32767 for value in reference),
        "candidate_clipped_samples": sum(abs(value) >= 32767 for value in candidate),
    }


def write_stereo_wav(path, rate, sides):
    """Write aligned analysis material without changing sample values."""
    frame_count = len(sides[0])
    interleaved = []
    for index in range(frame_count):
        for side in sides:
            interleaved.append(max(-32768, min(32767, round(side[index]))))
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(len(sides))
        wav.setsampwidth(2)
        wav.setframerate(rate)
        wav.writeframes(struct.pack(f"<{len(interleaved)}h", *interleaved))


def find_onset(values, threshold):
    """Find sustained audible activity without treating one spike as onset."""
    peak = max(abs(value) for value in values)
    cutoff = peak * threshold
    for index in range(0, len(values) - 32):
        if max(abs(value) for value in values[index : index + 32]) >= cutoff:
            return index
    raise AlignmentError("capture contains no audible onset")


def find_active_end(values, threshold):
    """Find the exclusive end of sustained activity using the capture peak."""
    peak = max(abs(value) for value in values)
    cutoff = peak * threshold
    for index in range(len(values) - 32, -1, -1):
        if max(abs(value) for value in values[index : index + 32]) >= cutoff:
            return index + 32
    raise AlignmentError("capture contains no audible activity")


def best_lag(reference, candidate, ref_start, candidate_anchor, length, radius):
    """Find the signed integer lag with the strongest positive correlation."""
    ref_window = reference[ref_start : ref_start + length]
    if len(ref_window) != length:
        raise AlignmentError("reference comparison window exceeds capture")
    best = None
    for delta in range(-radius, radius + 1):
        cand_start = candidate_anchor + delta
        cand_window = candidate[cand_start : cand_start + length]
        if cand_start < 0 or len(cand_window) != length:
            continue
        try:
            metrics = compare_window(ref_window, cand_window)
        except AlignmentError as error:
            if str(error) == "comparison window contains no AC energy":
                continue
            raise
        rank = (metrics["correlation"], -abs(delta), -delta)
        if best is None or rank > best[0]:
            best = (rank, cand_start, metrics)
    if best is None:
        raise AlignmentError("no valid candidate alignment window")
    return best[1], best[2]


def main():
    """Align onset/sustain/release windows and emit an automation verdict."""
    args = parse_args()
    try:
        reference = read_wav(args.reference)
        candidate = read_wav(args.candidate)
        if reference["rate"] != candidate["rate"]:
            raise AlignmentError("capture sample rates differ")
        if reference["rate"] != 65536:
            raise AlignmentError("DirectSound comparison requires 65536 Hz captures")
        if reference["channels"] != candidate["channels"]:
            raise AlignmentError("capture channel counts differ")
        ref_onset = find_onset(reference["mono"], args.onset_threshold)
        cand_onset = find_onset(candidate["mono"], args.onset_threshold)
        candidate_start, onset_metrics = best_lag(
            reference["mono"],
            candidate["mono"],
            ref_onset,
            cand_onset,
            args.window_length,
            args.search_radius,
        )
        global_lag = candidate_start - ref_onset

        overlap_start = max(ref_onset, -global_lag)
        overlap_end = min(reference["frames"], candidate["frames"] - global_lag)
        reference_active_end = min(
            find_active_end(reference["mono"], args.onset_threshold), overlap_end
        )
        available = reference_active_end - overlap_start
        if available < args.window_length * 3:
            raise AlignmentError("aligned captures do not contain three comparison windows")
        offsets = {
            "onset": 0,
            "sustain": max(args.window_length, available // 2 - args.window_length // 2),
            "release": available - args.window_length,
        }
        windows = {}
        failures = []
        for name, relative in offsets.items():
            ref_start = overlap_start + relative
            expected_candidate = ref_start + global_lag
            local_start, metrics = best_lag(
                reference["mono"],
                candidate["mono"],
                ref_start,
                expected_candidate,
                args.window_length,
                max(args.lag_tolerance, 1),
            )
            local_lag = local_start - ref_start
            side_metrics = []
            for side in range(reference["channels"]):
                side_metrics.append(
                    compare_window(
                        reference["sides"][side][ref_start : ref_start + args.window_length],
                        candidate["sides"][side][local_start : local_start + args.window_length],
                    )
                )
            windows[name] = {
                "reference_start": ref_start,
                "candidate_start": local_start,
                "lag_samples": local_lag,
                **metrics,
                "sides": side_metrics,
            }
            if abs(local_lag - global_lag) > args.lag_tolerance:
                failures.append(
                    f"{name} lag {local_lag} differs from global lag {global_lag}"
                )

        for name, window in windows.items():
            if window["correlation"] < args.min_correlation:
                failures.append(f"{name} correlation is below {args.min_correlation}")
            if window["gain_fitted_residual_percent"] > args.max_residual_percent:
                failures.append(f"{name} residual exceeds {args.max_residual_percent}%")
            if abs(window["level_db_candidate_to_reference"]) > args.max_level_db:
                failures.append(f"{name} level differs by more than {args.max_level_db} dB")

        result = {
            "sample_rate": reference["rate"],
            "reference_onset": ref_onset,
            "candidate_onset": cand_onset,
            "global_lag_samples": global_lag,
            "alignment_passed": not any("lag" in failure for failure in failures),
            "thresholds": {
                "lag_tolerance_samples": args.lag_tolerance,
                "min_correlation": args.min_correlation,
                "max_residual_percent": args.max_residual_percent,
                "max_level_db": args.max_level_db,
            },
            "windows": windows,
            "passed": not failures,
            "failures": failures,
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        aligned_reference = [
            side[overlap_start:reference_active_end] for side in reference["sides"]
        ]
        candidate_overlap_start = overlap_start + global_lag
        candidate_overlap_end = reference_active_end + global_lag
        aligned_candidate = [
            side[candidate_overlap_start:candidate_overlap_end] for side in candidate["sides"]
        ]
        difference = [
            [cand - ref for ref, cand in zip(ref_side, cand_side)]
            for ref_side, cand_side in zip(aligned_reference, aligned_candidate)
        ]
        write_stereo_wav(args.output.parent / "aligned-mgba.wav", reference["rate"], aligned_reference)
        write_stereo_wav(
            args.output.parent / "aligned-poryaaaa.wav", candidate["rate"], aligned_candidate
        )
        write_stereo_wav(args.output.parent / "difference.wav", reference["rate"], difference)
        args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result, indent=2))
        raise SystemExit(0 if not failures else 1)
    except (AlignmentError, OSError, wave.Error) as error:
        raise SystemExit(f"error: {error}") from error


if __name__ == "__main__":
    main()
