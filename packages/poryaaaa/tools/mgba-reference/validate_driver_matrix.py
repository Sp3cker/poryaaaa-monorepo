#!/usr/bin/env python3
"""Run the fixed five-fixture driver lifecycle matrix twice and publish one report."""

import argparse
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any

from atomic_publish import PublishError, publish_directory_no_replace
from driver_compare import TraceError, parse_trace
from validate_driver import SCENARIO_CONTRACTS, require_regular_file


REQUIRED_GATES = (
    "transaction_exact",
    "payload_exact",
    "logical_state_exact",
    "reference_native_exact",
    "reference_hardware_exact",
    "candidate_hardware_exact",
)
NATIVE_PREFIXES = (
    "reference-native",
    "reference-mgba",
    "reference-pory",
    "candidate-mgba",
    "candidate-pory",
)
CAPTURE_ARTIFACTS = (
    "reference.trace",
    "candidate.trace",
    "candidate.trace.manifest.json",
    *(f"{prefix}.{suffix}" for prefix in NATIVE_PREFIXES for suffix in ("pcm", "cycles", "json")),
)
IDENTITY_FIELDS = ("family", "resolved_type", "tone_data_sha256", "family_payload_sha256")
CANONICAL_MANIFEST = "manifest.json"

# The two PSW fixtures preserve the existing verified matrix.  The three new
# fixtures are source-verified in hearth-test: rg_poke_center.inc:6 and
# vs_wild.inc:3/:6 respectively.  Voice slots are zero-based.
FIXTURES = (
    {
        "kind": "normal",
        "family": "psw",
        "voicegroup": "voicegroup_aa_girl",
        "voice": 4,
        "resolved_type": 0x03,
    },
    {
        "kind": "alternate",
        "family": "psw",
        "voicegroup": "voicegroup_poke_center",
        "voice": 1,
        "resolved_type": 0x0B,
    },
    {
        "kind": "directsound",
        "family": "directsound",
        "voicegroup": "voicegroup_rg_poke_center",
        "voice": 4,
        "resolved_type": 0x00,
    },
    {
        "kind": "sq1",
        "family": "sq1",
        "voicegroup": "voicegroup_vs_wild",
        "voice": 1,
        "resolved_type": 0x01,
    },
    {
        "kind": "sq2",
        "family": "sq2",
        "voicegroup": "voicegroup_vs_wild",
        "voice": 4,
        "resolved_type": 0x02,
    },
)

# Every fixture covers the same fixed lifecycle actions.  The controls retain
# the existing PSW note and pan spread while exercising non-default endpoints.
SCENARIO_CONTROLS = (
    {"scenario": "start", "note": 36, "velocity": 127, "volume": 127, "pan": 0},
    {"scenario": "envelope", "note": 60, "velocity": 127, "volume": 127, "pan": 64},
    {"scenario": "pitch", "note": 84, "velocity": 127, "volume": 127, "pan": 127},
    {"scenario": "volume-pan", "note": 36, "velocity": 127, "volume": 127, "pan": 64},
    {"scenario": "retrigger", "note": 60, "velocity": 127, "volume": 127, "pan": 127},
    {"scenario": "release", "note": 84, "velocity": 127, "volume": 127, "pan": 0},
)
MATRIX_CASES = tuple(
    {**fixture, **controls, "id": f"{controls['scenario']}-{fixture['kind']}"}
    for fixture in FIXTURES
    for controls in SCENARIO_CONTROLS
)

