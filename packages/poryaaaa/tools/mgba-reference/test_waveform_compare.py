#!/usr/bin/env python3
"""Exercise waveform_compare.py with deterministic PCM16 fixtures."""

import json
import subprocess
import sys
import tempfile
import unittest
import wave
from pathlib import Path


TOOL = Path(__file__).with_name("waveform_compare.py")
SAMPLE_RATE = 65536
START = 128
LENGTH = 2048


def write_wav(path, samples, channels=1, sample_rate=SAMPLE_RATE):
    """Write a small PCM16 fixture without adding test dependencies."""
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(channels)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        frames = bytearray()
        for sample in samples:
            channel_samples = sample if isinstance(sample, tuple) else (sample,) * channels
            for channel_sample in channel_samples:
                frames.extend(int(channel_sample).to_bytes(2, "little", signed=True))
        wav.writeframes(frames)


def square_sample(index):
    """Provide a stable asymmetric square period for lag tests."""
    return 4000 if index % 32 < 10 else -4000


def noise_samples(count):
    """Generate a deterministic 15-bit LFSR sequence for noise comparisons."""
    state = 0x5A5A
    samples = []
    for _ in range(count):
        samples.append(2000 if state & 1 else -2000)
        feedback = (state ^ (state >> 1)) & 1
        state = (state >> 1) | (feedback << 14)
    return samples


class WaveformCompareTest(unittest.TestCase):
    """Prove the CLI's waveform gate and reported gain against known signals."""

    def setUp(self):
        """Create an isolated fixture directory for each test."""
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.directory = Path(self.temporary_directory.name)

    def tearDown(self):
        """Remove generated WAV fixtures after each test."""
        self.temporary_directory.cleanup()

    def run_compare(self, reference, candidate, max_lag=0, candidate_start=None):
        """Invoke the public CLI so exit status and JSON are tested together."""
        command = [
            sys.executable,
            str(TOOL),
            str(reference),
            str(candidate),
            "--reference-start",
            str(START),
            "--reference-length",
            str(LENGTH),
            "--max-lag",
            str(max_lag),
            "--min-correlation",
            "0.999",
            "--max-residual-percent",
            "0.1",
        ]
        if candidate_start is not None:
            command.extend(["--candidate-start", str(candidate_start)])
        return subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
        )

    def test_identical_stereo_square_passes(self):
        """An identical stereo-folded square wave must pass with unity gain."""
        samples = [square_sample(index) for index in range(START + LENGTH + 128)]
        reference = self.directory / "reference.wav"
        candidate = self.directory / "candidate.wav"
        write_wav(reference, samples, channels=1)
        stereo_samples = [(sample + 1000, sample - 1000) for sample in samples]
        write_wav(candidate, stereo_samples, channels=2)

        completed = self.run_compare(reference, candidate)

        self.assertEqual(completed.returncode, 0, completed.stderr or completed.stdout)
        result = json.loads(completed.stdout)
        self.assertTrue(result["passed"])
        self.assertEqual(result["reference_channels"], 1)
        self.assertEqual(result["candidate_channels"], 2)
        self.assertAlmostEqual(result["shape"]["gain_candidate_from_reference"], 1.0)
        self.assertEqual(result["shape"]["polarity"], "same")

    def test_sample_rate_mismatch_is_rejected(self):
        """Different sample clocks must stop before a misleading comparison."""
        samples = [square_sample(index) for index in range(START + LENGTH + 128)]
        reference = self.directory / "reference.wav"
        candidate = self.directory / "candidate.wav"
        write_wav(reference, samples)
        write_wav(candidate, samples, sample_rate=44100)

        completed = self.run_compare(reference, candidate)

        self.assertEqual(completed.returncode, 2)
        self.assertIn("sample-rate mismatch", completed.stderr)

    def test_delayed_inverted_scaled_square_fails_polarity_gate(self):
        """An inverted waveform must not count as the same captured wave."""
        lag = 7
        count = START + LENGTH + 128
        reference_samples = [square_sample(index) for index in range(count)]
        candidate_samples = [
            int(-1.5 * square_sample(index - lag)) for index in range(count)
        ]
        reference = self.directory / "reference.wav"
        candidate = self.directory / "candidate.wav"
        write_wav(reference, reference_samples)
        write_wav(candidate, candidate_samples)

        completed = self.run_compare(reference, candidate, max_lag=10)

        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        result = json.loads(completed.stdout)
        self.assertFalse(result["passed"])
        self.assertLess(result["shape"]["correlation"], 0.999)
        self.assertAlmostEqual(result["level"]["raw_rms_ratio_candidate_to_reference"], 1.5)

    def test_delayed_scaled_noise_passes_and_reports_gain(self):
        """A deterministic noise sequence must survive lag and gain fitting."""
        lag = -5
        count = START + LENGTH + 128
        reference_samples = noise_samples(count + 16)
        candidate_samples = [
            reference_samples[index - lag] // 2
            for index in range(count)
        ]
        reference = self.directory / "reference.wav"
        candidate = self.directory / "candidate.wav"
        write_wav(reference, reference_samples[:count])
        write_wav(candidate, candidate_samples)

        completed = self.run_compare(reference, candidate, max_lag=10)

        self.assertEqual(completed.returncode, 0, completed.stderr or completed.stdout)
        result = json.loads(completed.stdout)
        self.assertEqual(result["best_lag_samples"], lag)
        self.assertAlmostEqual(result["shape"]["gain_candidate_from_reference"], 0.5)
        self.assertAlmostEqual(result["level"]["raw_rms_ratio_candidate_to_reference"], 0.5)

    def test_candidate_anchor_limits_lag_search_to_matching_onset(self):
        """An explicit candidate onset must anchor a local lag search."""
        candidate_start = START + 64
        lag = 3
        count = candidate_start + LENGTH + 128
        reference_samples = noise_samples(count)
        candidate_samples = [0] * count
        for index in range(LENGTH):
            candidate_samples[candidate_start + lag + index] = reference_samples[START + index]
        reference = self.directory / "reference.wav"
        candidate = self.directory / "candidate.wav"
        write_wav(reference, reference_samples)
        write_wav(candidate, candidate_samples)

        completed = self.run_compare(reference, candidate, max_lag=5, candidate_start=candidate_start)

        self.assertEqual(completed.returncode, 0, completed.stderr or completed.stdout)
        result = json.loads(completed.stdout)
        self.assertEqual(result["candidate_search_anchor_sample"], candidate_start)
        self.assertEqual(result["candidate_start_sample"], candidate_start + lag)
        self.assertEqual(result["best_lag_samples"], candidate_start + lag - START)

    def test_wrong_noise_wave_fails_shape_gate(self):
        """Unrelated square and deterministic noise waves must fail automation."""
        count = START + LENGTH + 128
        reference_samples = [square_sample(index) for index in range(count)]
        candidate_samples = noise_samples(count)
        reference = self.directory / "reference.wav"
        candidate = self.directory / "candidate.wav"
        write_wav(reference, reference_samples)
        write_wav(candidate, candidate_samples)

        completed = self.run_compare(reference, candidate, max_lag=10)

        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        result = json.loads(completed.stdout)
        self.assertFalse(result["passed"])
        self.assertLess(result["shape"]["abs_correlation"], 0.999)
        self.assertTrue(result["failures"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
