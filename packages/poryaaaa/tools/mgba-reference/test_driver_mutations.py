#!/usr/bin/env python3
"""Phase 4 copied-trace mutation checks for the generic driver comparator."""

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


TOOL = Path(__file__).with_name("driver_compare.py")
CANONICAL_TRACE = """\
PORYAAAA_AUDIO_TRACE 1
CLOCK 16777216
BEGIN 0 0
WRITE 64 0 2 0x04000070 0x00000040
WRITE 128 0 4 0x04000090 0x03020100
WRITE 192 0 4 0x04000094 0x07060504
WRITE 256 0 4 0x04000098 0x0B0A0908
WRITE 320 0 4 0x0400009C 0x0F0E0D0C
SAMPLE 384 0
WRITE 448 0 2 0x04000070 0x00000000
WRITE 512 0 1 0x04000072 0x000000FF
WRITE 576 0 1 0x04000074 0x00000034
WRITE 640 0 1 0x04000075 0x00000087
WRITE 704 0 2 0x04000080 0x000044FF
WRITE 768 0 1 0x04000073 0x00000020
WRITE 832 0 2 0x04000070 0x00000080
WRITE 896 0 1 0x04000075 0x00000087
SAMPLE 960 0
END 1024 0
"""


class DriverMutationSensitivityTest(unittest.TestCase):
    """Prove every copied PSW mutation is detected by the generic public CLI."""

    def run_mutation(self, name, before, after):
        """Copy the canonical candidate and apply one literal trace mutation."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            directory = Path(temporary_directory)
            reference = directory / "oracle.trace"
            candidate = directory / f"candidate-{name}.trace"
            result_path = directory / f"{name}.json"
            reference.write_text(CANONICAL_TRACE, encoding="utf-8")
            shutil.copyfile(reference, candidate)

            candidate_text = candidate.read_text(encoding="utf-8")
            self.assertEqual(candidate_text.count(before), 1, name)
            candidate.write_text(candidate_text.replace(before, after, 1), encoding="utf-8")

            completed = subprocess.run(
                [sys.executable, str(TOOL), str(reference), str(candidate), "--family", "psw", "--output", str(result_path)],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertTrue(result_path.exists(), completed.stderr)
            self.assertEqual(result_path.read_text(encoding="utf-8"), completed.stdout)
            return completed, json.loads(completed.stdout)

    def assert_gates(self, result, transaction, payload, state):
        """Assert all required gates rather than allowing final state to excuse a fault."""
        self.assertIs(result["transaction_exact"], transaction)
        self.assertIs(result["payload_exact"], payload)
        self.assertIs(result["logical_state_exact"], state)

    def assert_write(self, record, line, cycle, width, address, value, register):
        """Require the complete source record reported by a diagnostic."""
        self.assertEqual(
            record,
            {
                "line": line,
                "cycle": cycle,
                "order": 0,
                "kind": "WRITE",
                "register": register,
                "width": width,
                "address": address,
                "value": value,
            },
        )

    def assert_transaction_cause(
        self,
        result,
        ordinal,
        reference,
        candidate,
    ):
        """Assert the transaction gate names the exact pair causing its failure."""
        divergence = result["first_divergence"]
        self.assertEqual(divergence["kind"], "transaction")
        self.assertEqual(divergence["ordinal"], ordinal)
        if reference is None:
            self.assertIsNone(divergence["reference"])
        else:
            self.assert_write(divergence["reference"], *reference)
        if candidate is None:
            self.assertIsNone(divergence["candidate"])
        else:
            self.assert_write(divergence["candidate"], *candidate)

    def assert_state_cause(self, result, ordinal, field, reference, candidate):
        """Assert the logical-state gate preserves its exact causal input records."""
        divergence = result["logical_state_divergence"]
        self.assertEqual(divergence["ordinal"], ordinal)
        self.assertEqual(divergence["field"], field)
        causal_record = divergence["causal_record"]
        if reference is None:
            self.assertIsNone(causal_record["reference"])
        else:
            self.assert_write(causal_record["reference"], *reference)
        if candidate is None:
            self.assertIsNone(causal_record["candidate"])
        else:
            self.assert_write(causal_record["candidate"], *candidate)

    def test_remove_nr30_zero(self):
        """Removing bank selection fails transaction and ordinal state replay."""
        completed, result = self.run_mutation(
            "remove-nr30-zero",
            "WRITE 448 0 2 0x04000070 0x00000000\n",
            "",
        )

        self.assertEqual(completed.returncode, 1)
        self.assert_gates(result, transaction=False, payload=True, state=False)
        self.assert_transaction_cause(
            result,
            5,
            (10, 448, 2, "0x04000070", "0x00000000", "NR30"),
            (10, 512, 1, "0x04000072", "0x000000FF", "NR31"),
        )
        self.assert_state_cause(
            result,
            5,
            "nr30",
            (10, 448, 2, "0x04000070", "0x00000000", "NR30"),
            (10, 512, 1, "0x04000072", "0x000000FF", "NR31"),
        )

    def test_move_nr32_before_nr51(self):
        """Reordering volume before routing fails at the moved record."""
        completed, result = self.run_mutation(
            "nr32-before-nr51",
            "WRITE 704 0 2 0x04000080 0x000044FF\n"
            "WRITE 768 0 1 0x04000073 0x00000020\n",
            "WRITE 704 0 1 0x04000073 0x00000020\n"
            "WRITE 768 0 2 0x04000080 0x000044FF\n",
        )

        self.assertEqual(completed.returncode, 1)
        self.assert_gates(result, transaction=False, payload=True, state=False)
        self.assert_transaction_cause(
            result,
            9,
            (14, 704, 2, "0x04000080", "0x000044FF", "NR50/NR51"),
            (14, 704, 1, "0x04000073", "0x00000020", "NR32"),
        )
        self.assert_state_cause(
            result,
            9,
            "nr32",
            (14, 704, 2, "0x04000080", "0x000044FF", "NR50/NR51"),
            (14, 704, 1, "0x04000073", "0x00000020", "NR32"),
        )

    def test_replace_wave_words_with_bytes(self):
        """Equal wave bytes cannot excuse the wrong four-word transaction shape."""
        completed, result = self.run_mutation(
            "wave-words-to-bytes",
            "WRITE 128 0 4 0x04000090 0x03020100\n"
            "WRITE 192 0 4 0x04000094 0x07060504\n"
            "WRITE 256 0 4 0x04000098 0x0B0A0908\n"
            "WRITE 320 0 4 0x0400009C 0x0F0E0D0C\n",
            "WRITE 128 0 1 0x04000090 0x00000000\n"
            "WRITE 129 0 1 0x04000091 0x00000001\n"
            "WRITE 130 0 1 0x04000092 0x00000002\n"
            "WRITE 131 0 1 0x04000093 0x00000003\n"
            "WRITE 192 0 1 0x04000094 0x00000004\n"
            "WRITE 193 0 1 0x04000095 0x00000005\n"
            "WRITE 194 0 1 0x04000096 0x00000006\n"
            "WRITE 195 0 1 0x04000097 0x00000007\n"
            "WRITE 256 0 1 0x04000098 0x00000008\n"
            "WRITE 257 0 1 0x04000099 0x00000009\n"
            "WRITE 258 0 1 0x0400009A 0x0000000A\n"
            "WRITE 259 0 1 0x0400009B 0x0000000B\n"
            "WRITE 320 0 1 0x0400009C 0x0000000C\n"
            "WRITE 321 0 1 0x0400009D 0x0000000D\n"
            "WRITE 322 0 1 0x0400009E 0x0000000E\n"
            "WRITE 323 0 1 0x0400009F 0x0000000F\n",
        )

        self.assertEqual(completed.returncode, 1)
        self.assert_gates(result, transaction=False, payload=True, state=False)
        self.assert_transaction_cause(
            result,
            1,
            (5, 128, 4, "0x04000090", "0x03020100", "WAVE_RAM"),
            (5, 128, 1, "0x04000090", "0x00000000", "WAVE_RAM"),
        )
        self.assert_state_cause(
            result,
            1,
            "wave_bank_0",
            (5, 128, 4, "0x04000090", "0x03020100", "WAVE_RAM"),
            (5, 128, 1, "0x04000090", "0x00000000", "WAVE_RAM"),
        )
        self.assertEqual(result["payload"]["first_differing_byte"], None)

    def test_swap_waveform_nibbles(self):
        """Nibble reversal fails both transaction and independently expanded payload."""
        completed, result = self.run_mutation(
            "swap-wave-nibbles",
            "WRITE 128 0 4 0x04000090 0x03020100\n"
            "WRITE 192 0 4 0x04000094 0x07060504\n"
            "WRITE 256 0 4 0x04000098 0x0B0A0908\n"
            "WRITE 320 0 4 0x0400009C 0x0F0E0D0C\n",
            "WRITE 128 0 4 0x04000090 0x30201000\n"
            "WRITE 192 0 4 0x04000094 0x70605040\n"
            "WRITE 256 0 4 0x04000098 0xB0A09080\n"
            "WRITE 320 0 4 0x0400009C 0xF0E0D0C0\n",
        )

        self.assertEqual(completed.returncode, 1)
        self.assert_gates(result, transaction=False, payload=False, state=False)
        self.assert_transaction_cause(
            result,
            1,
            (5, 128, 4, "0x04000090", "0x03020100", "WAVE_RAM"),
            (5, 128, 4, "0x04000090", "0x30201000", "WAVE_RAM"),
        )
        self.assert_state_cause(
            result,
            1,
            "wave_bank_0",
            (5, 128, 4, "0x04000090", "0x03020100", "WAVE_RAM"),
            (5, 128, 4, "0x04000090", "0x30201000", "WAVE_RAM"),
        )
        self.assertEqual(result["payload"]["first_differing_byte"], 1)
        self.assertEqual(result["payload"]["first_differing_nibble"], 2)

    def test_remove_second_triggered_nr34(self):
        """Removing the second trigger fails the terminal ordinal and state count."""
        completed, result = self.run_mutation(
            "remove-second-nr34",
            "WRITE 896 0 1 0x04000075 0x00000087\n",
            "",
        )

        self.assertEqual(completed.returncode, 1)
        self.assert_gates(result, transaction=False, payload=True, state=False)
        self.assert_transaction_cause(
            result,
            12,
            (17, 896, 1, "0x04000075", "0x00000087", "NR34"),
            None,
        )
        self.assert_state_cause(
            result,
            12,
            "record_count",
            (17, 896, 1, "0x04000075", "0x00000087", "NR34"),
            None,
        )

    def test_add_pitch_only_nr34(self):
        """An added non-triggering NR34 pitch write cannot be masked by the trigger."""
        completed, result = self.run_mutation(
            "add-pitch-only-nr34",
            "WRITE 832 0 2 0x04000070 0x00000080\n",
            "WRITE 832 0 2 0x04000070 0x00000080\n"
            "WRITE 864 0 1 0x04000075 0x00000007\n",
        )

        self.assertEqual(completed.returncode, 1)
        self.assert_gates(result, transaction=False, payload=True, state=False)
        self.assert_transaction_cause(
            result,
            12,
            (17, 896, 1, "0x04000075", "0x00000087", "NR34"),
            (17, 864, 1, "0x04000075", "0x00000007", "NR34"),
        )
        self.assert_state_cause(
            result,
            12,
            "nr34",
            (17, 896, 1, "0x04000075", "0x00000087", "NR34"),
            (17, 864, 1, "0x04000075", "0x00000007", "NR34"),
        )

    def test_move_write_across_sample(self):
        """A sample crossing is diagnostic only, but identifies the moved write exactly."""
        completed, result = self.run_mutation(
            "write-across-sample",
            "SAMPLE 384 0\nWRITE 448 0 2 0x04000070 0x00000000\n",
            "WRITE 352 0 2 0x04000070 0x00000000\nSAMPLE 384 0\n",
        )

        self.assertEqual(completed.returncode, 0)
        self.assert_gates(result, transaction=True, payload=True, state=True)
        self.assertFalse(result["cycle_exact"])
        self.assertIsNone(result["first_divergence"])
        self.assertIsNone(result["logical_state_divergence"])
        self.assertEqual(
            result["timing"]["first_cycle_mismatch"],
            {"ordinal": 5, "reference_cycle": 448, "candidate_cycle": 352},
        )
        crossings = result["timing"]["sample_boundary_crossings"]
        self.assertEqual([crossing["ordinal"] for crossing in crossings], [5, 6])
        self.assertTrue(crossings[0]["reference_crosses_sample"])
        self.assertFalse(crossings[0]["candidate_crosses_sample"])
        self.assertEqual(crossings[0]["reference_sample_cycle"], 384)
        self.assertIsNone(crossings[0]["candidate_sample_cycle"])
        self.assert_write(
            crossings[0]["reference"],
            10,
            448,
            2,
            "0x04000070",
            "0x00000000",
            "NR30",
        )
        self.assert_write(
            crossings[0]["candidate"],
            9,
            352,
            2,
            "0x04000070",
            "0x00000000",
            "NR30",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