# A class is required only when the fixed source-verified fixture is expected
# to emit it.  FIFO channel and timer variants remain visible but optional:
# the evidence must show which real path the one DirectSound fixture reaches,
# without falsely rejecting it merely for not instantiating another variant.
EVENT_CLASSES = {
    "psw": (
        {"name": "nr30", "kind": "WRITE", "addresses": (0x04000070, 0x04000071), "scope": "measured", "required": True},
        {"name": "nr31", "kind": "WRITE", "addresses": (0x04000072,), "scope": "measured", "required": True},
        {"name": "nr32", "kind": "WRITE", "addresses": (0x04000073,), "scope": "measured", "required": True},
        {"name": "nr33", "kind": "WRITE", "addresses": (0x04000074,), "scope": "measured", "required": True},
        {"name": "nr34", "kind": "WRITE", "addresses": (0x04000075,), "scope": "measured", "required": True},
        {"name": "nr50", "kind": "WRITE", "addresses": (0x04000080,), "scope": "measured", "required": True},
        {"name": "nr51", "kind": "WRITE", "addresses": (0x04000081,), "scope": "measured", "required": True},
        *(
            {
                "name": f"wave_ram_{offset:02d}",
                "kind": "WRITE",
                "addresses": (0x04000090 + offset,),
                "scope": "measured",
                "required": True,
            }
            for offset in range(16)
        ),
    ),
    "sq1": (
        {"name": "nr10_sweep", "kind": "WRITE", "addresses": (0x04000060, 0x04000061), "scope": "measured", "required": True},
        {"name": "nr11_duty_length", "kind": "WRITE", "addresses": (0x04000062,), "scope": "measured", "required": True},
        {"name": "nr12_envelope", "kind": "WRITE", "addresses": (0x04000063,), "scope": "measured", "required": True},
        {"name": "nr13_frequency_low", "kind": "WRITE", "addresses": (0x04000064,), "scope": "measured", "required": True},
        {"name": "nr14_frequency_high_trigger", "kind": "WRITE", "addresses": (0x04000065,), "scope": "measured", "required": True},
        {"name": "nr50", "kind": "WRITE", "addresses": (0x04000080,), "scope": "measured", "required": True},
        {"name": "nr51", "kind": "WRITE", "addresses": (0x04000081,), "scope": "measured", "required": True},
    ),
    "sq2": (
        {"name": "nr21_duty_length", "kind": "WRITE", "addresses": (0x04000068,), "scope": "measured", "required": True},
        {"name": "nr22_envelope", "kind": "WRITE", "addresses": (0x04000069,), "scope": "measured", "required": True},
        {"name": "nr23_frequency_low", "kind": "WRITE", "addresses": (0x0400006C,), "scope": "measured", "required": True},
        {"name": "nr24_frequency_high_trigger", "kind": "WRITE", "addresses": (0x0400006D,), "scope": "measured", "required": True},
        {"name": "nr50", "kind": "WRITE", "addresses": (0x04000080,), "scope": "measured", "required": True},
        {"name": "nr51", "kind": "WRITE", "addresses": (0x04000081,), "scope": "measured", "required": True},
    ),
    "directsound": (
        {"name": "fifo_word", "kind": "WRITE", "starts": (0x040000A0, 0x040000A4), "width": 4, "scope": "measured", "required": True},
        {"name": "timer_overflow", "kind": "TIMER", "timer_ids": (0, 1), "scope": "measured", "required": True},
        {"name": "sample", "kind": "SAMPLE", "scope": "measured", "required": True},
        {"name": "soundcnt_h_routing", "kind": "WRITE", "addresses": (0x04000082, 0x04000083), "scope": "setup", "required": True},
        {"name": "soundbias", "kind": "WRITE", "addresses": (0x04000088, 0x04000089), "scope": "setup", "required": True},
        {"name": "fifo_a_word", "kind": "WRITE", "starts": (0x040000A0,), "width": 4, "scope": "measured", "required": False},
        {"name": "fifo_b_word", "kind": "WRITE", "starts": (0x040000A4,), "width": 4, "scope": "measured", "required": False},
        {"name": "timer_0", "kind": "TIMER", "timer_ids": (0,), "scope": "measured", "required": False},
        {"name": "timer_1", "kind": "TIMER", "timer_ids": (1,), "scope": "measured", "required": False},
    ),
}


class InfrastructureFailure(Exception):
    """A matrix tool or its published diagnostics were unusable."""


