#!/usr/bin/env python3
"""Behavioral tests for the fixed-driver lifecycle validation orchestrator."""

import hashlib
import json
import os
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path


TOOL = Path(__file__).with_name("validate_driver.py")
FAMILIES = {
    "directsound": {"type": 0, "family": "directsound", "solo": "directsound", "mask": 48},
    "sq1": {"type": 1, "family": "sq1", "solo": "sq1", "mask": 1},
    "sq2": {"type": 2, "family": "sq2", "solo": "sq2", "mask": 2},
    "psw": {"type": 3, "family": "psw", "solo": "wave", "mask": 4},
    "psw-alt": {"type": 11, "family": "psw", "solo": "wave", "mask": 4},
}
SCENARIOS = {
    "start": {"logical_vblanks": 1, "capture_frames": 9, "span_cycles": 2_536_960, "high_level_action": "note-on at tick 0"},
    "envelope": {"logical_vblanks": 6, "capture_frames": 15, "span_cycles": 4_222_464, "high_level_action": "note-on at tick 0; sustain through tick 6"},
    "pitch": {"logical_vblanks": 4, "capture_frames": 12, "span_cycles": 3_379_712, "high_level_action": "note-on at tick 0; pitch bend +16 at tick 2; sustain through tick 4"},
    "volume-pan": {"logical_vblanks": 4, "capture_frames": 12, "span_cycles": 3_379_712, "high_level_action": "note-on at tick 0; volume 32 and pan 127 at tick 2; sustain through tick 4"},
    "retrigger": {"logical_vblanks": 5, "capture_frames": 15, "span_cycles": 4_222_464, "high_level_action": "note-on at tick 0; note-off at tick 2; note-on at tick 3; sustain through tick 5"},
    "release": {"logical_vblanks": 6, "capture_frames": 14, "span_cycles": 3_941_376, "high_level_action": "note-on at tick 0; note-off at tick 2; release through tick 6"},
}


