#!/usr/bin/env python3
"""Run the fixed programmable-wave lifecycle matrix and publish one aggregate report."""

import argparse
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any

from psw_compare import TraceError, parse_trace
from validate_psw import SCENARIO_CONTRACTS, require_regular_file


REQUIRED_GATES = (
    "transaction_exact",
    "wave_payload_exact",
    "logical_state_exact",
    "reference_native_exact",
    "reference_hardware_exact",
    "candidate_hardware_exact",
)
CAPTURE_ARTIFACTS = (
    "reference.trace",
    "reference-native.pcm",
    "reference-native.cycles",
    "reference-native.json",
    "candidate.trace",
    "candidate.trace.manifest.json",
)
RETAINED_REGISTERS = {
    "NR30": (0x04000070, 0x04000071),
    "NR31": (0x04000072,),
    "NR32": (0x04000073,),
    "NR33": (0x04000074,),
    "NR34": (0x04000075,),
    "NR50": (0x04000080,),
    "NR51": (0x04000081,),
}
WAVE_BASE = 0x04000090

# These inputs are deliberately fixed to loader-valid, non-symmetric PSW fixtures.
# All normal cases use aa_girl slot 4; alternate cases use poke_center slot 1.
MATRIX_CASES = (
    {
        "id": "start-normal",
        "fixture_kind": "normal",
        "voicegroup": "voicegroup_aa_girl",
        "voice": 4,
        "scenario": "start",
        "note": 36,
        "velocity": 127,
        "volume": 127,
        "pan": 0,
    },
    {
        "id": "envelope-normal",
        "fixture_kind": "normal",
        "voicegroup": "voicegroup_aa_girl",
        "voice": 4,
        "scenario": "envelope",
        "note": 60,
        "velocity": 127,
        "volume": 127,
        "pan": 64,
    },
    {
        "id": "pitch-normal",
        "fixture_kind": "normal",
        "voicegroup": "voicegroup_aa_girl",
        "voice": 4,
        "scenario": "pitch",
        "note": 84,
        "velocity": 127,
        "volume": 127,
        "pan": 127,
    },
    {
        "id": "volume-pan-normal",
        "fixture_kind": "normal",
        "voicegroup": "voicegroup_aa_girl",
        "voice": 4,
        "scenario": "volume-pan",
        "note": 36,
        "velocity": 127,
        "volume": 127,
        "pan": 64,
    },
    {
        "id": "retrigger-normal",
        "fixture_kind": "normal",
        "voicegroup": "voicegroup_aa_girl",
        "voice": 4,
        "scenario": "retrigger",
        "note": 60,
        "velocity": 127,
        "volume": 127,
        "pan": 127,
    },
    {
        "id": "release-normal",
        "fixture_kind": "normal",
        "voicegroup": "voicegroup_aa_girl",
        "voice": 4,
        "scenario": "release",
        "note": 84,
        "velocity": 127,
        "volume": 127,
        "pan": 0,
    },
    {
        "id": "start-alternate",
        "fixture_kind": "alternate",
        "voicegroup": "voicegroup_poke_center",
        "voice": 1,
        "scenario": "start",
        "note": 36,
        "velocity": 127,
        "volume": 127,
        "pan": 64,
    },
    {
        "id": "envelope-alternate",
        "fixture_kind": "alternate",
        "voicegroup": "voicegroup_poke_center",
        "voice": 1,
        "scenario": "envelope",
        "note": 60,
        "velocity": 127,
        "volume": 127,
        "pan": 127,
    },
    {
        "id": "pitch-alternate",
        "fixture_kind": "alternate",
        "voicegroup": "voicegroup_poke_center",
        "voice": 1,
        "scenario": "pitch",
        "note": 84,
        "velocity": 127,
        "volume": 127,
        "pan": 0,
    },
    {
        "id": "volume-pan-alternate",
        "fixture_kind": "alternate",
        "voicegroup": "voicegroup_poke_center",
        "voice": 1,
        "scenario": "volume-pan",
        "note": 36,
        "velocity": 127,
        "volume": 127,
        "pan": 127,
    },
    {
        "id": "retrigger-alternate",
        "fixture_kind": "alternate",
        "voicegroup": "voicegroup_poke_center",
        "voice": 1,
        "scenario": "retrigger",
        "note": 60,
        "velocity": 127,
        "volume": 127,
        "pan": 0,
    },
    {
        "id": "release-alternate",
        "fixture_kind": "alternate",
        "voicegroup": "voicegroup_poke_center",
        "voice": 1,
        "scenario": "release",
        "note": 84,
        "velocity": 127,
        "volume": 127,
        "pan": 64,
    },
)