def parse_args(argv: list[str], tool_dir: Path) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--decomp", required=True, type=Path, help="compiled decomp project root")
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--validator", type=Path, default=tool_dir / "validate_driver.py")
    parser.add_argument("--record-voice", type=Path)
    parser.add_argument("--candidate-trace", type=Path)
    parser.add_argument("--mgba-replay", type=Path)
    parser.add_argument("--pory-replay", type=Path)
    parser.add_argument("--driver-compare", type=Path)
    parser.add_argument("--native-compare", type=Path)
    return parser.parse_args(argv)


def resolved(path: Path) -> Path:
    try:
        return path.resolve(strict=False)
    except OSError as error:
        raise InfrastructureFailure(f"cannot resolve {path}: {error}") from error


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as source:
            for block in iter(lambda: source.read(1 << 20), b""):
                digest.update(block)
    except OSError as error:
        raise InfrastructureFailure(f"cannot hash {path}: {error}") from error
    return digest.hexdigest()


def load_json(path: Path, label: str) -> dict[str, Any]:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise InfrastructureFailure(f"cannot load {label} {path}: {error}") from error
    if not isinstance(document, dict):
        raise InfrastructureFailure(f"{label} {path} must contain a JSON object")
    return document


def write_json(path: Path, document: dict[str, Any]) -> None:
    try:
        path.write_text(json.dumps(document, indent=2, sort_keys=True, allow_nan=False) + "\n", encoding="utf-8")
    except (OSError, TypeError, ValueError) as error:
        raise InfrastructureFailure(f"cannot write {path}: {error}") from error


def stable_argument(argument: str, stage: Path) -> str:
    stage_text = str(stage)
    if argument == stage_text or argument.startswith(stage_text + os.sep):
        return "{output-dir}" + argument[len(stage_text) :]
    return argument


def validator_argv(args: argparse.Namespace, decomp: Path, case: dict[str, Any], run_output: Path) -> list[str]:
    command = [
        sys.executable,
        str(args.validator),
        "--decomp",
        str(decomp),
        "--voicegroup",
        case["voicegroup"],
        "--voice",
        str(case["voice"]),
        "--scenario",
        case["scenario"],
        "--output-dir",
        str(run_output),
    ]
    for name in ("note", "velocity", "volume", "pan"):
        command.extend((f"--{name}", str(case[name])))
    for name in ("record_voice", "candidate_trace", "mgba_replay", "pory_replay", "driver_compare", "native_compare"):
        executable = getattr(args, name)
        if executable is not None:
            command.extend((f"--{name.replace('_', '-')}", str(executable)))
    return command


def write_process_output(path: Path, data: bytes, label: str) -> None:
    try:
        path.write_bytes(data)
    except OSError as error:
        raise InfrastructureFailure(f"cannot write {label}: {error}") from error


def produced_directory(run_output: Path) -> Path | None:
    if run_output.is_dir():
        return run_output
    failed = run_output.with_name(f"{run_output.name}.failed")
    return failed if failed.is_dir() else None


def run_diagnostics(directory: Path | None) -> tuple[dict[str, Any], list[str]]:
    diagnostics: dict[str, Any] = {
        "first_transaction_divergence": None,
        "first_native_sample_divergence": None,
    }
    if directory is None:
        return diagnostics, []
    errors: list[str] = []
    manifest_path = directory / "manifest.json"
    if manifest_path.is_file():
        try:
            published = load_json(manifest_path, "validation manifest").get("diagnostics")
            if isinstance(published, dict):
                diagnostics["first_transaction_divergence"] = published.get("first_transaction_divergence")
                divergences = published.get("first_sample_divergences")
                if isinstance(divergences, dict):
                    for name in ("reference_native", "reference_hardware", "candidate_hardware"):
                        if divergences.get(name) is not None:
                            diagnostics["first_native_sample_divergence"] = {
                                "comparison": name,
                                "mismatch": divergences[name],
                            }
                            return diagnostics, errors
                return diagnostics, errors
        except InfrastructureFailure as error:
            errors.append(str(error))
    comparator_path = directory / "driver-compare.json"
    if comparator_path.is_file():
        try:
            diagnostics["first_transaction_divergence"] = load_json(
                comparator_path, "driver comparator result"
            ).get("first_divergence")
        except InfrastructureFailure as error:
            errors.append(str(error))
    for name in (
        "reference-native-compare.json",
        "reference-hardware-compare.json",
        "candidate-hardware-compare.json",
    ):
        path = directory / name
        if not path.is_file():
            continue
        try:
            mismatch = load_json(path, name).get("first_mismatch")
        except InfrastructureFailure as error:
            errors.append(str(error))
            continue
        if mismatch is not None:
            diagnostics["first_native_sample_divergence"] = {
                "comparison": name.removesuffix(".json"),
                "mismatch": mismatch,
            }
            break
    return diagnostics, errors


