#!/usr/bin/env python3
"""Compare cycle-stamped native stereo PCM captures without normalization."""

import argparse
import hashlib
import json
import mmap
import struct
import sys
from pathlib import Path


class ComparisonError(Exception):
    """Represent malformed capture input with a stable CLI diagnostic."""


def parse_args():
    """Expose the exact two-capture comparison used by automation."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path, help="reference capture .json manifest")
    parser.add_argument("candidate", type=Path, help="candidate capture .json manifest")
    return parser.parse_args()


def sha256_file(path):
    """Hash an artifact without loading a full song capture into memory."""
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_capture(manifest_path):
    """Validate one canonical manifest and its fixed-size sibling artifacts."""
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ComparisonError(f"cannot read {manifest_path}: {error}") from error

    required = {
        "format": "poryaaaa-native-capture",
        "version": 1,
        "channels": 2,
        "sample_format": "s16le",
        "cycle_format": "u64le",
    }
    for key, expected in required.items():
        if manifest.get(key) != expected:
            raise ComparisonError(
                f"{manifest_path}: expected {key}={expected!r}, got {manifest.get(key)!r}"
            )

    frame_count = manifest.get("frame_count")
    clock_hz = manifest.get("clock_hz")
    first_cycle = manifest.get("first_cycle")
    last_cycle = manifest.get("last_cycle")
    solo_mask = manifest.get("solo_mask")
    if not isinstance(frame_count, int) or isinstance(frame_count, bool) or frame_count <= 0:
        raise ComparisonError(f"{manifest_path}: frame_count must be a positive integer")
    if not isinstance(clock_hz, int) or isinstance(clock_hz, bool) or clock_hz <= 0:
        raise ComparisonError(f"{manifest_path}: clock_hz must be a positive integer")
    if not isinstance(first_cycle, int) or not isinstance(last_cycle, int):
        raise ComparisonError(f"{manifest_path}: first_cycle and last_cycle must be integers")
    if first_cycle < 0 or last_cycle < first_cycle:
        raise ComparisonError(f"{manifest_path}: invalid first/last cycle range")
    if not isinstance(solo_mask, int) or isinstance(solo_mask, bool) or not 1 <= solo_mask <= 63:
        raise ComparisonError(f"{manifest_path}: solo_mask must be an integer between 1 and 63")
    source = manifest.get("source")
    if source in {"mgba-full", "mgba-clone"}:
        mgba_contract = {
            "audio_channel_mask": solo_mask,
            "mgba_master_volume": 0x100,
            "bios_mode": "hle",
        }
        for key, expected in mgba_contract.items():
            if manifest.get(key) != expected:
                raise ComparisonError(
                    f"{manifest_path}: expected {key}={expected!r}, got {manifest.get(key)!r}"
                )

    pcm_path = manifest_path.with_suffix(".pcm")
    cycles_path = manifest_path.with_suffix(".cycles")
    try:
        pcm_size = pcm_path.stat().st_size
        cycles_size = cycles_path.stat().st_size
    except OSError as error:
        raise ComparisonError(f"{manifest_path}: missing capture artifact: {error}") from error
    expected_pcm_size = frame_count * 4
    expected_cycles_size = frame_count * 8
    if pcm_size != expected_pcm_size:
        raise ComparisonError(
            f"{pcm_path}: expected {expected_pcm_size} bytes for {frame_count} frames, got {pcm_size}"
        )
    if cycles_size != expected_cycles_size:
        raise ComparisonError(
            f"{cycles_path}: expected {expected_cycles_size} bytes for {frame_count} frames, got {cycles_size}"
        )
    try:
        artifact_first_cycle = None
        artifact_last_cycle = None
        with cycles_path.open("rb") as cycle_file:
            for frame_index in range(frame_count):
                cycle = struct.unpack("<Q", cycle_file.read(8))[0]
                if artifact_last_cycle is not None and cycle <= artifact_last_cycle:
                    raise ComparisonError(
                        f"{cycles_path}: cycle is not increasing at frame {frame_index}"
                    )
                if artifact_first_cycle is None:
                    artifact_first_cycle = cycle
                artifact_last_cycle = cycle
    except (OSError, struct.error) as error:
        raise ComparisonError(f"{cycles_path}: cannot read cycles: {error}") from error
    if artifact_first_cycle != first_cycle or artifact_last_cycle != last_cycle:
        raise ComparisonError(
            f"{cycles_path}: cycle endpoints do not match manifest "
            f"({artifact_first_cycle}..{artifact_last_cycle} artifact, "
            f"{first_cycle}..{last_cycle} manifest)"
        )

    return {
        "manifest_path": manifest_path,
        "manifest": manifest,
        "frame_count": frame_count,
        "clock_hz": clock_hz,
        "first_cycle": first_cycle,
        "last_cycle": last_cycle,
        "solo_mask": solo_mask,
        "audio_channel_mask": manifest.get("audio_channel_mask"),
        "mgba_master_volume": manifest.get("mgba_master_volume"),
        "bios_mode": manifest.get("bios_mode"),
        "pcm_path": pcm_path,
        "cycles_path": cycles_path,
    }


def compare(reference, candidate):
    """Find every timing and stereo integer mismatch at matching frame indices."""
    contract_failures = []
    for field in (
        "clock_hz",
        "solo_mask",
        "audio_channel_mask",
        "mgba_master_volume",
        "bios_mode",
    ):
        if reference[field] is not None and candidate[field] is not None and reference[field] != candidate[field]:
            contract_failures.append(
                f"{field} mismatch: {reference[field]} reference, {candidate[field]} candidate"
            )

    result = {
        "reference": str(reference["manifest_path"]),
        "candidate": str(candidate["manifest_path"]),
        "reference_source": reference["manifest"].get("source"),
        "candidate_source": candidate["manifest"].get("source"),
        "reference_frame_count": reference["frame_count"],
        "candidate_frame_count": candidate["frame_count"],
        "clock_hz": reference["clock_hz"],
        "contract_failures": contract_failures,
        "hashes": {
            "reference_pcm_sha256": sha256_file(reference["pcm_path"]),
            "candidate_pcm_sha256": sha256_file(candidate["pcm_path"]),
            "reference_cycles_sha256": sha256_file(reference["cycles_path"]),
            "candidate_cycles_sha256": sha256_file(candidate["cycles_path"]),
        },
    }
    if contract_failures:
        result.update(
            {
                "passed": False,
                "first_mismatch": {"kind": "capture_contract", "failures": contract_failures},
            }
        )
        return result

    compared_frames = min(reference["frame_count"], candidate["frame_count"])
    cycle_mismatches = 0
    left_mismatches = 0
    right_mismatches = 0
    max_left_error = 0
    max_right_error = 0
    first_mismatch = None

    try:
        with (
            reference["pcm_path"].open("rb") as reference_pcm_file,
            candidate["pcm_path"].open("rb") as candidate_pcm_file,
            reference["cycles_path"].open("rb") as reference_cycles_file,
            candidate["cycles_path"].open("rb") as candidate_cycles_file,
            mmap.mmap(reference_pcm_file.fileno(), 0, access=mmap.ACCESS_READ) as reference_pcm,
            mmap.mmap(candidate_pcm_file.fileno(), 0, access=mmap.ACCESS_READ) as candidate_pcm,
            mmap.mmap(reference_cycles_file.fileno(), 0, access=mmap.ACCESS_READ) as reference_cycles,
            mmap.mmap(candidate_cycles_file.fileno(), 0, access=mmap.ACCESS_READ) as candidate_cycles,
        ):
            for frame_index in range(compared_frames):
                reference_cycle = struct.unpack_from("<Q", reference_cycles, frame_index * 8)[0]
                candidate_cycle = struct.unpack_from("<Q", candidate_cycles, frame_index * 8)[0]
                reference_left, reference_right = struct.unpack_from(
                    "<hh", reference_pcm, frame_index * 4
                )
                candidate_left, candidate_right = struct.unpack_from(
                    "<hh", candidate_pcm, frame_index * 4
                )

                cycle_differs = reference_cycle != candidate_cycle
                left_error = candidate_left - reference_left
                right_error = candidate_right - reference_right
                left_differs = left_error != 0
                right_differs = right_error != 0
                cycle_mismatches += int(cycle_differs)
                left_mismatches += int(left_differs)
                right_mismatches += int(right_differs)
                max_left_error = max(max_left_error, abs(left_error))
                max_right_error = max(max_right_error, abs(right_error))

                if first_mismatch is None and (cycle_differs or left_differs or right_differs):
                    kinds = []
                    if cycle_differs:
                        kinds.append("cycle")
                    if left_differs:
                        kinds.append("left")
                    if right_differs:
                        kinds.append("right")
                    first_mismatch = {
                        "kind": "+".join(kinds),
                        "frame_index": frame_index,
                        "reference_cycle": reference_cycle,
                        "candidate_cycle": candidate_cycle,
                        "reference_left": reference_left,
                        "candidate_left": candidate_left,
                        "left_error": left_error,
                        "reference_right": reference_right,
                        "candidate_right": candidate_right,
                        "right_error": right_error,
                    }
    except OSError as error:
        raise ComparisonError(f"cannot map capture artifacts: {error}") from error

    frame_count_matches = reference["frame_count"] == candidate["frame_count"]
    if first_mismatch is None and not frame_count_matches:
        first_mismatch = {
            "kind": "frame_count",
            "frame_index": compared_frames,
            "reference_frame_count": reference["frame_count"],
            "candidate_frame_count": candidate["frame_count"],
        }

    passed = (
        frame_count_matches
        and cycle_mismatches == 0
        and left_mismatches == 0
        and right_mismatches == 0
    )
    result.update(
        {
            "compared_frames": compared_frames,
            "cycle_mismatch_count": cycle_mismatches,
            "left_mismatch_count": left_mismatches,
            "right_mismatch_count": right_mismatches,
            "max_abs_left_error_pcm16": max_left_error,
            "max_abs_right_error_pcm16": max_right_error,
            "first_mismatch": first_mismatch,
            "passed": passed,
        }
    )
    return result


def main():
    """Run the exact gate and emit one strict JSON result."""
    args = parse_args()
    try:
        reference = load_capture(args.reference)
        candidate = load_capture(args.candidate)
        result = compare(reference, candidate)
        print(json.dumps(result, indent=2, sort_keys=True, allow_nan=False))
        return 0 if result["passed"] else 1
    except ComparisonError as error:
        print(f"native_compare: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