class InfrastructureFailure(Exception):
    """A matrix tool or its published diagnostics were unusable."""


def parse_args(argv: list[str], tool_dir: Path) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--decomp", required=True, type=Path, help="compiled decomp project root")
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--validator", type=Path, default=tool_dir / "validate_psw.py")
    parser.add_argument("--record-voice", type=Path)
    parser.add_argument("--candidate-trace", type=Path)
    parser.add_argument("--mgba-replay", type=Path)
    parser.add_argument("--pory-replay", type=Path)
    parser.add_argument("--psw-compare", type=Path)
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


def validator_argv(args: argparse.Namespace, decomp: Path, stage: Path, case: dict[str, Any], run_output: Path) -> list[str]:
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
    for name in ("record_voice", "candidate_trace", "mgba_replay", "pory_replay", "psw_compare", "native_compare"):
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

    comparator_path = directory / "psw-compare.json"
    if comparator_path.is_file():
        try:
            comparator = load_json(comparator_path, "PSW comparator result")
            for name in REQUIRED_GATES[:3]:
                value = comparator.get(name)
                if not isinstance(value, bool):
                    errors.append(f"PSW comparator gate {name} must be boolean")
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


def run_once(command: list[str], run_output: Path, log_prefix: Path) -> dict[str, Any]:
    try:
        process = subprocess.run(command, check=False, capture_output=True)
    except OSError as error:
        return {
            "returncode": 2,
            "directory": None,
            "gates": {name: None for name in REQUIRED_GATES},
            "capture_hashes": {name: None for name in CAPTURE_ARTIFACTS},
            "errors": [f"cannot run validator: {error}"],
        }
    write_process_output(log_prefix.with_suffix(".stdout"), process.stdout, f"{log_prefix.name} stdout")
    write_process_output(log_prefix.with_suffix(".stderr"), process.stderr, f"{log_prefix.name} stderr")
    directory = produced_directory(run_output)
    gates, gate_errors = gate_values(directory)
    hashes, hash_errors = capture_hashes(directory)
    errors = gate_errors + hash_errors
    return {
        "returncode": process.returncode,
        "directory": None if directory is None else str(directory.relative_to(log_prefix.parents[2])),
        "gates": gates,
        "capture_hashes": hashes,
        "errors": errors,
    }


def collect_trace_coverage(
    path: Path,
    case_id: str,
    registers: dict[str, set[str]],
    wave_bytes: dict[int, set[str]],
) -> tuple[str | None, list[int], bytes | None]:
    try:
        records = parse_trace(path)
        begin = next(record for record in records if record.kind == "BEGIN")
        end = next(record for record in records if record.kind == "END")
    except (OSError, StopIteration, TraceError) as error:
        return str(error), [], None

    nr32_values: list[int] = []
    waveform: dict[int, int] = {}
    begin_position = (begin.cycle, begin.order)
    end_position = (end.cycle, end.order)
    for record in records:
        if record.kind != "WRITE" or not begin_position < (record.cycle, record.order) < end_position:
            continue
        for offset in range(record.width):
            address = record.address + offset
            value = (record.value >> (offset * 8)) & 0xFF
            for name, addresses in RETAINED_REGISTERS.items():
                if address in addresses:
                    registers[name].add(case_id)
                    if name == "NR32":
                        nr32_values.append(value)
                    break
            else:
                if WAVE_BASE <= address < WAVE_BASE + 16:
                    wave_offset = address - WAVE_BASE
                    wave_bytes[wave_offset].add(case_id)
                    waveform[wave_offset] = value
    payload = bytes(waveform[offset] for offset in range(16)) if len(waveform) == 16 else None
    return None, nr32_values, payload