def gate_values(directory: Path | None) -> tuple[dict[str, bool | None], list[str]]:
    gates: dict[str, bool | None] = {name: None for name in REQUIRED_GATES}
    errors: list[str] = []
    if directory is None:
        return gates, ["validator did not publish an output or failure directory"]

    manifest_path = directory / "manifest.json"
    if manifest_path.is_file():
        try:
            manifest = load_json(manifest_path, "validation manifest")
            published = manifest.get("gates")
            if not isinstance(published, dict):
                errors.append("validation manifest gates must be an object")
            else:
                for name in REQUIRED_GATES:
                    value = published.get(name)
                    if not isinstance(value, bool):
                        errors.append(f"validation manifest gate {name} must be boolean")
                    else:
                        gates[name] = value
        except InfrastructureFailure as error:
            errors.append(str(error))
        return gates, errors

    comparator_path = directory / "driver-compare.json"
    if comparator_path.is_file():
        try:
            comparator = load_json(comparator_path, "driver comparator result")
            for name in REQUIRED_GATES[:3]:
                value = comparator.get(name)
                if not isinstance(value, bool):
                    errors.append(f"driver comparator gate {name} must be boolean")
                else:
                    gates[name] = value
        except InfrastructureFailure as error:
            errors.append(str(error))
    for gate_name, result_name in (
        ("reference_native_exact", "reference-native-compare.json"),
        ("reference_hardware_exact", "reference-hardware-compare.json"),
        ("candidate_hardware_exact", "candidate-hardware-compare.json"),
    ):
        result_path = directory / result_name
        if not result_path.is_file():
            continue
        try:
            result = load_json(result_path, result_name)
            value = result.get("passed")
            if not isinstance(value, bool):
                errors.append(f"{result_name}.passed must be boolean")
            else:
                gates[gate_name] = value
        except InfrastructureFailure as error:
            errors.append(str(error))
    return gates, errors


def capture_hashes(directory: Path | None) -> tuple[dict[str, str | None], list[str]]:
    hashes: dict[str, str | None] = {}
    errors: list[str] = []
    for name in CAPTURE_ARTIFACTS:
        path = directory / name if directory is not None else None
        if path is None or not path.is_file():
            hashes[name] = None
            errors.append(f"missing capture artifact {name}")
            continue
        try:
            hashes[name] = sha256_file(path)
        except InfrastructureFailure as error:
            hashes[name] = None
            errors.append(str(error))
    return hashes, errors

def canonical_manifest_hash(directory: Path | None) -> str | None:
    path = directory / CANONICAL_MANIFEST if directory is not None else None
    if path is None or not path.is_file():
        return None
    return sha256_file(path)


def fixture_identity(directory: Path | None) -> tuple[dict[str, Any] | None, list[str]]:
    if directory is None:
        return None, ["missing validation directory for fixture identity"]
    documents = (
        ("reference", directory / "reference-native.json"),
        ("candidate", directory / "candidate.trace.manifest.json"),
    )
    identities: dict[str, dict[str, Any]] = {}
    errors: list[str] = []
    for source, path in documents:
        try:
            document = load_json(path, f"{source} fixture manifest")
            identity = {field: document[field] for field in IDENTITY_FIELDS}
        except (InfrastructureFailure, KeyError) as error:
            errors.append(f"cannot read {source} fixture identity: {error}")
            continue
        if not isinstance(identity["family"], str) or not identity["family"]:
            errors.append(f"{source} fixture family must be a non-empty string")
        if not isinstance(identity["resolved_type"], int) or isinstance(identity["resolved_type"], bool):
            errors.append(f"{source} fixture resolved_type must be an integer")
        for field in ("tone_data_sha256", "family_payload_sha256"):
            value = identity[field]
            if not isinstance(value, str) or len(value) != 64 or any(character not in "0123456789abcdef" for character in value):
                errors.append(f"{source} fixture {field} must be a lowercase SHA-256 string")
        identities[source] = identity
    exact = len(identities) == 2 and identities["reference"] == identities["candidate"]
    return {"reference": identities.get("reference"), "candidate": identities.get("candidate"), "exact": exact}, errors