MOCK_TOOL = r'''
import hashlib
import json
import os
import sys
from pathlib import Path


role = Path(sys.argv[0]).stem.removesuffix("_mock")
args = sys.argv[1:]
family = os.environ.get("MOCK_FAMILY", "psw")
families = {
    "directsound": {"type": 0, "family": "directsound", "solo": "directsound", "mask": 48},
    "sq1": {"type": 1, "family": "sq1", "solo": "sq1", "mask": 1},
    "sq2": {"type": 2, "family": "sq2", "solo": "sq2", "mask": 2},
    "psw": {"type": 3, "family": "psw", "solo": "wave", "mask": 4},
    "psw-alt": {"type": 11, "family": "psw", "solo": "wave", "mask": 4},
}
scenarios = {
    "start": {"logical_vblanks": 1, "capture_frames": 9, "span_cycles": 2536960, "high_level_action": "note-on at tick 0"},
    "envelope": {"logical_vblanks": 6, "capture_frames": 15, "span_cycles": 4222464, "high_level_action": "note-on at tick 0; sustain through tick 6"},
    "pitch": {"logical_vblanks": 4, "capture_frames": 12, "span_cycles": 3379712, "high_level_action": "note-on at tick 0; pitch bend +16 at tick 2; sustain through tick 4"},
    "volume-pan": {"logical_vblanks": 4, "capture_frames": 12, "span_cycles": 3379712, "high_level_action": "note-on at tick 0; volume 32 and pan 127 at tick 2; sustain through tick 4"},
    "retrigger": {"logical_vblanks": 5, "capture_frames": 15, "span_cycles": 4222464, "high_level_action": "note-on at tick 0; note-off at tick 2; note-on at tick 3; sustain through tick 5"},
    "release": {"logical_vblanks": 6, "capture_frames": 14, "span_cycles": 3941376, "high_level_action": "note-on at tick 0; note-off at tick 2; release through tick 6"},
}
if family not in families:
    raise SystemExit(90)
spec = families[family]
family_name = spec["family"]
published = Path(os.environ["MOCK_PUBLISHED"])
log_path = Path(os.environ["MOCK_LOG"])


def option(flag):
    try:
        return args[args.index(flag) + 1]
    except (ValueError, IndexError):
        print(f"{role}: missing {flag}", file=sys.stderr)
        raise SystemExit(91)


def controls():
    return {name: int(option(f"--{name}")) for name in ("note", "velocity", "volume", "pan")}


def payload_hash():
    return hashlib.sha256(bytes([spec["type"], 5, 7, 11])).hexdigest()


def tone_hash():
    return hashlib.sha256(bytes([spec["type"]]) + bytes(11)).hexdigest()


def write_trace(path):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("PORYAAAA_AUDIO_TRACE 1\nCLOCK 16777216\nBEGIN 0 0\nEND 512 1\n", encoding="utf-8")
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_capture(prefix, source, trace=None):
    prefix = Path(prefix)
    prefix.parent.mkdir(parents=True, exist_ok=True)
    pcm = prefix.with_suffix(".pcm")
    cycles = prefix.with_suffix(".cycles")
    pcm.write_bytes(b"\0\0\0\0")
    cycles.write_bytes((0).to_bytes(8, "little"))
    document = {
        "format": "poryaaaa-native-capture", "version": 1, "source": source,
        "clock_hz": 16777216, "channels": 2, "sample_format": "s16le", "cycle_format": "u64le",
        "frame_count": 1, "first_cycle": 0, "last_cycle": 0, "solo_mask": spec["mask"],
    }
    if source == "mgba-full":
        decomp = Path(option("--decomp"))
        scenario = option("--scenario")
        contract = scenarios[scenario]
        span = contract["span_cycles"]
        capture_frames = contract["capture_frames"]
        if os.environ.get("MOCK_TRUNCATE_SCENARIO") == "1":
            capture_frames -= 1
            span -= 280896
        document.update({
            "trace_sha256": hashlib.sha256(Path(trace).read_bytes()).hexdigest(),
            "rom_sha256": hashlib.sha256((decomp / "pokeemerald-hearth.gba").read_bytes()).hexdigest(),
            "elf_sha256": hashlib.sha256((decomp / "pokeemerald-hearth.elf").read_bytes()).hexdigest(),
            "tone_data_sha256": tone_hash(), "family_payload_sha256": payload_hash(),
            "pcm_sha256": hashlib.sha256(pcm.read_bytes()).hexdigest(), "cycles_sha256": hashlib.sha256(cycles.read_bytes()).hexdigest(),
            "rom_voice_address": 0x08000000, "scenario_span_frames": capture_frames,
            "scenario_span_cycles": span, "scenario_begin_cycle": 0, "scenario_end_cycle": span,
            "scenario_logical_vblanks": contract["logical_vblanks"], "scenario_capture_frames": capture_frames,
            "high_level_action": contract["high_level_action"], "voicegroup_symbol": "voicegroup_fixture", "voice_index": 7,
            "resolved_type": spec["type"], "family": family_name, "scenario": scenario, **controls(),
            "audio_channel_mask": spec["mask"], "mgba_master_volume": 256, "bios_mode": "hle",
            "mgba_commit": "afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9",
            "mgba_base_revision": "afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9",
            "mgba_observation_patch_sha256": "c" * 64, "mgba_source_policy": "authoritative-pinned-source", "mgba_dirty": False,
        })
    elif source == "mgba-clone":
        document.update({
            "trace_sha256": hashlib.sha256(Path(trace).read_bytes()).hexdigest(), "audio_channel_mask": spec["mask"],
            "mgba_master_volume": 256, "bios_mode": "hle", "mgba_commit": "afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9",
            "mgba_base_revision": "afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9", "mgba_observation_patch_sha256": "c" * 64,
            "mgba_source_policy": "authoritative-pinned-source", "mgba_dirty": False,
        })
    prefix.with_suffix(".json").write_text(json.dumps(document, sort_keys=True), encoding="utf-8")

def create_racing_destination():
    destination = os.environ.get("MOCK_RACE_DESTINATION")
    if destination and role == "native_compare" and Path(args[0]).name == "candidate-mgba.json":
        Path(destination).mkdir()


with log_path.open("a", encoding="utf-8") as output:
    output.write(json.dumps({"role": role, "argv": args, "published_exists": published.exists()}, sort_keys=True) + "\n")
if published.exists():
    raise SystemExit(95)

if role == "record_voice":
    if "--solo" in args or option("--capture-stage") != "native":
        raise SystemExit(92)
    trace = option("--trace-output")
    write_trace(trace)
    write_capture(option("--native-output-prefix"), "mgba-full", trace)
elif role == "candidate_trace":
    if len(args) < 3 or args[1] != "voicegroup_fixture" or args[2] != "7":
        raise SystemExit(92)
    scenario = option("--scenario")
    trace = option("--trace-output")
    trace_hash = write_trace(trace)
    contract = scenarios[scenario]
    capture_frames = contract["capture_frames"]
    span = contract["span_cycles"]
    if os.environ.get("MOCK_TRUNCATE_SCENARIO") == "1":
        capture_frames -= 1
        span -= 280896
    resolved_type = spec["type"]
    if os.environ.get("MOCK_FIXTURE_MISMATCH") == "type":
        resolved_type = 11 if resolved_type != 11 else 3
    payload = payload_hash()
    if os.environ.get("MOCK_FIXTURE_MISMATCH") == "payload":
        payload = "0" * 64
    decomp = Path(args[0])
    rom_hash = hashlib.sha256((decomp / "pokeemerald-hearth.gba").read_bytes()).hexdigest()
    elf_hash = hashlib.sha256((decomp / "pokeemerald-hearth.elf").read_bytes()).hexdigest()
    if os.environ.get("MOCK_CANDIDATE_ROM_HASH_MISMATCH") == "1":
        rom_hash = "0" * 64
    if os.environ.get("MOCK_CANDIDATE_ELF_HASH_MISMATCH") == "1":
        elf_hash = "0" * 64
    action_tick_schedule = [True] * contract["logical_vblanks"] + [False]
    if os.environ.get("MOCK_ACTION_TICK_SCHEDULE_MISMATCH") == "1":
        action_tick_schedule[-1] = True
    document = {
        "format": "poryaaaa-driver-candidate-trace", "version": 1, "source": "poryaaaa-driver",
        "trace_format": "PORYAAAA_AUDIO_TRACE", "trace_version": 1, "clock_hz": 16777216,
        "trace_begin_cycle": 0, "trace_end_cycle": span,
        "driver_origin_cycle": 1005 if family_name == "directsound" else 0,
        "logical_vblanks": contract["logical_vblanks"], "capture_frames": capture_frames,
        "capture_span_cycles": span, "high_level_action": contract["high_level_action"],
        "action_tick_schedule": action_tick_schedule,
        "voicegroup_symbol": "voicegroup_fixture", "voice_index": 7, "family": family_name, "resolved_type": resolved_type,
        "tone_data_sha256": tone_hash(), "family_payload_sha256": payload, "scenario": scenario,
        "trace_sha256": trace_hash, "rom_sha256": rom_hash, "elf_sha256": elf_hash,
        **controls(),
    }
    Path(str(trace) + ".manifest.json").write_text(json.dumps(document, sort_keys=True), encoding="utf-8")
elif role in {"mgba_replay", "pory_replay"}:
    trace = option("--input")
    if option("--solo") != spec["solo"]:
        raise SystemExit(92)
    write_capture(option("--output-prefix"), "mgba-clone" if role == "mgba_replay" else "poryaaaa", trace)
elif role == "driver_compare":
    if option("--family") != family_name:
        raise SystemExit(92)
    output = Path(option("--output"))
    transaction = os.environ.get("MOCK_COMPARE_EXIT", "0") == "0"
    output.write_text(json.dumps({
        "family": family_name, "transaction_exact": transaction, "payload_exact": True, "logical_state_exact": True,
        "cycle_exact": True, "reference_event_count": 1, "candidate_event_count": 1,
        "first_divergence": None if transaction else {"kind": "transaction", "ordinal": 0},
        "logical_state_divergence": None, "payload": {"kind": family_name}, "timing": {},
        "hashes": {
            "reference_trace_sha256": hashlib.sha256(Path(args[0]).read_bytes()).hexdigest(),
            "candidate_trace_sha256": hashlib.sha256(Path(args[1]).read_bytes()).hexdigest(),
            "reference_payload_sha256": payload_hash(), "candidate_payload_sha256": payload_hash(),
        },
    }, sort_keys=True), encoding="utf-8")
    raise SystemExit(int(os.environ.get("MOCK_COMPARE_EXIT", "0")))
elif role == "native_compare":
    if len(args) != 6 or args[2] != "--reference-trace" or args[4] != "--candidate-trace":
        raise SystemExit(92)
    reference = Path(args[0])
    candidate = Path(args[1])
    reference_document = json.loads(reference.read_text(encoding="utf-8"))
    candidate_document = json.loads(candidate.read_text(encoding="utf-8"))
    passed = os.environ.get("MOCK_NATIVE_EXIT", "0") == "0"
    print(json.dumps({
        "reference": args[0], "candidate": args[1], "passed": passed, "first_mismatch": None if passed else {"kind": "pcm", "frame_index": 0},
        "reference_frame_count": reference_document["frame_count"], "candidate_frame_count": candidate_document["frame_count"],
        "hashes": {
            "reference_pcm_sha256": hashlib.sha256(reference.with_suffix(".pcm").read_bytes()).hexdigest(),
            "candidate_pcm_sha256": hashlib.sha256(candidate.with_suffix(".pcm").read_bytes()).hexdigest(),
            "reference_cycles_sha256": hashlib.sha256(reference.with_suffix(".cycles").read_bytes()).hexdigest(),
            "candidate_cycles_sha256": hashlib.sha256(candidate.with_suffix(".cycles").read_bytes()).hexdigest(),
        },
    }, sort_keys=True))
    create_racing_destination()
    raise SystemExit(int(os.environ.get("MOCK_NATIVE_EXIT", "0")))
else:
    raise SystemExit(93)
'''