def has_ordered_values(values: list[int], expected: tuple[int, ...]) -> bool:
    """Require the expected register values to occur in trace order."""
    expected_offset = 0
    for value in values:
        if value == expected[expected_offset]:
            expected_offset += 1
            if expected_offset == len(expected):
                return True
    return False


def payload_report(samples: list[dict[str, Any]]) -> dict[str, Any]:
    """Summarize one fixture's complete, stable waveform evidence."""
    payloads = {sample["payload_hex"] for sample in samples if sample["payload_hex"] is not None}
    payload = next(iter(payloads)) if len(payloads) == 1 else None
    payload_bytes = bytes.fromhex(payload) if payload is not None else None
    complete = len(payloads) == 1 and all(sample["payload_hex"] is not None for sample in samples)
    byte_order_non_symmetric = payload_bytes is not None and payload_bytes != payload_bytes[::-1]
    nibble_non_symmetric = payload_bytes is not None and any(
        (value >> 4) != (value & 0x0F) for value in payload_bytes
    )
    return {
        "case_runs": samples,
        "payload_hex": payload,
        "complete": complete,
        "byte_order_non_symmetric": byte_order_non_symmetric,
        "nibble_non_symmetric": nibble_non_symmetric,
        "non_symmetric": byte_order_non_symmetric and nibble_non_symmetric,
    }


def coverage_report(case_reports: list[dict[str, Any]], stage: Path) -> dict[str, Any]:
    source_reports: dict[str, Any] = {}
    for source, trace_name in (("reference", "reference.trace"), ("candidate", "candidate.trace")):
        registers = {name: set() for name in RETAINED_REGISTERS}
        wave_bytes = {offset: set() for offset in range(16)}
        errors: list[dict[str, str]] = []
        fixture_samples: dict[str, list[dict[str, Any]]] = {"normal": [], "alternate": []}
        alternate_envelope_nr32: list[dict[str, Any]] = []
        for case in case_reports:
            for run_name in ("first", "repeat"):
                output = case["runs"][run_name]["directory"]
                trace_error: str | None = None
                nr32_values: list[int] = []
                payload: bytes | None = None
                if output is None:
                    trace_error = "missing validation directory"
                else:
                    trace = stage / output / trace_name
                    trace_error, nr32_values, payload = collect_trace_coverage(
                        trace,
                        f"{case['id']}/{run_name}",
                        registers,
                        wave_bytes,
                    )
                if trace_error is not None:
                    errors.append({"case": case["id"], "run": run_name, "error": trace_error})
                fixture_samples[case["fixture"]["kind"]].append(
                    {
                        "case": case["id"],
                        "run": run_name,
                        "payload_hex": None if payload is None else payload.hex(),
                    }
                )
                if case["id"] == "envelope-alternate":
                    alternate_envelope_nr32.append(
                        {
                            "case": case["id"],
                            "run": run_name,
                            "values": [f"0x{value:02x}" for value in nr32_values],
                            "transition_20_to_80": has_ordered_values(nr32_values, (0x20, 0x80)),
                        }
                    )
        retained = [
            {
                "name": name,
                "addresses": [f"0x{address:08x}" for address in addresses],
                "case_runs": sorted(registers[name]),
                "cases": sorted({case_run.rsplit("/", 1)[0] for case_run in registers[name]}),
                "exercised": bool(registers[name]),
            }
            for name, addresses in RETAINED_REGISTERS.items()
        ]
        wave = [
            {
                "offset": offset,
                "address": f"0x{WAVE_BASE + offset:08x}",
                "case_runs": sorted(wave_bytes[offset]),
                "cases": sorted({case_run.rsplit("/", 1)[0] for case_run in wave_bytes[offset]}),
                "exercised": bool(wave_bytes[offset]),
            }
            for offset in range(16)
        ]
        fixture_payloads = {kind: payload_report(samples) for kind, samples in fixture_samples.items()}
        payloads_distinct = (
            fixture_payloads["normal"]["payload_hex"] is not None
            and fixture_payloads["alternate"]["payload_hex"] is not None
            and fixture_payloads["normal"]["payload_hex"] != fixture_payloads["alternate"]["payload_hex"]
        )
        nr32_complete = len(alternate_envelope_nr32) == 2 and all(
            item["transition_20_to_80"] for item in alternate_envelope_nr32
        )
        payloads_complete = payloads_distinct and all(
            item["complete"] and item["non_symmetric"] for item in fixture_payloads.values()
        )
        complete = (
            not errors
            and all(item["exercised"] for item in retained + wave)
            and nr32_complete
            and payloads_complete
        )
        source_reports[source] = {
            "retained_register_coverage": retained,
            "wave_byte_coverage": wave,
            "alternate_envelope_nr32": {
                "case_runs": alternate_envelope_nr32,
                "complete": nr32_complete,
            },
            "fixture_wave_payloads": fixture_payloads,
            "fixture_wave_payloads_distinct": payloads_distinct,
            "complete": complete,
            "errors": errors,
        }
    return {"sources": source_reports, "complete": all(report["complete"] for report in source_reports.values())}