def run_once(command: list[str], run_output: Path, log_prefix: Path, stage: Path) -> dict[str, Any]:
    try:
        process = subprocess.run(command, check=False, capture_output=True)
    except OSError as error:
        return {
            "returncode": 2,
            "directory": None,
            "gates": {name: None for name in REQUIRED_GATES},
            "capture_hashes": {name: None for name in CAPTURE_ARTIFACTS},
            "canonical_manifest_sha256": None,
            "fixture_identity": None,
            "diagnostics": {
                "first_transaction_divergence": None,
                "first_native_sample_divergence": None,
            },
            "errors": [f"cannot run validator: {error}"],
        }
    write_process_output(log_prefix.with_suffix(".stdout"), process.stdout, f"{log_prefix.name} stdout")
    write_process_output(log_prefix.with_suffix(".stderr"), process.stderr, f"{log_prefix.name} stderr")
    directory = produced_directory(run_output)
    gates, gate_errors = gate_values(directory)
    hashes, hash_errors = capture_hashes(directory)
    try:
        manifest_hash = canonical_manifest_hash(directory)
    except InfrastructureFailure as error:
        manifest_hash = None
        hash_errors.append(str(error))
    identity, identity_errors = fixture_identity(directory)
    diagnostics, diagnostic_errors = run_diagnostics(directory)
    return {
        "returncode": process.returncode,
        "directory": None if directory is None else str(directory.relative_to(stage)),
        "gates": gates,
        "capture_hashes": hashes,
        "canonical_manifest_sha256": manifest_hash,
        "fixture_identity": identity,
        "diagnostics": diagnostics,
        "errors": gate_errors + hash_errors + identity_errors + diagnostic_errors,
    }


def position_in_measured_region(record: Any, begin: Any, end: Any) -> bool:
    return (begin.cycle, begin.order) < (record.cycle, record.order) < (end.cycle, end.order)


def record_matches(record: Any, event_class: dict[str, Any], begin: Any, end: Any) -> bool:
    if record.kind != event_class["kind"]:
        return False
    if event_class["scope"] == "measured" and not position_in_measured_region(record, begin, end):
        return False
    if record.kind == "TIMER":
        return record.value in event_class.get("timer_ids", ())
    if record.kind == "SAMPLE":
        return True
    if record.kind != "WRITE":
        return False
    if "width" in event_class and record.width != event_class["width"]:
        return False
    if "starts" in event_class:
        return record.address in event_class["starts"]
    return any(record.address <= address < record.address + record.width for address in event_class["addresses"])


def collect_trace_coverage(
    path: Path,
    family: str,
    case_run: str,
    coverage: dict[str, set[str]],
) -> str | None:
    try:
        records = parse_trace(path)
        begin = next(record for record in records if record.kind == "BEGIN")
        end = next(record for record in records if record.kind == "END")
    except (OSError, StopIteration, TraceError) as error:
        return str(error)
    for record in records:
        for event_class in EVENT_CLASSES[family]:
            if record_matches(record, event_class, begin, end):
                coverage[event_class["name"]].add(case_run)
    return None


