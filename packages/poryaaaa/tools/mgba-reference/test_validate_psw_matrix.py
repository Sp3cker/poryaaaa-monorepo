#!/usr/bin/env python3
"""Behavioral tests for the fixed programmable-wave lifecycle matrix."""

import json
import os
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path


TOOL = Path(__file__).with_name("validate_psw_matrix.py")

MOCK_VALIDATOR = r'''
import json
import os
import sys
from pathlib import Path


args = sys.argv[1:]


def option(flag):
    return args[args.index(flag) + 1]


output = Path(option("--output-dir"))
scenario = option("--scenario")
voicegroup = option("--voicegroup")
voice = int(option("--voice"))
case_id = f"{scenario}-{'normal' if voicegroup == 'voicegroup_aa_girl' else 'alternate'}"
run = output.name
record = {
    "case": case_id,
    "scenario": scenario,
    "voicegroup": voicegroup,
    "voice": voice,
    "note": int(option("--note")),
    "pan": int(option("--pan")),
    "run": run,
}
with Path(os.environ["MATRIX_LOG"]).open("a", encoding="utf-8") as log:
    log.write(json.dumps(record, sort_keys=True) + "\n")

failed = os.environ.get("MATRIX_FAIL_CASE") == case_id
infrastructure = os.environ.get("MATRIX_INFRA_CASE") == case_id
if failed:
    output = output.with_name(output.name + ".failed")
output.mkdir(parents=True)

normal_wave_words = ("0x76543210", "0xfedcba98", "0x67452301", "0xefcdab89")
alternate_wave_words = ("0x10325476", "0x98badcfe", "0x01234567", "0x89abcdef")
fixture_kind = "normal" if voicegroup == "voicegroup_aa_girl" else "alternate"
wave_words = normal_wave_words if fixture_kind == "normal" else alternate_wave_words
if os.environ.get("MATRIX_IDENTICAL_WAVES") == "1":
    wave_words = normal_wave_words
if os.environ.get("MATRIX_SYMMETRIC_WAVE_FIXTURE") == fixture_kind:
    wave_words = ("0x33221100", "0x77665544", "0x44556677", "0x00112233")
nr32_line = "WRITE 4 0 1 0x04000073 0x00"
late_nr32_lines = []
end_line = "END 13 0"
if case_id == "envelope-alternate":
    nr32_line = "WRITE 4 0 1 0x04000073 0x20"
    if os.environ.get("MATRIX_DROP_NR32_PROGRESSION") != case_id:
        late_nr32_lines.append("WRITE 13 0 1 0x04000073 0x80")
        end_line = "END 14 0"
trace_lines = [
    "PORYAAAA_AUDIO_TRACE 1",
    "CLOCK 16777216",
    "BEGIN 0 0",
    "WRITE 1 0 1 0x04000070 0x00",
    "WRITE 2 0 1 0x04000071 0x20",
    "WRITE 3 0 1 0x04000072 0x60",
    nr32_line,
    "WRITE 5 0 1 0x04000074 0x80",
    "WRITE 6 0 1 0x04000075 0xc0",
    "WRITE 7 0 1 0x04000080 0x77",
    "WRITE 8 0 1 0x04000081 0xff",
    f"WRITE 9 0 4 0x04000090 {wave_words[0]}",
    f"WRITE 10 0 4 0x04000094 {wave_words[1]}",
    f"WRITE 11 0 4 0x04000098 {wave_words[2]}",
    f"WRITE 12 0 4 0x0400009c {wave_words[3]}",
    *late_nr32_lines,
    end_line,
]
coverage_addresses = {
    "NR30": ("0x04000070", "0x04000071"),
    "NR31": ("0x04000072",),
    "NR32": ("0x04000073",),
    "NR33": ("0x04000074",),
    "NR34": ("0x04000075",),
    "NR50": ("0x04000080",),
    "NR51": ("0x04000081",),
}
dropped_addresses = coverage_addresses.get(os.environ.get("MATRIX_DROP_COVERAGE"), ())
if dropped_addresses:
    trace_lines = [line for line in trace_lines if not any(address in line for address in dropped_addresses)]
trace = "\n".join(trace_lines) + "\n"
if os.environ.get("MATRIX_REPEAT_MISMATCH") == case_id and run == "repeat":
    trace += "# deterministic-capture-mismatch\n"
for name in ("reference.trace", "candidate.trace"):
    (output / name).write_text(trace, encoding="utf-8")
if os.environ.get("MATRIX_TRUNCATED_GATE_CASE") == case_id:
    (output / "candidate.trace").unlink()
for name in ("reference-native.pcm", "reference-native.cycles", "reference-native.json", "candidate.trace.manifest.json"):
    (output / name).write_bytes(name.encode("ascii"))

gates = {
    "transaction_exact": not failed,
    "wave_payload_exact": True,
    "logical_state_exact": True,
    "reference_native_exact": True,
    "reference_hardware_exact": True,
    "candidate_hardware_exact": True,
}
if failed:
    (output / "psw-compare.json").write_text(json.dumps(gates), encoding="utf-8")
    for name in ("reference-native-compare.json", "reference-hardware-compare.json", "candidate-hardware-compare.json"):
        (output / name).write_text(json.dumps({"passed": True}), encoding="utf-8")
else:
    (output / "manifest.json").write_text(json.dumps({"gates": gates}), encoding="utf-8")
raise SystemExit(2 if infrastructure else (1 if failed else 0))
'''


