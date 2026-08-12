#!/usr/bin/env python3
"""Behavioral tests for the fixed multi-family driver lifecycle matrix."""

import json
import os
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path


TOOL = Path(__file__).with_name("validate_driver_matrix.py")

MOCK_VALIDATOR = r'''
import json
import os
import sys
from pathlib import Path


args = sys.argv[1:]


def option(flag):
    return args[args.index(flag) + 1]


def write_trace(output, family):
    setup = []
    if family == "directsound":
        setup = [
            "WRITE 0 0 2 0x04000082 0x0000b600",
            "WRITE 0 1 2 0x04000088 0x00000200",
        ]
        body = [
            "WRITE 1 0 4 0x040000a0 0x76543210",
            "TIMER 2 0 0",
            "SAMPLE 3 0",
        ]
    elif family == "sq1":
        body = [
            "WRITE 1 0 2 0x04000060 0x0000",
            "WRITE 2 0 1 0x04000062 0x80",
            "WRITE 3 0 1 0x04000063 0xf3",
            "WRITE 4 0 1 0x04000064 0x44",
            "WRITE 5 0 1 0x04000065 0xc3",
            "WRITE 6 0 2 0x04000080 0xff77",
        ]
    elif family == "sq2":
        body = [
            "WRITE 1 0 1 0x04000068 0x80",
            "WRITE 2 0 1 0x04000069 0xf3",
            "WRITE 3 0 1 0x0400006c 0x44",
            "WRITE 4 0 1 0x0400006d 0xc3",
            "WRITE 5 0 2 0x04000080 0xff77",
        ]
    else:
        body = [
            "WRITE 1 0 2 0x04000070 0x2000",
            "WRITE 2 0 1 0x04000072 0x60",
            "WRITE 3 0 1 0x04000073 0x20",
            "WRITE 4 0 1 0x04000074 0x80",
            "WRITE 5 0 1 0x04000075 0xc0",
            "WRITE 6 0 2 0x04000080 0xff77",
            "WRITE 7 0 4 0x04000090 0x76543210",
            "WRITE 8 0 4 0x04000094 0xfedcba98",
            "WRITE 9 0 4 0x04000098 0x67452301",
            "WRITE 10 0 4 0x0400009c 0xefcdab89",
        ]
    if os.environ.get("MATRIX_DROP_EVENT") == f"{family}:timer_overflow":
        body = [line for line in body if not line.startswith("TIMER ")]
    return "\n".join(["PORYAAAA_AUDIO_TRACE 1", "CLOCK 16777216", *setup, "BEGIN 0 2", *body, "END 11 0", ""])


output = Path(option("--output-dir"))
voicegroup = option("--voicegroup")
voice = int(option("--voice"))
scenario = option("--scenario")
family = {
    "voicegroup_aa_girl": "psw",
    "voicegroup_poke_center": "psw",
    "voicegroup_rg_poke_center": "directsound",
    "voicegroup_vs_wild": "sq1" if voice == 1 else "sq2",
}[voicegroup]
case_id = f"{scenario}-{'normal' if voicegroup == 'voicegroup_aa_girl' else 'alternate' if voicegroup == 'voicegroup_poke_center' else family}"
run = output.name


def create_racing_destination():
    destination = os.environ.get("MATRIX_RACE_DESTINATION")
    if destination and case_id == "release-sq2" and run == "repeat":
        Path(destination).mkdir()

record = {
    "case": case_id,
    "family": family,
    "voicegroup": voicegroup,
    "voice": voice,
    "scenario": scenario,
    "note": int(option("--note")),
    "pan": int(option("--pan")),
    "run": run,
}
with Path(os.environ["MATRIX_LOG"]).open("a", encoding="utf-8") as log:
    log.write(json.dumps(record, sort_keys=True) + "\n")

failed = os.environ.get("MATRIX_FAIL_CASE") == case_id
if failed:
    output = output.with_name(output.name + ".failed")
output.mkdir(parents=True)
trace = write_trace(output, family)
for name in ("reference.trace", "candidate.trace"):
    (output / name).write_text(trace, encoding="utf-8")
for prefix in ("reference-native", "reference-mgba", "reference-pory", "candidate-mgba", "candidate-pory"):
    for suffix in ("pcm", "cycles", "json"):
        (output / f"{prefix}.{suffix}").write_bytes(f"{prefix}.{suffix}".encode("ascii"))
resolved_type = {"psw": 11 if voicegroup == "voicegroup_poke_center" else 3, "directsound": 0, "sq1": 1, "sq2": 2}[family]
hash_value = "a" * 64
candidate_payload = "b" * 64 if os.environ.get("MATRIX_IDENTITY_MISMATCH") == case_id else hash_value
reference_manifest = {
    "family": family,
    "resolved_type": resolved_type,
    "tone_data_sha256": hash_value,
    "family_payload_sha256": hash_value,
}
candidate_manifest = dict(reference_manifest, family_payload_sha256=candidate_payload)
(output / "reference-native.json").write_text(json.dumps(reference_manifest), encoding="utf-8")
(output / "candidate.trace.manifest.json").write_text(json.dumps(candidate_manifest), encoding="utf-8")

gates = {
    "transaction_exact": not failed,
    "payload_exact": True,
    "logical_state_exact": True,
    "reference_native_exact": True,
    "reference_hardware_exact": not failed,
    "candidate_hardware_exact": True,
}
if failed:
    (output / "driver-compare.json").write_text(json.dumps({
        "transaction_exact": False,
        "payload_exact": True,
        "logical_state_exact": True,
        "first_divergence": {"ordinal": 2, "kind": "transaction"},
    }), encoding="utf-8")
    (output / "reference-native-compare.json").write_text(json.dumps({"passed": True}), encoding="utf-8")
    (output / "reference-hardware-compare.json").write_text(json.dumps({
        "passed": False,
        "first_mismatch": {"frame_index": 7, "reference_causal_sample": {"ordinal": 3, "cycle": 3}},
    }), encoding="utf-8")
    (output / "candidate-hardware-compare.json").write_text(json.dumps({"passed": True}), encoding="utf-8")
else:
    (output / "manifest.json").write_text(json.dumps({"gates": gates}), encoding="utf-8")
create_racing_destination()
raise SystemExit(1 if failed else 0)
'''