def coverage_report(case_reports: list[dict[str, Any]], stage: Path) -> dict[str, Any]:
    source_reports: dict[str, Any] = {}
    for source, trace_name in (("reference", "reference.trace"), ("candidate", "candidate.trace")):
        family_reports: dict[str, Any] = {}
        source_errors: list[dict[str, str]] = []
        for family in EVENT_CLASSES:
            coverage = {event_class["name"]: set() for event_class in EVENT_CLASSES[family]}
            for case in (case for case in case_reports if case["fixture"]["family"] == family):
                for run_name in ("first", "repeat"):
                    output = case["runs"][run_name]["directory"]
                    if output is None:
                        source_errors.append({"case": case["id"], "run": run_name, "error": "missing validation directory"})
                        continue
                    trace_error = collect_trace_coverage(
                        stage / output / trace_name,
                        family,
                        f"{case['id']}/{run_name}",
                        coverage,
                    )
                    if trace_error is not None:
                        source_errors.append({"case": case["id"], "run": run_name, "error": trace_error})
            events = []
            for event_class in EVENT_CLASSES[family]:
                case_runs = sorted(coverage[event_class["name"]])
                events.append(
                    {
                        "name": event_class["name"],
                        "kind": event_class["kind"],
                        "scope": event_class["scope"],
                        "required": event_class["required"],
                        "case_runs": case_runs,
                        "cases": sorted({case_run.rsplit("/", 1)[0] for case_run in case_runs}),
                        "exercised": bool(case_runs),
                    }
                )
            unexercised_required = [event["name"] for event in events if event["required"] and not event["exercised"]]
            family_reports[family] = {
                "required_event_coverage": events,
                "unexercised_required_event_classes": unexercised_required,
                "complete": not unexercised_required,
            }
        source_reports[source] = {
            "families": family_reports,
            "complete": not source_errors and all(report["complete"] for report in family_reports.values()),
            "errors": source_errors,
        }
    return {"sources": source_reports, "complete": all(report["complete"] for report in source_reports.values())}


def all_gates_pass(gates: dict[str, bool | None]) -> bool:
    return all(gates[name] is True for name in REQUIRED_GATES)


def combined_gates(first: dict[str, bool | None], repeat: dict[str, bool | None]) -> dict[str, dict[str, bool | None]]:
    return {
        name: {"first": first[name], "repeat": repeat[name], "passed": first[name] is True and repeat[name] is True}
        for name in REQUIRED_GATES
    }


def runs_are_deterministic(first: dict[str, Any], repeat: dict[str, Any]) -> bool:
    return (
        first["canonical_manifest_sha256"] is not None
        and first["canonical_manifest_sha256"] == repeat["canonical_manifest_sha256"]
        and all(
            first["capture_hashes"][name] is not None
            and first["capture_hashes"][name] == repeat["capture_hashes"][name]
            for name in CAPTURE_ARTIFACTS
        )
    )


def expected_identity(case: dict[str, Any]) -> dict[str, Any]:
    return {"family": case["family"], "resolved_type": case["resolved_type"]}


def identity_matches(identity: dict[str, Any] | None, expected: dict[str, Any]) -> bool:
    if identity is None or not identity["exact"]:
        return False
    reference = identity["reference"]
    return reference is not None and all(reference[field] == value for field, value in expected.items())


def case_report(args: argparse.Namespace, decomp: Path, stage: Path, case: dict[str, Any]) -> dict[str, Any]:
    case_directory = stage / "cases" / case["id"]
    case_directory.mkdir(parents=True)
    first_output = case_directory / "first"
    repeat_output = case_directory / "repeat"
    first_command = validator_argv(args, decomp, case, first_output)
    repeat_command = validator_argv(args, decomp, case, repeat_output)
    first = run_once(first_command, first_output, case_directory / "first", stage)
    repeat = run_once(repeat_command, repeat_output, case_directory / "repeat", stage)
    expected = expected_identity(case)
    identity_exact = identity_matches(first["fixture_identity"], expected) and identity_matches(repeat["fixture_identity"], expected)
    deterministic = runs_are_deterministic(first, repeat)
    gates = combined_gates(first["gates"], repeat["gates"])
    passed = (
        first["returncode"] == 0
        and repeat["returncode"] == 0
        and all_gates_pass(first["gates"])
        and all_gates_pass(repeat["gates"])
        and deterministic
        and identity_exact
    )
    return {
        "id": case["id"],
        "fixture": {
            "kind": case["kind"],
            "family": case["family"],
            "voicegroup_symbol": case["voicegroup"],
            "voice_index": case["voice"],
            "resolved_type": case["resolved_type"],
        },
        "scenario": case["scenario"],
        "scenario_timing": SCENARIO_CONTRACTS[case["scenario"]],
        "controls": {name: case[name] for name in ("note", "velocity", "volume", "pan")},
        "commands": {
            "first": [stable_argument(argument, stage) for argument in first_command],
            "repeat": [stable_argument(argument, stage) for argument in repeat_command],
        },
        "runs": {"first": first, "repeat": repeat},
        "gates": gates,
        "fixture_identity": {"expected": expected, "first": first["fixture_identity"], "repeat": repeat["fixture_identity"], "exact": identity_exact},
        "deterministic": deterministic,
        "passed": passed,
    }