class ValidatePswMatrixTest(unittest.TestCase):
    """Exercise the fixed matrix runner through its public command line."""

    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.directory = Path(self.temporary_directory.name)
        self.decomp = self.directory / "decomp"
        self.decomp.mkdir()
        self.outputs = self.directory / "outputs"
        self.outputs.mkdir()
        self.log = self.directory / "validator-log.jsonl"
        self.log.write_text("", encoding="utf-8")
        self.validator = self.directory / "validator.py"
        self.validator.write_text("#!/usr/bin/env python3\n" + textwrap.dedent(MOCK_VALIDATOR), encoding="utf-8")

    def tearDown(self):
        self.temporary_directory.cleanup()

    def run_matrix(self, output, **environment):
        command_environment = os.environ.copy()
        command_environment.update({"MATRIX_LOG": str(self.log), **environment})
        return subprocess.run(
            [
                sys.executable,
                str(TOOL),
                "--decomp",
                str(self.decomp),
                "--output-dir",
                str(output),
                "--validator",
                str(self.validator),
            ],
            check=False,
            capture_output=True,
            text=True,
            env=command_environment,
        )

    def calls(self):
        return [json.loads(line) for line in self.log.read_text(encoding="utf-8").splitlines()]

    def test_fixed_matrix_exercises_every_contract_and_publishes_complete_coverage(self):
        output = self.outputs / "matrix"
        completed = self.run_matrix(output)

        self.assertEqual(completed.returncode, 0, completed.stderr or completed.stdout)
        report = json.loads((output / "matrix_report.json").read_text(encoding="utf-8"))
        self.assertTrue(report["matrix_exact"])
        self.assertEqual((report["case_count"], report["passed_case_count"], report["failed_case_count"]), (12, 12, 0))
        self.assertEqual([case["scenario"] for case in report["cases"][:6]], ["start", "envelope", "pitch", "volume-pan", "retrigger", "release"])
        self.assertEqual([case["fixture"]["kind"] for case in report["cases"][:6]], ["normal"] * 6)
        self.assertEqual([case["fixture"]["kind"] for case in report["cases"][6:]], ["alternate"] * 6)
        self.assertEqual({case["controls"]["note"] for case in report["cases"]}, {36, 60, 84})
        self.assertEqual({case["controls"]["pan"] for case in report["cases"]}, {0, 64, 127})
        self.assertTrue(all(case["fixture"]["voice_index"] == 4 for case in report["cases"][:6]))
        self.assertTrue(all(case["fixture"]["voice_index"] == 1 for case in report["cases"][6:]))
        self.assertTrue(all(case["fixture"]["voicegroup_symbol"] == "voicegroup_aa_girl" for case in report["cases"][:6]))
        self.assertTrue(all(case["fixture"]["voicegroup_symbol"] == "voicegroup_poke_center" for case in report["cases"][6:]))
        self.assertTrue(all(case["deterministic"] for case in report["cases"]))
        self.assertTrue(all(gate["passed"] for case in report["cases"] for gate in case["gates"].values()))
        for source in ("reference", "candidate"):
            coverage = report["coverage"]["sources"][source]
            self.assertTrue(coverage["complete"])
            self.assertEqual(len(coverage["retained_register_coverage"]), 7)
            self.assertTrue(
                all(
                    any(case_run.endswith("/first") for case_run in item["case_runs"])
                    and any(case_run.endswith("/repeat") for case_run in item["case_runs"])
                    for item in coverage["retained_register_coverage"] + coverage["wave_byte_coverage"]
                )
            )
            self.assertEqual(len(coverage["wave_byte_coverage"]), 16)
            self.assertTrue(all(item["exercised"] for item in coverage["retained_register_coverage"] + coverage["wave_byte_coverage"]))
            nr32 = coverage["alternate_envelope_nr32"]
            self.assertTrue(nr32["complete"])
            self.assertEqual(
                [item["values"] for item in nr32["case_runs"]],
                [["0x20", "0x80"], ["0x20", "0x80"]],
            )
            self.assertTrue(all(item["transition_20_to_80"] for item in nr32["case_runs"]))
            payloads = coverage["fixture_wave_payloads"]
            self.assertTrue(coverage["fixture_wave_payloads_distinct"])
            self.assertNotEqual(payloads["normal"]["payload_hex"], payloads["alternate"]["payload_hex"])
            for payload in payloads.values():
                self.assertEqual(len(payload["case_runs"]), 12)
                self.assertTrue(payload["complete"])
                self.assertTrue(payload["byte_order_non_symmetric"])
                self.assertTrue(payload["nibble_non_symmetric"])
                self.assertTrue(payload["non_symmetric"])
        self.assertTrue(report["coverage"]["complete"])
        self.assertEqual(len(self.calls()), 24)

    def test_unexercised_retained_register_fails_the_aggregate_coverage_gate(self):
        output = self.outputs / "incomplete-coverage"
        completed = self.run_matrix(output, MATRIX_DROP_COVERAGE="NR30")

        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        failed = output.with_name(output.name + ".failed")
        report = json.loads((failed / "matrix_report.json").read_text(encoding="utf-8"))
        self.assertFalse(report["matrix_exact"])
        for source in ("reference", "candidate"):
            coverage = report["coverage"]["sources"][source]
            nr30 = next(item for item in coverage["retained_register_coverage"] if item["name"] == "NR30")
            self.assertFalse(coverage["complete"])
            self.assertFalse(nr30["exercised"])
            self.assertEqual(nr30["case_runs"], [])
        self.assertEqual(len(self.calls()), 24)

    def test_missing_alternate_envelope_nr32_progression_fails_aggregate_evidence(self):
        output = self.outputs / "missing-nr32-progression"
        completed = self.run_matrix(output, MATRIX_DROP_NR32_PROGRESSION="envelope-alternate")

        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        report = json.loads((output.with_name(output.name + ".failed") / "matrix_report.json").read_text(encoding="utf-8"))
        self.assertFalse(report["matrix_exact"])
        for source in ("reference", "candidate"):
            nr32 = report["coverage"]["sources"][source]["alternate_envelope_nr32"]
            self.assertFalse(nr32["complete"])
            self.assertEqual([item["values"] for item in nr32["case_runs"]], [["0x20"], ["0x20"]])
            self.assertTrue(all(not item["transition_20_to_80"] for item in nr32["case_runs"]))
        self.assertEqual(len(self.calls()), 24)

    def test_symmetric_fixture_waveform_fails_aggregate_evidence(self):
        output = self.outputs / "symmetric-waveform"
        completed = self.run_matrix(output, MATRIX_SYMMETRIC_WAVE_FIXTURE="alternate")

        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        report = json.loads((output.with_name(output.name + ".failed") / "matrix_report.json").read_text(encoding="utf-8"))
        self.assertFalse(report["matrix_exact"])
        for source in ("reference", "candidate"):
            alternate = report["coverage"]["sources"][source]["fixture_wave_payloads"]["alternate"]
            self.assertTrue(alternate["complete"])
            self.assertFalse(alternate["byte_order_non_symmetric"])
            self.assertFalse(alternate["non_symmetric"])
        self.assertEqual(len(self.calls()), 24)

    def test_repeat_hash_mismatch_and_gate_failure_remain_explicit_and_do_not_abort_matrix(self):
        output = self.outputs / "failed"
        completed = self.run_matrix(output, MATRIX_REPEAT_MISMATCH="start-normal", MATRIX_FAIL_CASE="pitch-alternate")

        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        failed = output.with_name(output.name + ".failed")
        self.assertFalse(output.exists())
        report = json.loads((failed / "matrix_report.json").read_text(encoding="utf-8"))
        by_id = {case["id"]: case for case in report["cases"]}
        self.assertFalse(report["matrix_exact"])
        self.assertFalse(by_id["start-normal"]["deterministic"])
        self.assertNotEqual(
            by_id["start-normal"]["runs"]["first"]["capture_hashes"]["reference.trace"],
            by_id["start-normal"]["runs"]["repeat"]["capture_hashes"]["reference.trace"],
        )
        self.assertFalse(by_id["start-normal"]["passed"])
        self.assertFalse(by_id["pitch-alternate"]["gates"]["transaction_exact"]["passed"])
        self.assertFalse(by_id["pitch-alternate"]["passed"])
        self.assertTrue((failed / "cases" / "pitch-alternate" / "first.failed" / "psw-compare.json").is_file())
        self.assertEqual(len(self.calls()), 24)

    def test_infrastructure_failure_returns_two_after_recording_every_case(self):
        output = self.outputs / "infrastructure"
        completed = self.run_matrix(output, MATRIX_INFRA_CASE="release-normal")

        self.assertEqual(completed.returncode, 2, completed.stderr or completed.stdout)
        failed = output.with_name(output.name + ".failed")
        report = json.loads((failed / "matrix_report.json").read_text(encoding="utf-8"))
        by_id = {case["id"]: case for case in report["cases"]}
        self.assertEqual(report["case_count"], 12)
        self.assertFalse(by_id["release-normal"]["passed"])
        self.assertEqual(by_id["release-normal"]["runs"]["first"]["returncode"], 2)
        self.assertEqual(len(self.calls()), 24)

    def test_truncated_gate_diagnostics_are_infrastructure_failures(self):
        output = self.outputs / "truncated"
        completed = self.run_matrix(
            output,
            MATRIX_FAIL_CASE="pitch-normal",
            MATRIX_TRUNCATED_GATE_CASE="pitch-normal",
        )

        self.assertEqual(completed.returncode, 2, completed.stderr or completed.stdout)
        failed = output.with_name(output.name + ".failed")
        report = json.loads((failed / "matrix_report.json").read_text(encoding="utf-8"))
        by_id = {case["id"]: case for case in report["cases"]}
        self.assertIn("missing capture artifact candidate.trace", by_id["pitch-normal"]["runs"]["first"]["errors"])
        self.assertEqual(len(self.calls()), 24)

    def test_equivalent_output_parents_produce_byte_identical_aggregate_reports(self):
        first_parent = self.directory / "first"
        second_parent = self.directory / "second"
        first_parent.mkdir()
        second_parent.mkdir()
        first = first_parent / "matrix"
        second = second_parent / "matrix"

        first_completed = self.run_matrix(first)
        second_completed = self.run_matrix(second)

        self.assertEqual(first_completed.returncode, 0, first_completed.stderr or first_completed.stdout)
        self.assertEqual(second_completed.returncode, 0, second_completed.stderr or second_completed.stdout)
        self.assertEqual((first / "matrix_report.json").read_bytes(), (second / "matrix_report.json").read_bytes())


if __name__ == "__main__":
    unittest.main()
