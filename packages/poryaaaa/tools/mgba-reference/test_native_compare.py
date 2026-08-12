#!/usr/bin/env python3
"""Exercise native_compare.py with deterministic cycle-stamped stereo captures."""

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


TOOL = Path(__file__).with_name("native_compare.py")
CLOCK_HZ = 16_777_216


def write_capture(
    manifest_path,
    frames,
    cycles,
    *,
    clock_hz=CLOCK_HZ,
    source="test",
    solo_mask=63,
    manifest_overrides=None,
):
    """Write one canonical capture without introducing test dependencies."""
    manifest = {
        "format": "poryaaaa-native-capture",
        "version": 1,
        "source": source,
        "clock_hz": clock_hz,
        "channels": 2,
        "sample_format": "s16le",
        "cycle_format": "u64le",
        "frame_count": len(frames),
        "first_cycle": cycles[0],
        "last_cycle": cycles[-1],
        "solo_mask": solo_mask,
    }
    if source in {"mgba-full", "mgba-clone"}:
        manifest.update(
            {
                "audio_channel_mask": solo_mask,
                "mgba_master_volume": 0x100,
                "bios_mode": "hle",
            }
        )
    if manifest_overrides:
        manifest.update(manifest_overrides)
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    pcm = bytearray()
    for left, right in frames:
        pcm.extend(int(left).to_bytes(2, "little", signed=True))
        pcm.extend(int(right).to_bytes(2, "little", signed=True))
    manifest_path.with_suffix(".pcm").write_bytes(pcm)
    cycle_bytes = bytearray()
    for cycle in cycles:
        cycle_bytes.extend(int(cycle).to_bytes(8, "little", signed=False))
    manifest_path.with_suffix(".cycles").write_bytes(cycle_bytes)


def write_trace(path, sample_positions, events=()):
    """Write one strict trace whose source SAMPLE positions are test-controlled."""
    lines = ["PORYAAAA_AUDIO_TRACE 1", "CLOCK 16777216", "BEGIN 0 0", *events]
    for cycle, order in sample_positions:
        lines.append(f"SAMPLE {cycle} {order}")
    lines.append(f"END {sample_positions[-1][0] + 1} 0")
    path.write_text("\n".join(lines) + "\n", encoding="ascii")