def has_infrastructure_failure(case_reports: list[dict[str, Any]]) -> bool:
    return any(
        run["returncode"] not in (0, 1) or run["errors"]
        for case in case_reports
        for run in case["runs"].values()
    )


def publish(stage: Path, destination: Path) -> None:
    try:
        publish_directory_no_replace(stage, destination)
    except PublishError as error:
        raise InfrastructureFailure(str(error)) from error


def failed_publication(stage: Path | None, output: Path | None, message: str) -> int:
    print(f"validate_driver_matrix: {message}", file=sys.stderr)
    if stage is not None and stage.exists() and output is not None:
        try:
            publish(stage, output.with_name(f"{output.name}.failed"))
        except InfrastructureFailure as error:
            print(f"validate_driver_matrix: {error}; staged diagnostics remain at {stage}", file=sys.stderr)
    return 2


def main(argv: list[str] | None = None) -> int:
    tool_dir = Path(__file__).resolve().parent
    args = parse_args(sys.argv[1:] if argv is None else argv, tool_dir)
    stage: Path | None = None
    output: Path | None = None
    try:
        decomp = resolved(args.decomp)
        output = resolved(args.output_dir)
        args.validator = resolved(args.validator)
        if not decomp.is_dir():
            raise InfrastructureFailure(f"--decomp is not a directory: {decomp}")
        if not output.parent.is_dir():
            raise InfrastructureFailure(f"output parent is not a directory: {output.parent}")
        require_regular_file(args.validator, "validator")
        for option_name in ("record_voice", "candidate_trace", "mgba_replay", "pory_replay", "driver_compare", "native_compare"):
            executable = getattr(args, option_name)
            if executable is not None:
                executable = resolved(executable)
                require_regular_file(executable, option_name.replace("_", " "), executable=True)
                setattr(args, option_name, executable)
        failed = output.with_name(f"{output.name}.failed")
        if output.exists() or failed.exists():
            raise InfrastructureFailure(f"output or failure directory already exists: {output} / {failed}")
        stage = output.parent / f".{output.name}.matrix.stage"
        if stage.exists():
            raise InfrastructureFailure(f"staging directory already exists: {stage}")
        stage.mkdir(mode=0o700)

        reports = [case_report(args, decomp, stage, case) for case in MATRIX_CASES]
        coverage = coverage_report(reports, stage)
        passing = [case for case in reports if case["passed"]]
        matrix_exact = len(passing) == len(reports) and coverage["complete"]
        report = {
            "format": "poryaaaa-driver-lifecycle-matrix",
            "version": 1,
            "case_count": len(reports),
            "passed_case_count": len(passing),
            "failed_case_count": len(reports) - len(passing),
            "matrix_exact": matrix_exact,
            "cases": reports,
            "coverage": coverage,
        }
        write_json(stage / "matrix_report.json", report)
        if matrix_exact:
            publish(stage, output)
            return 0
        publish(stage, failed)
        return 2 if has_infrastructure_failure(reports) else 1
    except InfrastructureFailure as error:
        return failed_publication(stage, output, str(error))
    except Exception as error:
        return failed_publication(stage, output, f"unexpected failure: {error}")


if __name__ == "__main__":
    sys.exit(main())
