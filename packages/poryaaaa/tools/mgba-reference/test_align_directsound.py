#!/usr/bin/env python3
"""Exercise DirectSound alignment with deterministic delayed PCM fixtures."""

import json
import math
import struct
import subprocess
import tempfile
import wave
from pathlib import Path


TOOL = Path(__file__).with_name("align_directsound.py")
RATE = 65536


def write_wav(path, mono):
    """Write one stereo PCM16 fixture with identical sides."""
    frames = []
    for sample in mono:
        value = max(-32768, min(32767, round(sample)))
        frames.extend((value, value))
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(2)
        wav.setsampwidth(2)
        wav.setframerate(RATE)
        wav.writeframes(struct.pack(f"<{len(frames)}h", *frames))


def run_tool(reference, candidate, output, *extra):
    """Run the CLI exactly as capture automation does."""
    return subprocess.run(
        [
            "python3",
            str(TOOL),
            str(reference),
            str(candidate),
            "--output",
            str(output),
            "--window-length",
            "512",
            "--search-radius",
            "512",
            *extra,
        ],
        capture_output=True,
        text=True,
    )


def main():
    """Prove fixed lag passes while a gain error remains red-capable."""
    with tempfile.TemporaryDirectory(prefix="poryaaaa-align-test.") as temp:
        root = Path(temp)
        signal = [0.0] * 700
        signal.extend(
            9000.0 * math.sin(index * 0.071) + 3500.0 * math.sin(index * 0.019)
            for index in range(5000)
        )
        signal.extend([0.0] * 700)
        delayed = [0.0] * 173 + signal
        reference = root / "reference.wav"
        candidate = root / "candidate.wav"
        output = root / "comparison.json"
        write_wav(reference, signal)
        write_wav(candidate, delayed)

        passed = run_tool(reference, candidate, output)
        assert passed.returncode == 0, passed.stderr + passed.stdout
        result = json.loads(output.read_text())
        assert result["global_lag_samples"] == 173
        assert result["alignment_passed"] is True
        assert all(window["lag_samples"] == 173 for window in result["windows"].values())
        assert all(len(window["sides"]) == 2 for window in result["windows"].values())
        for artifact in ("aligned-mgba.wav", "aligned-poryaaaa.wav", "difference.wav"):
            assert (root / artifact).is_file()

        write_wav(candidate, [sample * 1.5 for sample in delayed])
        failed = run_tool(reference, candidate, output)
        assert failed.returncode == 1, failed.stderr + failed.stdout
        result = json.loads(output.read_text())
        assert result["alignment_passed"] is True
        assert any("level" in failure for failure in result["failures"])

    print("PASS: DirectSound alignment finds fixed lag and rejects level mismatch")


if __name__ == "__main__":
    main()