class NativeCompareTest(unittest.TestCase):
    """Prove exact timing and stereo integer mismatches fail automation."""

    def setUp(self):
        """Create isolated artifacts for each public-CLI invocation."""
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.directory = Path(self.temporary_directory.name)
        self.frames = [(index * 17 - 200, 300 - index * 11) for index in range(32)]
        self.cycles = [index * 256 for index in range(32)]

    def tearDown(self):
        """Remove generated capture artifacts."""
        self.temporary_directory.cleanup()

    def run_compare(self, reference, candidate, reference_trace=None, candidate_trace=None):
        """Invoke the comparator exactly as capture automation does."""
        command = [sys.executable, str(TOOL), str(reference), str(candidate)]
        if reference_trace is not None:
            command.extend(["--reference-trace", str(reference_trace)])
        if candidate_trace is not None:
            command.extend(["--candidate-trace", str(candidate_trace)])
        return subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
        )

    def test_identical_stereo_capture_passes(self):
        """Identical cycles and signed stereo samples must pass bit-exactly."""
        reference = self.directory / "reference.json"
        candidate = self.directory / "candidate.json"
        write_capture(reference, self.frames, self.cycles, source="mgba-clone")
        write_capture(candidate, self.frames, self.cycles, source="poryaaaa")

        completed = self.run_compare(reference, candidate)

        self.assertEqual(completed.returncode, 0, completed.stderr or completed.stdout)
        result = json.loads(completed.stdout)
        self.assertTrue(result["passed"])
        self.assertIsNone(result["first_mismatch"])
        self.assertEqual(result["left_mismatch_count"], 0)
        self.assertEqual(result["right_mismatch_count"], 0)

    def test_one_lsb_corruption_fails_with_exact_location(self):
        """A one-LSB left-channel change must fail and identify its frame."""
        reference = self.directory / "reference.json"
        candidate = self.directory / "candidate.json"
        candidate_frames = list(self.frames)
        candidate_frames[7] = (candidate_frames[7][0] + 1, candidate_frames[7][1])
        write_capture(reference, self.frames, self.cycles)
        write_capture(candidate, candidate_frames, self.cycles)

        completed = self.run_compare(reference, candidate)

        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        result = json.loads(completed.stdout)
        self.assertFalse(result["passed"])
        self.assertEqual(
            result["first_mismatch"],
            {
                "kind": "left",
                "frame_index": 7,
                "reference_cycle": 1792,
                "candidate_cycle": 1792,
                "reference_left": -81,
                "candidate_left": -80,
                "left_error": 1,
                "reference_right": 223,
                "candidate_right": 223,
                "right_error": 0,
            },
        )
        self.assertEqual(result["max_abs_left_error_pcm16"], 1)

    def test_pcm_mismatch_reports_unretimed_causal_source_samples(self):
        """The first PCM mismatch must cite each trace's real preceding SAMPLE."""
        reference = self.directory / "reference.json"
        candidate = self.directory / "candidate.json"
        reference_trace = self.directory / "reference.trace"
        candidate_trace = self.directory / "candidate.trace"
        candidate_frames = list(self.frames)
        candidate_frames[7] = (candidate_frames[7][0], candidate_frames[7][1] + 1)
        sample_positions = [(0, 4), (512, 11), (1024, 18), (1536, 25), (2048, 32)]
        write_capture(reference, self.frames, self.cycles)
        write_capture(candidate, candidate_frames, self.cycles)
        directsound_events = [
            "WRITE 0 1 2 0x04000084 0x00000080",
            "WRITE 0 2 2 0x04000082 0x00000300",
            "WRITE 0 3 4 0x040000A0 0x04030201",
            "TIMER 256 0 0",
        ]
        write_trace(reference_trace, sample_positions, directsound_events)
        write_trace(candidate_trace, sample_positions, directsound_events)

        completed = self.run_compare(reference, candidate, reference_trace, candidate_trace)

        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        mismatch = json.loads(completed.stdout)["first_mismatch"]
        self.assertEqual(mismatch["frame_index"], 7)
        self.assertEqual(mismatch["reference_cycle"], 1792)
        self.assertEqual(mismatch["reference_left"], -81)
        self.assertEqual(mismatch["candidate_right"], 224)
        self.assertEqual(
            mismatch["reference_causal_sample"],
            {"ordinal": 4, "line": 11, "cycle": 1536, "order": 25, "cycle_delta": 256},
        )
        self.assertEqual(mismatch["candidate_causal_sample"], mismatch["reference_causal_sample"])

    def test_trace_hash_mismatch_rejects_unrelated_causal_source(self):
        """Attribution must not pair a hashed capture with another trace."""
        reference = self.directory / "reference.json"
        candidate = self.directory / "candidate.json"
        reference_trace = self.directory / "reference.trace"
        candidate_trace = self.directory / "candidate.trace"
        sample_positions = [(0, 0), (512, 0)]
        write_capture(
            reference,
            self.frames,
            self.cycles,
            manifest_overrides={"trace_sha256": "0" * 64},
        )
        write_capture(candidate, self.frames, self.cycles)
        write_trace(reference_trace, sample_positions)
        write_trace(candidate_trace, sample_positions)

        completed = self.run_compare(reference, candidate, reference_trace, candidate_trace)

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(completed.stdout, "")
        self.assertIn("trace_sha256 does not match reference trace", completed.stderr)

    def test_stereo_swap_fails_both_channels(self):
        """Swapped routing must not disappear through a mono fold."""
        reference = self.directory / "reference.json"
        candidate = self.directory / "candidate.json"
        swapped = [(right, left) for left, right in self.frames]
        write_capture(reference, self.frames, self.cycles)
        write_capture(candidate, swapped, self.cycles)

        completed = self.run_compare(reference, candidate)

        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        result = json.loads(completed.stdout)
        self.assertFalse(result["passed"])
        self.assertEqual(
            result["first_mismatch"],
            {
                "kind": "left+right",
                "frame_index": 0,
                "reference_cycle": 0,
                "candidate_cycle": 0,
                "reference_left": -200,
                "candidate_left": 300,
                "left_error": 500,
                "reference_right": 300,
                "candidate_right": -200,
                "right_error": -500,
            },
        )
        self.assertEqual(result["left_mismatch_count"], len(self.frames))
        self.assertEqual(result["right_mismatch_count"], len(self.frames))

    def test_cycle_shift_fails_without_lag_search(self):
        """A one-cycle timing shift must fail at the first shifted frame."""
        reference = self.directory / "reference.json"
        candidate = self.directory / "candidate.json"
        shifted_cycles = list(self.cycles)
        shifted_cycles[9] += 1
        write_capture(reference, self.frames, self.cycles)
        write_capture(candidate, self.frames, shifted_cycles)

        completed = self.run_compare(reference, candidate)

        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        result = json.loads(completed.stdout)
        self.assertEqual(
            result["first_mismatch"],
            {
                "kind": "cycle",
                "frame_index": 9,
                "reference_cycle": 2304,
                "candidate_cycle": 2305,
                "reference_left": -47,
                "candidate_left": -47,
                "left_error": 0,
                "reference_right": 201,
                "candidate_right": 201,
                "right_error": 0,
            },
        )
        self.assertEqual(result["cycle_mismatch_count"], 1)

    def test_clock_mismatch_fails_capture_contract(self):
        """Different clock domains must stop sample-index comparison."""
        reference = self.directory / "reference.json"
        candidate = self.directory / "candidate.json"
        write_capture(reference, self.frames, self.cycles)
        write_capture(candidate, self.frames, self.cycles, clock_hz=CLOCK_HZ // 2)

        completed = self.run_compare(reference, candidate)

        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        result = json.loads(completed.stdout)
        self.assertFalse(result["passed"])
        self.assertEqual(
            result["first_mismatch"],
            {
                "kind": "capture_contract",
                "failures": [
                    "clock_hz mismatch: 16777216 reference, 8388608 candidate"
                ],
            },
        )
        self.assertEqual(
            result["contract_failures"],
            ["clock_hz mismatch: 16777216 reference, 8388608 candidate"],
        )
        self.assertEqual(result["first_mismatch"]["kind"], "capture_contract")
        self.assertTrue(result["contract_failures"])

    def test_solo_mask_mismatch_fails_capture_contract(self):
        """Different channel isolation masks must not be compared as peers."""
        reference = self.directory / "reference.json"
        candidate = self.directory / "candidate.json"
        write_capture(reference, self.frames, self.cycles, solo_mask=1)
        write_capture(candidate, self.frames, self.cycles, solo_mask=2)

        completed = self.run_compare(reference, candidate)

        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        result = json.loads(completed.stdout)
        self.assertIn("solo_mask mismatch", result["contract_failures"][0])

    def test_mgba_manifest_requires_authoritative_audio_contract(self):
        """mGBA artifacts with a different configured volume must be rejected."""
        reference = self.directory / "reference.json"
        candidate = self.directory / "candidate.json"
        write_capture(reference, self.frames, self.cycles, source="mgba-full")
        write_capture(
            candidate,
            self.frames,
            self.cycles,
            source="mgba-clone",
            manifest_overrides={"mgba_master_volume": 128},
        )

        completed = self.run_compare(reference, candidate)

        self.assertEqual(completed.returncode, 2, completed.stderr or completed.stdout)
        self.assertIn("expected mgba_master_volume=256, got 128", completed.stderr)

    def test_missing_candidate_frame_fails_at_first_unpaired_index(self):
        """One missing valid frame must identify the first unavailable index."""
        reference = self.directory / "reference.json"
        candidate = self.directory / "candidate.json"
        write_capture(reference, self.frames, self.cycles)
        write_capture(candidate, self.frames[:-1], self.cycles[:-1])

        completed = self.run_compare(reference, candidate)

        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        result = json.loads(completed.stdout)
        self.assertFalse(result["passed"])
        self.assertEqual(
            result["first_mismatch"],
            {
                "kind": "frame_count",
                "frame_index": 31,
                "reference_frame_count": 32,
                "candidate_frame_count": 31,
            },
        )
        self.assertEqual(result["compared_frames"], 31)
        self.assertEqual(result["cycle_mismatch_count"], 0)
        self.assertEqual(result["left_mismatch_count"], 0)
        self.assertEqual(result["right_mismatch_count"], 0)

    def test_extra_candidate_frame_fails_at_first_extra_index(self):
        """One extra valid frame must identify the first non-reference index."""
        reference = self.directory / "reference.json"
        candidate = self.directory / "candidate.json"
        extra_frames = [*self.frames, (123, -456)]
        extra_cycles = [*self.cycles, self.cycles[-1] + 256]
        write_capture(reference, self.frames, self.cycles)
        write_capture(candidate, extra_frames, extra_cycles)

        completed = self.run_compare(reference, candidate)

        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        result = json.loads(completed.stdout)
        self.assertFalse(result["passed"])
        self.assertEqual(
            result["first_mismatch"],
            {
                "kind": "frame_count",
                "frame_index": 32,
                "reference_frame_count": 32,
                "candidate_frame_count": 33,
            },
        )
        self.assertEqual(result["compared_frames"], 32)
        self.assertEqual(result["cycle_mismatch_count"], 0)
        self.assertEqual(result["left_mismatch_count"], 0)
        self.assertEqual(result["right_mismatch_count"], 0)

    def test_same_cycle_capture_frames_are_rejected_as_malformed(self):
        """Same-cycle output frames have no canonical ordering and must fail."""
        reference = self.directory / "reference.json"
        candidate = self.directory / "candidate.json"
        invalid_cycles = list(self.cycles)
        invalid_cycles[10] = invalid_cycles[9]
        write_capture(reference, self.frames, self.cycles)
        write_capture(candidate, self.frames, invalid_cycles)

        completed = self.run_compare(reference, candidate)

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(completed.stdout, "")
        self.assertIn("cycle is not increasing at frame 10", completed.stderr)

    def test_truncated_pcm_is_rejected_as_malformed(self):
        """Artifact size disagreement must return the input-error status."""
        reference = self.directory / "reference.json"
        candidate = self.directory / "candidate.json"
        write_capture(reference, self.frames, self.cycles)
        write_capture(candidate, self.frames, self.cycles)
        pcm_path = candidate.with_suffix(".pcm")
        pcm_path.write_bytes(pcm_path.read_bytes()[:-1])
        completed = self.run_compare(reference, candidate)

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(completed.stdout, "")
        self.assertIn("expected 128 bytes for 32 frames, got 127", completed.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