class ValidateDriverMatrixTest(unittest.TestCase):
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

    def test_fixed_matrix_covers_every_family_scenario_gate_and_event_class_twice(self):
        output = self.outputs / "matrix"
        completed = self.run_matrix(output)

        self.assertEqual(completed.returncode, 0, completed.stderr or completed.stdout)
        report = json.loads((output / "matrix_report.json").read_text(encoding="utf-8"))
        self.assertTrue(report["matrix_exact"])
        self.assertEqual((report["case_count"], report["passed_case_count"], report["failed_case_count"]), (30, 30, 0))
        by_family = {}
        for case in report["cases"]:
            by_family.setdefault(case["fixture"]["family"], []).append(case)
            self.assertTrue(case["deterministic"])
            self.assertTrue(case["fixture_identity"]["exact"])
            self.assertTrue(all(gate["passed"] for gate in case["gates"].values()))
        self.assertEqual(set(by_family), {"psw", "directsound", "sq1", "sq2"})
        self.assertEqual(len(by_family["psw"]), 12)
        self.assertTrue(all(len(cases) == 6 for family, cases in by_family.items() if family != "psw"))
        for family, cases in by_family.items():
            self.assertEqual([case["scenario"] for case in cases[:6]], ["start", "envelope", "pitch", "volume-pan", "retrigger", "release"])
        fixture = {case["fixture"]["kind"]: case["fixture"] for case in report["cases"]}
        self.assertEqual(fixture["directsound"]["voicegroup_symbol"], "voicegroup_rg_poke_center")
        self.assertEqual(fixture["directsound"]["voice_index"], 4)
        self.assertEqual(fixture["sq1"]["voicegroup_symbol"], "voicegroup_vs_wild")
        self.assertEqual(fixture["sq1"]["voice_index"], 1)
        self.assertEqual(fixture["sq2"]["voicegroup_symbol"], "voicegroup_vs_wild")
        self.assertEqual(fixture["sq2"]["voice_index"], 4)
        self.assertEqual({case["controls"]["note"] for case in report["cases"]}, {36, 60, 84})
        self.assertEqual({case["controls"]["pan"] for case in report["cases"]}, {0, 64, 127})
        for source in ("reference", "candidate"):
            coverage = report["coverage"]["sources"][source]
            self.assertTrue(coverage["complete"])
            for family in coverage["families"].values():
                self.assertTrue(family["complete"])
                self.assertEqual(family["unexercised_required_event_classes"], [])
                for event in family["required_event_coverage"]:
                    if event["required"]:
                        self.assertTrue(event["exercised"])
                        self.assertTrue(any(case_run.endswith("/first") for case_run in event["case_runs"]))
                        self.assertTrue(any(case_run.endswith("/repeat") for case_run in event["case_runs"]))
        self.assertTrue(report["coverage"]["complete"])
        self.assertEqual(len(self.calls()), 60)

    def test_unexercised_required_directsound_event_class_fails_coverage(self):
        output = self.outputs / "missing-directsound-event"
        completed = self.run_matrix(output, MATRIX_DROP_EVENT="directsound:timer_overflow")

        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        report = json.loads((output.with_name(output.name + ".failed") / "matrix_report.json").read_text(encoding="utf-8"))
        self.assertFalse(report["matrix_exact"])
        for source in ("reference", "candidate"):
            directsound = report["coverage"]["sources"][source]["families"]["directsound"]
            self.assertFalse(directsound["complete"])
            self.assertEqual(directsound["unexercised_required_event_classes"], ["timer_overflow"])
        self.assertEqual(len(self.calls()), 60)

    def test_failed_case_preserves_first_transaction_and_native_sample_divergence(self):
        output = self.outputs / "failed"
        completed = self.run_matrix(output, MATRIX_FAIL_CASE="start-directsound")

        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        failed = output.with_name(output.name + ".failed")
        report = json.loads((failed / "matrix_report.json").read_text(encoding="utf-8"))
        self.assertFalse(report["matrix_exact"])
        case = next(case for case in report["cases"] if case["id"] == "start-directsound")
        self.assertEqual(case["runs"]["first"]["diagnostics"]["first_transaction_divergence"]["ordinal"], 2)
        native = case["runs"]["first"]["diagnostics"]["first_native_sample_divergence"]
        self.assertEqual(native["comparison"], "reference-hardware-compare")
        self.assertEqual(native["mismatch"]["reference_causal_sample"]["ordinal"], 3)
        self.assertEqual(len(self.calls()), 60)

    def test_payload_identity_mismatch_is_an_explicit_semantic_matrix_failure(self):
        output = self.outputs / "identity"
        completed = self.run_matrix(output, MATRIX_IDENTITY_MISMATCH="release-sq2")

        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        report = json.loads((output.with_name(output.name + ".failed") / "matrix_report.json").read_text(encoding="utf-8"))
        case = next(case for case in report["cases"] if case["id"] == "release-sq2")
        self.assertFalse(case["fixture_identity"]["exact"])
        self.assertFalse(case["passed"])
        self.assertEqual(len(self.calls()), 60)

    def test_existing_output_or_diagnostics_is_rejected_without_running_the_matrix(self):
        output = self.outputs / "existing"
        for existing in (output, output.with_name(output.name + ".failed")):
            with self.subTest(existing=existing.name):
                existing.mkdir()
                marker = existing / "prior-owner.bin"
                marker.write_bytes(b"prior owner")
                completed = self.run_matrix(output)
                self.assertEqual(completed.returncode, 2, completed.stderr or completed.stdout)
                self.assertEqual(self.calls(), [])
                self.assertEqual(marker.read_bytes(), b"prior owner")
                marker.unlink()
                existing.rmdir()

    def test_success_publication_does_not_replace_a_racing_empty_directory(self):
        output = self.outputs / "racing-success"
        completed = self.run_matrix(output, MATRIX_RACE_DESTINATION=str(output))

        self.assertEqual(completed.returncode, 2, completed.stderr or completed.stdout)
        self.assertTrue(output.is_dir())
        self.assertEqual(list(output.iterdir()), [])
        self.assertTrue((output.with_name(output.name + ".failed") / "matrix_report.json").is_file())

    def test_failed_publication_preserves_diagnostics_when_a_racing_empty_directory_wins(self):
        output = self.outputs / "racing-failure"
        failed = output.with_name(output.name + ".failed")
        completed = self.run_matrix(
            output,
            MATRIX_FAIL_CASE="start-directsound",
            MATRIX_RACE_DESTINATION=str(failed),
        )

        self.assertEqual(completed.returncode, 2, completed.stderr or completed.stdout)
        self.assertTrue(failed.is_dir())
        self.assertEqual(list(failed.iterdir()), [])
        stage = output.parent / f".{output.name}.matrix.stage"
        self.assertTrue((stage / "matrix_report.json").is_file())
        self.assertIn(str(stage), completed.stderr)

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