class ValidateDriverTest(unittest.TestCase):
    """Exercise the validator only through its public command seam."""

    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.directory = Path(self.temporary_directory.name)
        self.decomp = self.directory / "decomp"
        self.decomp.mkdir()
        (self.decomp / "pokeemerald-hearth.gba").write_bytes(b"mock rom")
        (self.decomp / "pokeemerald-hearth.elf").write_bytes(b"mock elf")
        self.output_parent = self.directory / "published"
        self.output_parent.mkdir()
        self.output = self.output_parent / "validation"
        self.log_path = self.directory / "tool-log.jsonl"
        self.log_path.write_text("", encoding="utf-8")
        self.tools = self.directory / "tools"
        self.tools.mkdir()
        self.overrides = {
            "--record-voice": self.make_mock("record_voice"),
            "--candidate-trace": self.make_mock("candidate_trace"),
            "--mgba-replay": self.make_mock("mgba_replay"),
            "--pory-replay": self.make_mock("pory_replay"),
            "--driver-compare": self.make_mock("driver_compare"),
            "--native-compare": self.make_mock("native_compare"),
        }

    def tearDown(self):
        self.temporary_directory.cleanup()

    def make_mock(self, role):
        path = self.tools / f"{role}_mock.py"
        path.write_text("#!/usr/bin/env python3\n" + textwrap.dedent(MOCK_TOOL), encoding="utf-8")
        path.chmod(0o755)
        return path

    @staticmethod
    def option(arguments, flag):
        try:
            return arguments[arguments.index(flag) + 1]
        except (ValueError, IndexError) as error:
            raise AssertionError(f"missing {flag} in argv: {arguments}") from error

    def command(self, output, scenario="start"):
        command = [
            sys.executable, str(TOOL), "--decomp", str(self.decomp), "--voicegroup", "voicegroup_fixture",
            "--voice", "7", "--scenario", scenario, "--output-dir", str(output),
            "--note", "61", "--velocity", "102", "--volume", "99", "--pan", "12",
        ]
        for flag, executable in self.overrides.items():
            command.extend((flag, str(executable)))
        return command

    def run_validator(self, output=None, scenario="start", **environment):
        output = self.output if output is None else output
        command_environment = os.environ.copy()
        command_environment.update({"MOCK_LOG": str(self.log_path), "MOCK_PUBLISHED": str(output), **environment})
        return subprocess.run(self.command(output, scenario), check=False, capture_output=True, text=True, env=command_environment)

    def tool_log(self):
        return [json.loads(line) for line in self.log_path.read_text(encoding="utf-8").splitlines()]

    def assert_artifacts(self, directory):
        expected = {"reference.trace", "candidate.trace", "candidate.trace.manifest.json", "driver-compare.json", "manifest.json"}
        for prefix in ("reference-native", "reference-mgba", "reference-pory", "candidate-mgba", "candidate-pory"):
            expected.update({f"{prefix}.pcm", f"{prefix}.cycles", f"{prefix}.json"})
        expected.update({"reference-native-compare.json", "reference-hardware-compare.json", "candidate-hardware-compare.json"})
        self.assertTrue(directory.is_dir())
        self.assertTrue(expected.issubset({path.name for path in directory.iterdir()}))

    def test_family_is_manifest_derived_and_all_commands_use_its_solo_profile(self):
        for family, spec in FAMILIES.items():
            with self.subTest(family=family):
                output = self.output_parent / family
                completed = self.run_validator(output, MOCK_FAMILY=family)
                self.assertEqual(completed.returncode, 0, completed.stderr or completed.stdout)
                calls = self.tool_log()
                recorder = next(call["argv"] for call in calls if call["role"] == "record_voice")
                self.assertNotIn("--solo", recorder)
                self.assertEqual(self.option(recorder, "--scenario"), "start")
                comparator = next(call["argv"] for call in calls if call["role"] == "driver_compare")
                self.assertEqual(self.option(comparator, "--family"), spec["family"])
                replay_calls = [call["argv"] for call in calls if call["role"] in {"mgba_replay", "pory_replay"}]
                self.assertEqual(len(replay_calls), 4)
                self.assertTrue(all(self.option(call, "--solo") == spec["solo"] for call in replay_calls))
                native_calls = [call["argv"] for call in calls if call["role"] == "native_compare"]
                self.assertEqual(len(native_calls), 3)
                self.assertTrue(all("--reference-trace" in call and "--candidate-trace" in call for call in native_calls))
                manifest = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
                self.assertEqual(manifest["family"], spec["family"])
                self.assertEqual(manifest["fixture"]["resolved_type"], spec["type"])
                self.assertEqual(manifest["status"], "passed")
                self.assert_artifacts(output)
                self.log_path.write_text("", encoding="utf-8")

    def test_all_scenarios_are_forwarded_and_publish_fixed_actions(self):
        for scenario, expected in SCENARIOS.items():
            with self.subTest(scenario=scenario):
                output = self.output_parent / scenario
                completed = self.run_validator(output, scenario, MOCK_FAMILY="sq2")
                self.assertEqual(completed.returncode, 0, completed.stderr or completed.stdout)
                manifest = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
                self.assertEqual(manifest["scenario"], scenario)
                self.assertEqual(manifest["scenario_timing"]["logical_vblanks"], expected["logical_vblanks"])
                self.assertEqual(manifest["scenario_timing"]["high_level_action"], expected["high_level_action"])
                self.assertEqual(manifest["scenario_timing"]["capture_frames"], expected["capture_frames"])
                self.assertEqual(manifest["scenario_timing"]["span_cycles"], expected["span_cycles"])
                self.assertEqual(
                    manifest["scenario_timing"]["action_tick_schedule"],
                    [True] * expected["logical_vblanks"] + [False],
                )
                self.log_path.write_text("", encoding="utf-8")

    def test_final_settled_tick_must_reject_new_actions(self):
        completed = self.run_validator(MOCK_ACTION_TICK_SCHEDULE_MISMATCH="1")

        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        self.assertFalse(self.output.exists())
        self.assertEqual([call["role"] for call in self.tool_log()], ["record_voice", "candidate_trace"])

    def test_identically_truncated_adapter_windows_fail_before_comparison(self):
        completed = self.run_validator(scenario="retrigger", MOCK_TRUNCATE_SCENARIO="1")
        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        self.assertFalse(self.output.exists())
        self.assertEqual([call["role"] for call in self.tool_log()], ["record_voice"])

    def test_fixture_identity_mismatch_fails_before_comparator(self):
        for mismatch in ("type", "payload"):
            with self.subTest(mismatch=mismatch):
                output = self.output_parent / f"mismatch-{mismatch}"
                completed = self.run_validator(output, MOCK_FAMILY="sq1", MOCK_FIXTURE_MISMATCH=mismatch)
                self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
                self.assertFalse(output.exists())
                self.assertTrue(output.with_name(output.name + ".failed").is_dir())
                roles = [call["role"] for call in self.tool_log()]
                self.assertEqual(roles, ["record_voice", "candidate_trace"])
                self.log_path.write_text("", encoding="utf-8")

    def test_candidate_rom_pairing_mismatch_fails_before_comparator(self):
        completed = self.run_validator(MOCK_CANDIDATE_ROM_HASH_MISMATCH="1")
        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        self.assertFalse(self.output.exists())
        self.assertTrue(self.output.with_name(self.output.name + ".failed").is_dir())
        self.assertEqual([call["role"] for call in self.tool_log()], ["record_voice", "candidate_trace"])

    def test_candidate_elf_pairing_mismatch_fails_before_comparator(self):
        completed = self.run_validator(MOCK_CANDIDATE_ELF_HASH_MISMATCH="1")
        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        self.assertFalse(self.output.exists())
        self.assertTrue(self.output.with_name(self.output.name + ".failed").is_dir())
        self.assertEqual([call["role"] for call in self.tool_log()], ["record_voice", "candidate_trace"])

    def test_semantic_comparator_failure_keeps_all_diagnostics_and_publishes_failed_manifest(self):
        completed = self.run_validator(MOCK_FAMILY="directsound", MOCK_COMPARE_EXIT="1")
        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        failed = self.output.with_name(self.output.name + ".failed")
        self.assertFalse(self.output.exists())
        self.assert_artifacts(failed)
        manifest = json.loads((failed / "manifest.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["status"], "failed")
        self.assertFalse(manifest["gates"]["transaction_exact"])
        self.assertEqual(manifest["diagnostics"]["first_transaction_divergence"]["ordinal"], 0)
        self.assertEqual(set(manifest["diagnostics"]["first_sample_divergences"]), {"reference_native", "reference_hardware", "candidate_hardware"})
        calls = self.tool_log()
        self.assertEqual(sum(call["role"] == "mgba_replay" for call in calls), 2)
        self.assertEqual(sum(call["role"] == "pory_replay" for call in calls), 2)
        self.assertEqual(sum(call["role"] == "native_compare" for call in calls), 3)

    def test_native_gate_failure_preserves_first_sample_diagnostics(self):
        completed = self.run_validator(MOCK_FAMILY="sq1", MOCK_NATIVE_EXIT="1")
        self.assertEqual(completed.returncode, 1, completed.stderr or completed.stdout)
        failed = self.output.with_name(self.output.name + ".failed")
        self.assert_artifacts(failed)
        manifest = json.loads((failed / "manifest.json").read_text(encoding="utf-8"))
        self.assertFalse(manifest["gates"]["reference_native_exact"])
        self.assertFalse(manifest["gates"]["reference_hardware_exact"])
        self.assertFalse(manifest["gates"]["candidate_hardware_exact"])
        self.assertTrue(all(item["frame_index"] == 0 for item in manifest["diagnostics"]["first_sample_divergences"].values()))

    def test_comparator_infrastructure_failure_stops_before_replay(self):
        completed = self.run_validator(MOCK_COMPARE_EXIT="2")
        self.assertEqual(completed.returncode, 2, completed.stderr or completed.stdout)
        failed = self.output.with_name(self.output.name + ".failed")
        self.assertTrue(failed.is_dir())
        self.assertFalse((failed / "manifest.json").exists())
        self.assertEqual([call["role"] for call in self.tool_log()], ["record_voice", "candidate_trace", "driver_compare"])

    def test_existing_destination_is_rejected_without_tools(self):
        for existing in (self.output, self.output.with_name(self.output.name + ".failed")):
            with self.subTest(existing=existing.name):
                existing.mkdir()
                marker = existing / "prior-owner.bin"
                marker.write_bytes(b"prior owner")
                completed = self.run_validator()
                self.assertEqual(completed.returncode, 2, completed.stderr or completed.stdout)
                self.assertEqual(self.tool_log(), [])
                self.assertEqual(marker.read_bytes(), b"prior owner")
                marker.unlink()
                existing.rmdir()

    def test_success_publication_does_not_replace_a_racing_empty_directory(self):
        completed = self.run_validator(MOCK_RACE_DESTINATION=str(self.output))

        self.assertEqual(completed.returncode, 2, completed.stderr or completed.stdout)
        self.assertTrue(self.output.is_dir())
        self.assertEqual(list(self.output.iterdir()), [])
        self.assert_artifacts(self.output.with_name(self.output.name + ".failed"))

    def test_failed_publication_preserves_diagnostics_when_a_racing_empty_directory_wins(self):
        failed = self.output.with_name(self.output.name + ".failed")
        completed = self.run_validator(MOCK_COMPARE_EXIT="1", MOCK_RACE_DESTINATION=str(failed))

        self.assertEqual(completed.returncode, 2, completed.stderr or completed.stdout)
        self.assertTrue(failed.is_dir())
        self.assertEqual(list(failed.iterdir()), [])
        stage = self.output.parent / f".{self.output.name}.stage"
        self.assert_artifacts(stage)
        self.assertIn(str(stage), completed.stderr)

    def test_equivalent_output_parents_have_identical_manifest_bytes(self):
        first_parent = self.directory / "first"
        second_parent = self.directory / "second"
        first_parent.mkdir()
        second_parent.mkdir()
        first = self.run_validator(first_parent / "result", MOCK_FAMILY="sq1")
        second = self.run_validator(second_parent / "result", MOCK_FAMILY="sq1")
        self.assertEqual(first.returncode, 0, first.stderr or first.stdout)
        self.assertEqual(second.returncode, 0, second.stderr or second.stdout)
        self.assertEqual((first_parent / "result" / "manifest.json").read_bytes(), (second_parent / "result" / "manifest.json").read_bytes())


if __name__ == "__main__":
    unittest.main()
