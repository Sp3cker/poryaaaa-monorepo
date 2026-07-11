#!/usr/bin/env python3
"""Exercise coverage_report.py with deterministic capture-pair fixtures."""

import json
import subprocess
import sys
import tempfile
import unittest
import wave
from pathlib import Path


TOOL = Path(__file__).with_name("coverage_report.py")
SAMPLE_RATE = 65536


def write_wav(path, samples, channels=1):
    """Write a PCM16 mono or stereo fixture using only the standard library."""
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(channels)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)
        frames = bytearray()
        for sample in samples:
            channel_samples = sample if isinstance(sample, tuple) else (sample,) * channels
            for channel_sample in channel_samples:
                frames.extend(int(channel_sample).to_bytes(2, "little", signed=True))
        wav.writeframes(frames)


def noise_samples(count):
    """Generate an aperiodic deterministic waveform for alignment assertions."""
    state = 0x5A5A
    samples = []
    for _ in range(count):
        samples.append(4000 if state & 1 else -4000)
        feedback = (state ^ (state >> 1)) & 1
        state = (state >> 1) | (feedback << 14)
    return samples


class CoverageReportTest(unittest.TestCase):
    """Prove metadata, onset anchoring, mono folding, and threshold status."""

    def setUp(self):
        """Create an isolated capture root for each test."""
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.directory = Path(self.temporary_directory.name)

    def tearDown(self):
        """Remove generated capture-pair fixtures after each test."""
        self.temporary_directory.cleanup()

    def make_pair(self, name, reference, candidate, candidate_channels=1):
        """Create the three files expected from capture_song_pair.sh."""
        pair = self.directory / name
        pair.mkdir()
        write_wav(pair / "mgba.wav", reference)
        write_wav(pair / "poryaaaa.wav", candidate, channels=candidate_channels)
        (pair / "manifest.txt").write_text(
            f"manifest_version=1\nsong={name}\nsolo=sq1\n",
            encoding="utf-8",
        )
        return pair

    def run_report(self, *pairs, extra_args=()):
        """Invoke the public CLI and capture its JSON Lines output."""
        command = [sys.executable, str(TOOL), *(str(pair) for pair in pairs), *extra_args]
        return subprocess.run(command, check=False, capture_output=True, text=True)

    def test_reports_onset_anchored_stereo_fold_and_level(self):
        """Different leading silence and stereo side offsets must preserve the mono wave."""
        signal = noise_samples(640)
        reference = [0] * 37 + signal
        candidate = [(12, -12)] * 83 + [
            (sample // 2 + 200, sample // 2 - 200) for sample in signal
        ]
        pair = self.make_pair("square_song", reference, candidate, candidate_channels=2)

        completed = self.run_report(pair)

        self.assertEqual(completed.returncode, 0, completed.stderr)
        report = json.loads(completed.stdout)
        self.assertEqual(report["coverage_scope"], "selected_local_wave_window")
        self.assertEqual(report["selection_policy"], "earliest_passing_audible_window")
        self.assertEqual(report["song"], "square_song")
        self.assertEqual(report["solo"], "sq1")
        self.assertEqual(report["mono_fold"], "arithmetic_mean")
        self.assertEqual(report["anchors"]["onset_threshold_pcm16"], 64.0)
        self.assertEqual(report["anchors"]["mgba_onset_sample"], 37)
        self.assertEqual(report["anchors"]["poryaaaa_onset_sample"], 83)
        self.assertEqual(report["anchors"]["selected_offset_from_onsets_samples"], 0)
        self.assertEqual(report["anchors"]["selected_lag_from_relative_poryaaaa_window_samples"], 0)
        self.assertAlmostEqual(report["correlation"], 1.0)
        self.assertEqual(report["polarity"], "same")
        self.assertAlmostEqual(report["gain_fitted_residual_percent"], 0.0)
        self.assertAlmostEqual(report["raw_rms_db_poryaaaa_to_mgba"], -6.020599913, places=6)
        self.assertTrue(report["level_comparable"])
        self.assertTrue(report["passed"])

    def test_emits_one_json_report_per_pair_and_fails_bad_shape(self):
        """A mixed batch must retain every report and return the threshold failure status."""
        signal = noise_samples(640)
        good_pair = self.make_pair("good", [0] * 10 + signal, [0] * 20 + signal)
        bad_signal = [4000 if index % 32 < 8 else -4000 for index in range(640)]
        bad_pair = self.make_pair("bad", [0] * 10 + signal, [0] * 20 + bad_signal)

        completed = self.run_report(good_pair, bad_pair)

        self.assertEqual(completed.returncode, 1, completed.stderr)
        reports = [json.loads(line) for line in completed.stdout.splitlines()]
        self.assertEqual([report["song"] for report in reports], ["good", "bad"])
        self.assertTrue(reports[0]["passed"])
        self.assertFalse(reports[1]["passed"])
        self.assertFalse(reports[1]["level_comparable"])
        self.assertIsNone(reports[1]["raw_rms_db_poryaaaa_to_mgba"])
        self.assertTrue(reports[1]["failures"])

    def test_rejects_polarity_inversion_as_a_different_wave(self):
        """A negative gain fit must not authorize a waveform or level pass."""
        signal = noise_samples(640)
        pair = self.make_pair("inverted", [0] * 10 + signal, [0] * 20 + [-sample for sample in signal])

        completed = self.run_report(pair)

        self.assertEqual(completed.returncode, 1, completed.stderr)
        report = json.loads(completed.stdout)
        self.assertFalse(report["passed"])
        self.assertFalse(report["level_comparable"])
        self.assertIsNone(report["raw_rms_db_poryaaaa_to_mgba"])

    def test_scans_past_onset_transient_for_same_wave_window(self):
        """A shared stable region must win over different audible onset transients."""
        stable = noise_samples(768)
        reference = [4000, -4000] * 256 + stable
        candidate = [4000] * 256 + [-4000] * 256 + stable
        pair = self.make_pair("transient_then_stable", reference, candidate)

        completed = self.run_report(pair)

        self.assertEqual(completed.returncode, 0, completed.stderr)
        report = json.loads(completed.stdout)
        self.assertEqual(report["anchors"]["selected_offset_from_onsets_samples"], 512)
        self.assertAlmostEqual(report["correlation"], 1.0)
        self.assertTrue(report["level_comparable"])

    def test_does_not_select_an_inaudible_candidate_window(self):
        """A shape fit below the activity floor must not support a level claim."""
        stable = noise_samples(768)
        reference = [4000, -4000] * 256 + stable
        candidate = [4000] * 256 + [-4000] * 256 + [sample // 100 for sample in stable]
        pair = self.make_pair("quiet_candidate", reference, candidate)

        completed = self.run_report(pair, extra_args=("--scan-span", "512"))

        self.assertEqual(completed.returncode, 1, completed.stderr)
        report = json.loads(completed.stdout)
        self.assertNotEqual(report["anchors"]["selected_offset_from_onsets_samples"], 512)
        self.assertFalse(report["level_comparable"])
        self.assertIsNone(report["raw_rms_db_poryaaaa_to_mgba"])

    def test_rejects_pair_without_audible_activity(self):
        """An all-quiet reference must fail before a misleading comparison is emitted."""
        pair = self.make_pair("silent", [0] * 1024, [0] * 1024)

        completed = self.run_report(pair)

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(completed.stdout, "")
        self.assertIn("no sample reaches the 64 PCM16 onset threshold", completed.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