def all_gates_pass(gates: dict[str, bool | None]) -> bool:
    return all(gates[name] is True for name in REQUIRED_GATES)


def combined_gates(first: dict[str, bool | None], repeat: dict[str, bool | None]) -> dict[str, dict[str, bool | None]]:
    return {
        name: {"first": first[name], "repeat": repeat[name], "passed": first[name] is True and repeat[name] is True}
        for name in REQUIRED_GATES
    }


def runs_are_deterministic(first: dict[str, str | None], repeat: dict[str, str | None]) -> bool:
    return all(first[name] is not None and first[name] == repeat[name] for name in CAPTURE_ARTIFACTS)


def case_report(args: argparse.Namespace, decomp: Path, stage: Path, case: dict[str, Any]) -> dict[str, Any]:
    case_directory = stage / "cases" / case["id"]
    case_directory.mkdir(parents=True)
    first_output = case_directory / "first"
    repeat_output = case_directory / "repeat"
    first_command = validator_argv(args, decomp, stage, case, first_output)
    repeat_command = validator_argv(args, decomp, stage, case, repeat_output)
    first = run_once(first_command, first_output, case_directory / "first")
    repeat = run_once(repeat_command, repeat_output, case_directory / "repeat")
    deterministic = runs_are_deterministic(first["capture_hashes"], repeat["capture_hashes"])
    gates = combined_gates(first["gates"], repeat["gates"])
    passed = (
        first["returncode"] == 0
        and repeat["returncode"] == 0
        and all_gates_pass(first["gates"])
        and all_gates_pass(repeat["gates"])
        and deterministic
    )
    return {
        "id": case["id"],
        "fixture": {
            "kind": case["fixture_kind"],
            "voicegroup_symbol": case["voicegroup"],
            "voice_index": case["voice"],
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
        "deterministic": deterministic,
        "passed": passed,
    }


def has_infrastructure_failure(case_reports: list[dict[str, Any]]) -> bool:
    for case in case_reports:
        for run in case["runs"].values():
            if run["returncode"] not in (0, 1) or run["errors"]:
                return True
    return False


def publish(stage: Path, destination: Path) -> None:
    if destination.exists():
        raise InfrastructureFailure(f"refusing to overwrite existing output: {destination}")
    try:
        stage.rename(destination)
    except OSError as error:
        raise InfrastructureFailure(f"cannot publish {destination}: {error}") from error


def failed_publication(stage: Path | None, output: Path | None, message: str) -> int:
    print(f"validate_psw_matrix: {message}", file=sys.stderr)
    if stage is not None and stage.exists() and output is not None:
        try:
            publish(stage, output.with_name(f"{output.name}.failed"))
        except InfrastructureFailure as error:
            print(f"validate_psw_matrix: {error}; staged diagnostics remain at {stage}", file=sys.stderr)
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
        for option_name in ("record_voice", "candidate_trace", "mgba_replay", "pory_replay", "psw_compare", "native_compare"):
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
            "format": "poryaaaa-psw-lifecycle-matrix",
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
