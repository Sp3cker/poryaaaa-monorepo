#!/usr/bin/env python3
"""Publish one fail-closed fixed-driver lifecycle validation run."""

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Any

from atomic_publish import PublishError, publish_directory_no_replace

PINNED_MGBA_REVISION = "afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9"
CLOCK_HZ = 16_777_216
SHA256_RE = re.compile(r"[0-9a-f]{64}\Z")
VOICEGROUP_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*\Z")

# The family is always resolved from the independently captured ToneData type.
FAMILY_BY_TYPE = {
    0x00: "directsound",
    0x01: "sq1",
    0x02: "sq2",
    0x03: "psw",
    0x0B: "psw",
}
FAMILY_SPECS = {
    "directsound": {"solo": "directsound", "solo_mask": 0x30},
    "sq1": {"solo": "sq1", "solo_mask": 0x01},
    "sq2": {"solo": "sq2", "solo_mask": 0x02},
    "psw": {"solo": "wave", "solo_mask": 0x04},
}
DRIVER_ORIGIN_CYCLES = {
    "directsound": 1005,
    "sq1": 0,
    "sq2": 0,
    "psw": 0,
}
SCENARIO_CONTRACTS = {
    "start": {
        "logical_vblanks": 1,
        "capture_frames": 9,
        "span_cycles": 2_536_960,
        "high_level_action": "note-on at tick 0",
    },
    "envelope": {
        "logical_vblanks": 6,
        "capture_frames": 15,
        "span_cycles": 4_222_464,
        "high_level_action": "note-on at tick 0; sustain through tick 6",
    },
    "pitch": {
        "logical_vblanks": 4,
        "capture_frames": 12,
        "span_cycles": 3_379_712,
        "high_level_action": "note-on at tick 0; pitch bend +16 at tick 2; sustain through tick 4",
    },
    "volume-pan": {
        "logical_vblanks": 4,
        "capture_frames": 12,
        "span_cycles": 3_379_712,
        "high_level_action": "note-on at tick 0; volume 32 and pan 127 at tick 2; sustain through tick 4",
    },
    "retrigger": {
        "logical_vblanks": 5,
        "capture_frames": 15,
        "span_cycles": 4_222_464,
        "high_level_action": "note-on at tick 0; note-off at tick 2; note-on at tick 3; sustain through tick 5",
    },
    "release": {
        "logical_vblanks": 6,
        "capture_frames": 14,
        "span_cycles": 3_941_376,
        "high_level_action": "note-on at tick 0; note-off at tick 2; release through tick 6",
    },
}
REQUIRED_GATES = (
    "transaction_exact",
    "payload_exact",
    "logical_state_exact",
    "reference_native_exact",
    "reference_hardware_exact",
    "candidate_hardware_exact",
)


class InfrastructureFailure(Exception):
    """A command or its declared artifact was unusable."""


class GateFailure(Exception):
    """A complete run produced evidence that failed a required gate."""


def decimal_value(text: str, *, upper: int | None, label: str) -> int:
    """Accept only the decimal syntax understood by both capture adapters."""
    if not text.isascii() or not text.isdecimal():
        raise argparse.ArgumentTypeError(f"{label} must be a decimal integer")
    value = int(text)
    if upper is not None and value > upper:
        raise argparse.ArgumentTypeError(f"{label} must be from 0 through {upper}")
    return value


def control_value(text: str) -> int:
    return decimal_value(text, upper=127, label="control")


def voice_index(text: str) -> int:
    return decimal_value(text, upper=None, label="voice index")


def voicegroup_symbol(text: str) -> str:
    """Normalize the one voicegroup spelling shared by both adapters."""
    if not VOICEGROUP_RE.fullmatch(text):
        raise argparse.ArgumentTypeError("must be a non-empty C identifier")
    symbol = text if text.startswith("voicegroup_") else f"voicegroup_{text}"
    if symbol == "voicegroup_":
        raise argparse.ArgumentTypeError("must name a voicegroup after the voicegroup_ prefix")
    return symbol


def parse_args(argv: list[str], tool_dir: Path, package_dir: Path) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--decomp", required=True, type=Path, help="compiled decomp project root")
    parser.add_argument("--voicegroup", required=True, type=voicegroup_symbol)
    parser.add_argument("--voice", required=True, type=voice_index, help="zero-based voice slot")
    parser.add_argument("--scenario", required=True, choices=tuple(SCENARIO_CONTRACTS))
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--note", type=control_value, default=60)
    parser.add_argument("--velocity", type=control_value, default=127)
    parser.add_argument("--volume", type=control_value, default=127)
    parser.add_argument("--pan", type=control_value, default=64)
    parser.add_argument("--record-voice", type=Path, default=tool_dir / "record_voice.sh", help="reference capture wrapper")
    parser.add_argument("--candidate-trace", type=Path, default=package_dir / "build" / "poryaaaa_driver_trace")
    parser.add_argument("--mgba-replay", type=Path, default=package_dir / "build" / "mgba_audio_trace_replay")
    parser.add_argument("--pory-replay", type=Path, default=package_dir / "build" / "poryaaaa_audio_trace")
    parser.add_argument("--driver-compare", type=Path, default=tool_dir / "driver_compare.py")
    parser.add_argument("--native-compare", type=Path, default=tool_dir / "native_compare.py")
    return parser.parse_args(argv)


def resolved(path: Path) -> Path:
    try:
        return path.expanduser().resolve(strict=False)
    except OSError as error:
        raise InfrastructureFailure(f"cannot resolve {path}: {error}") from error


def require_regular_file(path: Path, label: str, executable: bool = False) -> None:
    if not path.is_file():
        raise InfrastructureFailure(f"{label} is not a regular file: {path}")
    if executable and not os.access(path, os.X_OK):
        raise InfrastructureFailure(f"{label} is not executable: {path}")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as source:
            for block in iter(lambda: source.read(1 << 20), b""):
                digest.update(block)
    except OSError as error:
        raise InfrastructureFailure(f"cannot hash {path}: {error}") from error
    return digest.hexdigest()


def require_sha256(value: Any, label: str) -> str:
    if not isinstance(value, str) or not SHA256_RE.fullmatch(value):
        raise InfrastructureFailure(f"{label} must be a lowercase SHA-256 string")
    return value


def require_object(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise InfrastructureFailure(f"{label} must be a JSON object")
    return value


def require_string(value: Any, label: str) -> str:
    if not isinstance(value, str) or not value:
        raise InfrastructureFailure(f"{label} must be a non-empty string")
    return value


def require_integer(value: Any, label: str, minimum: int | None = None) -> int:
    if not isinstance(value, int) or isinstance(value, bool):
        raise InfrastructureFailure(f"{label} must be an integer")
    if minimum is not None and value < minimum:
        raise InfrastructureFailure(f"{label} must be at least {minimum}")
    return value


def require_boolean(value: Any, label: str) -> bool:
    if not isinstance(value, bool):
        raise InfrastructureFailure(f"{label} must be a boolean")
    return value


def field(document: dict[str, Any], name: str, label: str) -> Any:
    if name not in document:
        raise InfrastructureFailure(f"{label} is missing {name}")
    return document[name]


def duplicate_rejecting_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    output: dict[str, Any] = {}
    for name, value in pairs:
        if name in output:
            raise ValueError(f"duplicate key {name!r}")
        output[name] = value
    return output


def reject_json_constant(value: str) -> None:
    raise ValueError(f"non-finite JSON constant {value!r}")


def load_json(path: Path, label: str) -> dict[str, Any]:
    try:
        document = json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=duplicate_rejecting_object,
            parse_constant=reject_json_constant,
        )
    except (OSError, UnicodeDecodeError, ValueError, json.JSONDecodeError) as error:
        raise InfrastructureFailure(f"cannot load {label} {path}: {error}") from error
    return require_object(document, label)


def require_optional_object(value: Any, label: str) -> dict[str, Any] | None:
    if value is None:
        return None
    return require_object(value, label)


def require_equal(actual: Any, expected: Any, label: str) -> None:
    if actual != expected:
        raise GateFailure(f"{label}: expected {expected!r}, got {actual!r}")


def require_file_hash(path: Path, declared: Any, label: str) -> str:
    declared_hash = require_sha256(declared, label)
    actual_hash = sha256_file(path)
    require_equal(actual_hash, declared_hash, label)
    return actual_hash


def controls_from_args(args: argparse.Namespace) -> dict[str, int]:
    return {"note": args.note, "velocity": args.velocity, "volume": args.volume, "pan": args.pan}


def validate_controls(document: dict[str, Any], controls: dict[str, int], label: str) -> None:
    for name, expected in controls.items():
        require_equal(require_integer(field(document, name, label), f"{label}.{name}", 0), expected, f"{label}.{name}")


def validate_fixture(
    document: dict[str, Any], symbol: str, voice: int, scenario: str, controls: dict[str, int], label: str
) -> dict[str, Any]:
    require_equal(require_string(field(document, "voicegroup_symbol", label), f"{label}.voicegroup_symbol"), symbol, f"{label}.voicegroup_symbol")
    require_equal(require_integer(field(document, "voice_index", label), f"{label}.voice_index", 0), voice, f"{label}.voice_index")
    resolved_type = require_integer(field(document, "resolved_type", label), f"{label}.resolved_type", 0)
    expected_family = FAMILY_BY_TYPE.get(resolved_type)
    if expected_family is None:
        raise GateFailure(f"{label}.resolved_type is not a verified lifecycle family: {resolved_type}")
    family = require_string(field(document, "family", label), f"{label}.family")
    require_equal(family, expected_family, f"{label}.family")
    tone_data = require_sha256(field(document, "tone_data_sha256", label), f"{label}.tone_data_sha256")
    payload = require_sha256(field(document, "family_payload_sha256", label), f"{label}.family_payload_sha256")
    require_equal(require_string(field(document, "scenario", label), f"{label}.scenario"), scenario, f"{label}.scenario")
    validate_controls(document, controls, label)
    return {
        "family": family,
        "resolved_type": resolved_type,
        "tone_data_sha256": tone_data,
        "family_payload_sha256": payload,
    }


def validate_scenario(
    document: dict[str, Any], scenario: str, label: str, *, logical_vblanks_field: str, capture_frames_field: str, span_cycles_field: str
) -> dict[str, Any]:
    contract = SCENARIO_CONTRACTS[scenario]
    logical_vblanks = require_integer(field(document, logical_vblanks_field, label), f"{label}.{logical_vblanks_field}", 1)
    capture_frames = require_integer(field(document, capture_frames_field, label), f"{label}.{capture_frames_field}", 1)
    span_cycles = require_integer(field(document, span_cycles_field, label), f"{label}.{span_cycles_field}", 1)
    require_equal(logical_vblanks, contract["logical_vblanks"], f"{label}.{logical_vblanks_field}")
    require_equal(capture_frames, contract["capture_frames"], f"{label}.{capture_frames_field}")
    require_equal(span_cycles, contract["span_cycles"], f"{label}.{span_cycles_field}")
    require_equal(require_string(field(document, "high_level_action", label), f"{label}.high_level_action"), contract["high_level_action"], f"{label}.high_level_action")
    return {
        "logical_vblanks": logical_vblanks,
        "capture_frames": capture_frames,
        "span_cycles": span_cycles,
        "high_level_action": contract["high_level_action"],
    }


def validate_native_artifacts(prefix: Path, document: dict[str, Any], label: str, solo_mask: int) -> dict[str, Path]:
    require_equal(require_string(field(document, "format", label), f"{label}.format"), "poryaaaa-native-capture", f"{label}.format")
    require_equal(require_integer(field(document, "version", label), f"{label}.version", 1), 1, f"{label}.version")
    require_equal(require_integer(field(document, "channels", label), f"{label}.channels", 1), 2, f"{label}.channels")
    require_equal(require_string(field(document, "sample_format", label), f"{label}.sample_format"), "s16le", f"{label}.sample_format")
    require_equal(require_string(field(document, "cycle_format", label), f"{label}.cycle_format"), "u64le", f"{label}.cycle_format")
    require_equal(require_integer(field(document, "clock_hz", label), f"{label}.clock_hz", 1), CLOCK_HZ, f"{label}.clock_hz")
    frame_count = require_integer(field(document, "frame_count", label), f"{label}.frame_count", 1)
    first_cycle = require_integer(field(document, "first_cycle", label), f"{label}.first_cycle", 0)
    last_cycle = require_integer(field(document, "last_cycle", label), f"{label}.last_cycle", first_cycle)
    if last_cycle < first_cycle:
        raise InfrastructureFailure(f"{label} has an inverted cycle range")
    require_equal(require_integer(field(document, "solo_mask", label), f"{label}.solo_mask", 1), solo_mask, f"{label}.solo_mask")
    paths = {"pcm": prefix.with_suffix(".pcm"), "cycles": prefix.with_suffix(".cycles"), "json": prefix.with_suffix(".json")}
    for artifact_label, path in paths.items():
        require_regular_file(path, f"{label} {artifact_label}")
    try:
        pcm_size = paths["pcm"].stat().st_size
        cycles_size = paths["cycles"].stat().st_size
    except OSError as error:
        raise InfrastructureFailure(f"cannot stat {label} artifacts: {error}") from error
    if pcm_size != frame_count * 4 or cycles_size != frame_count * 8:
        raise InfrastructureFailure(f"{label} artifact sizes do not match frame_count")
    return paths


def validate_reference_manifest(
    path: Path,
    reference_trace: Path,
    reference_native: Path,
    rom: Path,
    elf: Path,
    symbol: str,
    voice: int,
    scenario: str,
    controls: dict[str, int],
) -> tuple[dict[str, Any], dict[str, Path], dict[str, Any], dict[str, Any]]:
    label = "reference native manifest"
    document = load_json(path, label)
    fixture = validate_fixture(document, symbol, voice, scenario, controls, label)
    family_spec = FAMILY_SPECS[fixture["family"]]
    artifacts = validate_native_artifacts(reference_native, document, label, family_spec["solo_mask"])
    require_equal(require_string(field(document, "source", label), f"{label}.source"), "mgba-full", f"{label}.source")
    require_equal(require_integer(field(document, "audio_channel_mask", label), f"{label}.audio_channel_mask", 1), family_spec["solo_mask"], f"{label}.audio_channel_mask")
    require_equal(require_integer(field(document, "mgba_master_volume", label), f"{label}.mgba_master_volume", 1), 0x100, f"{label}.mgba_master_volume")
    require_equal(require_string(field(document, "bios_mode", label), f"{label}.bios_mode"), "hle", f"{label}.bios_mode")
    require_file_hash(reference_trace, field(document, "trace_sha256", label), f"{label}.trace_sha256")
    require_file_hash(rom, field(document, "rom_sha256", label), f"{label}.rom_sha256")
    require_file_hash(elf, field(document, "elf_sha256", label), f"{label}.elf_sha256")
    require_file_hash(artifacts["pcm"], field(document, "pcm_sha256", label), f"{label}.pcm_sha256")
    require_file_hash(artifacts["cycles"], field(document, "cycles_sha256", label), f"{label}.cycles_sha256")
    require_integer(field(document, "rom_voice_address", label), f"{label}.rom_voice_address", 0)
    timing = validate_scenario(
        document,
        scenario,
        label,
        logical_vblanks_field="scenario_logical_vblanks",
        capture_frames_field="scenario_capture_frames",
        span_cycles_field="scenario_span_cycles",
    )
    scenario_begin = require_integer(field(document, "scenario_begin_cycle", label), f"{label}.scenario_begin_cycle", 0)
    scenario_end = require_integer(field(document, "scenario_end_cycle", label), f"{label}.scenario_end_cycle", scenario_begin)
    require_equal(scenario_end - scenario_begin, timing["span_cycles"], f"{label}.scenario cycle span")
    require_equal(require_integer(field(document, "scenario_span_frames", label), f"{label}.scenario_span_frames", 1), timing["capture_frames"], f"{label}.scenario_span_frames")
    for name, expected in (("mgba_commit", PINNED_MGBA_REVISION), ("mgba_base_revision", PINNED_MGBA_REVISION), ("mgba_source_policy", "authoritative-pinned-source")):
        require_equal(require_string(field(document, name, label), f"{label}.{name}"), expected, f"{label}.{name}")
    require_sha256(field(document, "mgba_observation_patch_sha256", label), f"{label}.mgba_observation_patch_sha256")
    require_boolean(field(document, "mgba_dirty", label), f"{label}.mgba_dirty")
    return document, artifacts, fixture, timing


def validate_candidate_manifest(
    path: Path,
    candidate_trace: Path,
    rom: Path,
    elf: Path,
    symbol: str,
    voice: int,
    scenario: str,
    controls: dict[str, int],
) -> tuple[dict[str, Any], dict[str, Any], dict[str, Any]]:
    label = "candidate trace manifest"
    document = load_json(path, label)
    require_equal(require_string(field(document, "format", label), f"{label}.format"), "poryaaaa-driver-candidate-trace", f"{label}.format")
    require_equal(require_integer(field(document, "version", label), f"{label}.version", 1), 1, f"{label}.version")
    require_equal(require_string(field(document, "source", label), f"{label}.source"), "poryaaaa-driver", f"{label}.source")
    require_equal(require_string(field(document, "trace_format", label), f"{label}.trace_format"), "PORYAAAA_AUDIO_TRACE", f"{label}.trace_format")
    require_equal(require_integer(field(document, "trace_version", label), f"{label}.trace_version", 1), 1, f"{label}.trace_version")
    require_equal(require_integer(field(document, "clock_hz", label), f"{label}.clock_hz", 1), CLOCK_HZ, f"{label}.clock_hz")
    trace_begin = require_integer(field(document, "trace_begin_cycle", label), f"{label}.trace_begin_cycle", 0)
    trace_end = require_integer(field(document, "trace_end_cycle", label), f"{label}.trace_end_cycle", trace_begin + 1)
    timing = validate_scenario(
        document,
        scenario,
        label,
        logical_vblanks_field="logical_vblanks",
        capture_frames_field="capture_frames",
        span_cycles_field="capture_span_cycles",
    )
    require_equal(trace_end - trace_begin, timing["span_cycles"], f"{label}.trace cycle span")
    require_file_hash(candidate_trace, field(document, "trace_sha256", label), f"{label}.trace_sha256")
    require_file_hash(rom, field(document, "rom_sha256", label), f"{label}.rom_sha256")
    require_file_hash(elf, field(document, "elf_sha256", label), f"{label}.elf_sha256")
    fixture = validate_fixture(document, symbol, voice, scenario, controls, label)
    require_equal(
        require_integer(field(document, "driver_origin_cycle", label), f"{label}.driver_origin_cycle", 0),
        DRIVER_ORIGIN_CYCLES[fixture["family"]],
        f"{label}.driver_origin_cycle",
    )
    return document, fixture, timing


def validate_replay_manifest(
    path: Path,
    prefix: Path,
    trace: Path,
    source: str,
    provenance: dict[str, Any] | None,
    solo_mask: int,
    label: str,
) -> tuple[dict[str, Any], dict[str, Path]]:
    document = load_json(path, label)
    artifacts = validate_native_artifacts(prefix, document, label, solo_mask)
    require_equal(require_string(field(document, "source", label), f"{label}.source"), source, f"{label}.source")
    if source == "mgba-clone":
        require_file_hash(trace, field(document, "trace_sha256", label), f"{label}.trace_sha256")
        if provenance is None:
            raise InfrastructureFailure(f"{label} lacks expected mGBA provenance")
        for name in ("mgba_commit", "mgba_base_revision", "mgba_observation_patch_sha256", "mgba_source_policy", "mgba_dirty"):
            require_equal(field(document, name, label), field(provenance, name, "reference native manifest"), f"{label}.{name}")
        require_equal(require_string(field(document, "mgba_commit", label), f"{label}.mgba_commit"), PINNED_MGBA_REVISION, f"{label}.mgba_commit")
        require_equal(require_integer(field(document, "audio_channel_mask", label), f"{label}.audio_channel_mask", 1), solo_mask, f"{label}.audio_channel_mask")
        require_equal(require_integer(field(document, "mgba_master_volume", label), f"{label}.mgba_master_volume", 1), 0x100, f"{label}.mgba_master_volume")
        require_equal(require_string(field(document, "bios_mode", label), f"{label}.bios_mode"), "hle", f"{label}.bios_mode")
        require_boolean(field(document, "mgba_dirty", label), f"{label}.mgba_dirty")
    return document, artifacts


def run_command(argv: list[str], label: str, capture_stdout: bool = False) -> subprocess.CompletedProcess[bytes]:
    try:
        return subprocess.run(argv, stdout=subprocess.PIPE if capture_stdout else None, check=False)
    except OSError as error:
        raise InfrastructureFailure(f"cannot run {label}: {error}") from error


def require_success(result: subprocess.CompletedProcess[bytes], label: str) -> None:
    if result.returncode != 0:
        raise InfrastructureFailure(f"{label} exited {result.returncode}")


def validate_driver_result(
    path: Path, reference_trace: Path, candidate_trace: Path, family: str
) -> dict[str, Any]:
    label = "driver comparator result"
    document = load_json(path, label)
    require_equal(require_string(field(document, "family", label), f"{label}.family"), family, f"{label}.family")
    transaction = require_boolean(field(document, "transaction_exact", label), f"{label}.transaction_exact")
    payload = require_boolean(field(document, "payload_exact", label), f"{label}.payload_exact")
    logical = require_boolean(field(document, "logical_state_exact", label), f"{label}.logical_state_exact")
    require_boolean(field(document, "cycle_exact", label), f"{label}.cycle_exact")
    require_integer(field(document, "reference_event_count", label), f"{label}.reference_event_count", 0)
    require_integer(field(document, "candidate_event_count", label), f"{label}.candidate_event_count", 0)
    hashes = require_object(field(document, "hashes", label), f"{label}.hashes")
    require_equal(require_sha256(field(hashes, "reference_trace_sha256", f"{label}.hashes"), f"{label}.hashes.reference_trace_sha256"), sha256_file(reference_trace), f"{label}.hashes.reference_trace_sha256")
    require_equal(require_sha256(field(hashes, "candidate_trace_sha256", f"{label}.hashes"), f"{label}.hashes.candidate_trace_sha256"), sha256_file(candidate_trace), f"{label}.hashes.candidate_trace_sha256")
    reference_payload = require_sha256(field(hashes, "reference_payload_sha256", f"{label}.hashes"), f"{label}.hashes.reference_payload_sha256")
    candidate_payload = require_sha256(field(hashes, "candidate_payload_sha256", f"{label}.hashes"), f"{label}.hashes.candidate_payload_sha256")
    require_equal(payload, reference_payload == candidate_payload, f"{label}.payload_exact")
    require_object(field(document, "payload", label), f"{label}.payload")
    require_object(field(document, "timing", label), f"{label}.timing")
    first_divergence = require_optional_object(field(document, "first_divergence", label), f"{label}.first_divergence")
    logical_divergence = require_optional_object(field(document, "logical_state_divergence", label), f"{label}.logical_state_divergence")
    if not transaction and first_divergence is None:
        raise InfrastructureFailure(f"{label}.first_divergence is required when transactions differ")
    if not logical and logical_divergence is None:
        raise InfrastructureFailure(f"{label}.logical_state_divergence is required when states differ")
    document["_required_gates_passed"] = transaction and payload and logical
    return document


def write_native_result(path: Path, output: bytes, label: str) -> dict[str, Any]:
    try:
        path.write_bytes(output)
    except OSError as error:
        raise InfrastructureFailure(f"cannot write {label}: {error}") from error
    return load_json(path, label)


def validate_native_result(
    document: dict[str, Any],
    reference_manifest: Path,
    candidate_manifest: Path,
    reference_artifacts: dict[str, Path],
    candidate_artifacts: dict[str, Path],
    label: str,
) -> bool:
    require_equal(require_string(field(document, "reference", label), f"{label}.reference"), str(reference_manifest), f"{label}.reference")
    require_equal(require_string(field(document, "candidate", label), f"{label}.candidate"), str(candidate_manifest), f"{label}.candidate")
    passed = require_boolean(field(document, "passed", label), f"{label}.passed")
    reference_frames = require_integer(field(load_json(reference_manifest, "reference capture manifest"), "frame_count", "reference capture manifest"), "reference capture manifest.frame_count", 1)
    candidate_frames = require_integer(field(load_json(candidate_manifest, "candidate capture manifest"), "frame_count", "candidate capture manifest"), "candidate capture manifest.frame_count", 1)
    require_equal(require_integer(field(document, "reference_frame_count", label), f"{label}.reference_frame_count", 1), reference_frames, f"{label}.reference_frame_count")
    require_equal(require_integer(field(document, "candidate_frame_count", label), f"{label}.candidate_frame_count", 1), candidate_frames, f"{label}.candidate_frame_count")
    hashes = require_object(field(document, "hashes", label), f"{label}.hashes")
    for name, expected in {
        "reference_pcm_sha256": sha256_file(reference_artifacts["pcm"]),
        "candidate_pcm_sha256": sha256_file(candidate_artifacts["pcm"]),
        "reference_cycles_sha256": sha256_file(reference_artifacts["cycles"]),
        "candidate_cycles_sha256": sha256_file(candidate_artifacts["cycles"]),
    }.items():
        require_equal(require_sha256(field(hashes, name, f"{label}.hashes"), f"{label}.hashes.{name}"), expected, f"{label}.hashes.{name}")
    first_mismatch = require_optional_object(field(document, "first_mismatch", label), f"{label}.first_mismatch")
    if not passed and first_mismatch is None:
        raise InfrastructureFailure(f"{label}.first_mismatch is required when captures differ")
    return passed


def stable_output_argument(argument: str, stage: Path) -> str:
    """Remove the private staging root from a published reproducible argv."""
    stage_text = str(stage)
    if argument == stage_text or argument.startswith(stage_text + os.sep):
        return "{output-dir}" + argument[len(stage_text) :]
    return argument


def run_native_comparison(
    executable: Path,
    reference_manifest: Path,
    candidate_manifest: Path,
    reference_trace: Path,
    candidate_trace: Path,
    output: Path,
    reference_artifacts: dict[str, Path],
    candidate_artifacts: dict[str, Path],
    commands: dict[str, list[str]],
    name: str,
) -> bool:
    argv = [
        str(executable),
        str(reference_manifest),
        str(candidate_manifest),
        "--reference-trace",
        str(reference_trace),
        "--candidate-trace",
        str(candidate_trace),
    ]
    commands[name] = argv
    result = run_command(argv, name, capture_stdout=True)
    if result.returncode not in (0, 1):
        raise InfrastructureFailure(f"{name} exited {result.returncode}")
    document = write_native_result(output, result.stdout or b"", name)
    passed = validate_native_result(document, reference_manifest, candidate_manifest, reference_artifacts, candidate_artifacts, name)
    if (result.returncode == 0) != passed:
        raise InfrastructureFailure(f"{name} exit status contradicts its JSON result")
    document["reference"] = stable_output_argument(document["reference"], output.parent)
    document["candidate"] = stable_output_argument(document["candidate"], output.parent)
    write_manifest(output, document)
    return passed


def artifact_records(stage: Path, names: tuple[str, ...]) -> dict[str, dict[str, str]]:
    records: dict[str, dict[str, str]] = {}
    for name in names:
        path = stage / name
        require_regular_file(path, f"artifact {name}")
        records[name] = {"path": name, "sha256": sha256_file(path)}
    return records


def manifest_commands(commands: dict[str, list[str]], stage: Path) -> dict[str, list[str]]:
    return {name: [stable_output_argument(argument, stage) for argument in argv] for name, argv in commands.items()}


def write_manifest(path: Path, document: dict[str, Any]) -> None:
    try:
        path.write_text(json.dumps(document, indent=2, sort_keys=True, allow_nan=False) + "\n", encoding="utf-8")
    except (OSError, TypeError, ValueError) as error:
        raise InfrastructureFailure(f"cannot write manifest {path}: {error}") from error


def publish(stage: Path, destination: Path) -> None:
    try:
        publish_directory_no_replace(stage, destination)
    except PublishError as error:
        raise InfrastructureFailure(str(error)) from error


def preflight(args: argparse.Namespace) -> tuple[Path, Path, Path, Path, dict[str, Path]]:
    decomp = resolved(args.decomp)
    output = resolved(args.output_dir)
    if not decomp.is_dir():
        raise InfrastructureFailure(f"--decomp is not a directory: {decomp}")
    if not output.parent.is_dir():
        raise InfrastructureFailure(f"output parent is not a directory: {output.parent}")
    rom = decomp / "pokeemerald-hearth.gba"
    elf = decomp / "pokeemerald-hearth.elf"
    require_regular_file(rom, "ROM")
    require_regular_file(elf, "ELF")
    failed = output.with_name(f"{output.name}.failed")
    if output.exists() or failed.exists():
        raise InfrastructureFailure(f"output or failure directory already exists: {output} / {failed}")
    executables = {
        "record_voice": resolved(args.record_voice),
        "candidate_trace": resolved(args.candidate_trace),
        "mgba_replay": resolved(args.mgba_replay),
        "pory_replay": resolved(args.pory_replay),
        "driver_compare": resolved(args.driver_compare),
        "native_compare": resolved(args.native_compare),
    }
    for name, path in executables.items():
        require_regular_file(path, name.replace("_", " "), executable=True)
    return decomp, output, rom, elf, executables


def failed_publication(stage: Path | None, output: Path, message: str, code: int) -> int:
    print(f"validate_driver: {message}", file=sys.stderr)
    if stage is not None and stage.exists():
        try:
            publish(stage, output.with_name(f"{output.name}.failed"))
        except InfrastructureFailure as error:
            print(f"validate_driver: {error}; staged diagnostics remain at {stage}", file=sys.stderr)
            return 2
    return code


def artifact_names() -> tuple[str, ...]:
    return (
        "reference.trace",
        "reference-native.pcm",
        "reference-native.cycles",
        "reference-native.json",
        "reference-mgba.pcm",
        "reference-mgba.cycles",
        "reference-mgba.json",
        "reference-pory.pcm",
        "reference-pory.cycles",
        "reference-pory.json",
        "candidate.trace",
        "candidate.trace.manifest.json",
        "candidate-mgba.pcm",
        "candidate-mgba.cycles",
        "candidate-mgba.json",
        "candidate-pory.pcm",
        "candidate-pory.cycles",
        "candidate-pory.json",
        "driver-compare.json",
        "reference-native-compare.json",
        "reference-hardware-compare.json",
        "candidate-hardware-compare.json",
    )


def validation_manifest(
    stage: Path,
    decomp: Path,
    rom: Path,
    elf: Path,
    executables: dict[str, Path],
    controls: dict[str, int],
    scenario: str,
    fixture: dict[str, Any],
    timing: dict[str, Any],
    reference_document: dict[str, Any],
    commands: dict[str, list[str]],
    diagnostics: dict[str, Any],
    gates: dict[str, bool],
) -> dict[str, Any]:
    return {
        "format": "poryaaaa-driver-validation",
        "version": 1,
        "status": "passed" if all(gates.values()) else "failed",
        "family": fixture["family"],
        "scenario": scenario,
        "scenario_timing": timing,
        "controls": controls,
        "fixture": fixture,
        "diagnostics": diagnostics,
        "provenance": {
            name: reference_document[name]
            for name in ("mgba_commit", "mgba_base_revision", "mgba_observation_patch_sha256", "mgba_source_policy", "mgba_dirty")
        },
        "inputs": {
            "decomp": {"path": str(decomp)},
            "rom": {"path": str(rom), "sha256": sha256_file(rom)},
            "elf": {"path": str(elf), "sha256": sha256_file(elf)},
        },
        "executables": {name: {"path": str(path), "sha256": sha256_file(path)} for name, path in executables.items()},
        "commands": manifest_commands(commands, stage),
        "artifacts": artifact_records(stage, artifact_names()),
        "gates": gates,
    }


def main(argv: list[str] | None = None) -> int:
    tool_dir = Path(__file__).resolve().parent
    package_dir = tool_dir.parent.parent
    args = parse_args(sys.argv[1:] if argv is None else argv, tool_dir, package_dir)
    stage: Path | None = None
    try:
        decomp, output, rom, elf, executables = preflight(args)
        stage = output.parent / f".{output.name}.stage"
        if stage.exists():
            raise InfrastructureFailure(f"staging directory already exists: {stage}")
        try:
            stage.mkdir(mode=0o700)
        except OSError as error:
            raise InfrastructureFailure(f"cannot create staging directory {stage}: {error}") from error

        controls = controls_from_args(args)
        reference_trace = stage / "reference.trace"
        reference_native = stage / "reference-native"
        candidate_trace = stage / "candidate.trace"
        reference_manifest = reference_native.with_suffix(".json")
        candidate_manifest = Path(f"{candidate_trace}.manifest.json")
        commands: dict[str, list[str]] = {}

        reference_argv = [
            str(executables["record_voice"]),
            "--decomp", str(decomp),
            "--voicegroup", args.voicegroup,
            "--voice", str(args.voice),
            "--capture-stage", "native",
            "--trace-output", str(reference_trace),
            "--native-output-prefix", str(reference_native),
            "--scenario", args.scenario,
        ]
        for name, value in controls.items():
            reference_argv.extend((f"--{name}", str(value)))
        commands["reference_capture"] = reference_argv
        require_success(run_command(reference_argv, "reference capture"), "reference capture")
        require_regular_file(reference_trace, "reference trace")
        require_regular_file(reference_manifest, "reference native manifest")
        reference_document, reference_artifacts, reference_fixture, reference_timing = validate_reference_manifest(
            reference_manifest, reference_trace, reference_native, rom, elf, args.voicegroup, args.voice, args.scenario, controls
        )
        family = reference_fixture["family"]
        family_spec = FAMILY_SPECS[family]

        candidate_argv = [
            str(executables["candidate_trace"]), str(decomp), args.voicegroup, str(args.voice),
            "--scenario", args.scenario, "--trace-output", str(candidate_trace),
        ]
        for name, value in controls.items():
            candidate_argv.extend((f"--{name}", str(value)))
        commands["candidate_capture"] = candidate_argv
        require_success(run_command(candidate_argv, "candidate capture"), "candidate capture")
        require_regular_file(candidate_trace, "candidate trace")
        require_regular_file(candidate_manifest, "candidate trace manifest")
        candidate_document, candidate_fixture, candidate_timing = validate_candidate_manifest(
            candidate_manifest, candidate_trace, rom, elf, args.voicegroup, args.voice, args.scenario, controls
        )
        for name in ("family", "resolved_type", "tone_data_sha256", "family_payload_sha256"):
            require_equal(candidate_fixture[name], reference_fixture[name], f"fixture.{name}")
        require_equal(candidate_timing, reference_timing, "candidate/reference scenario timing")

        comparator_path = stage / "driver-compare.json"
        comparator_argv = [
            str(executables["driver_compare"]), str(reference_trace), str(candidate_trace),
            "--family", family, "--output", str(comparator_path),
        ]
        commands["driver_compare"] = comparator_argv
        comparator_process = run_command(comparator_argv, "driver comparator")
        if comparator_process.returncode not in (0, 1):
            raise InfrastructureFailure(f"driver comparator exited {comparator_process.returncode}")
        require_regular_file(comparator_path, "driver comparator result")
        comparator_result = validate_driver_result(comparator_path, reference_trace, candidate_trace, family)
        if (comparator_process.returncode == 0) != comparator_result["_required_gates_passed"]:
            raise InfrastructureFailure("driver comparator exit status contradicts its JSON result")

        replay_specs = (
            ("reference_mgba_replay", executables["mgba_replay"], reference_trace, stage / "reference-mgba", "mgba-clone", True),
            ("reference_pory_replay", executables["pory_replay"], reference_trace, stage / "reference-pory", "poryaaaa", False),
            ("candidate_mgba_replay", executables["mgba_replay"], candidate_trace, stage / "candidate-mgba", "mgba-clone", False),
            ("candidate_pory_replay", executables["pory_replay"], candidate_trace, stage / "candidate-pory", "poryaaaa", False),
        )
        replay_artifacts: dict[str, dict[str, Path]] = {}
        for name, executable, trace, prefix, source, use_reference_manifest in replay_specs:
            replay_argv = [str(executable), "--input", str(trace), "--output-prefix", str(prefix), "--solo", family_spec["solo"]]
            if use_reference_manifest:
                replay_argv.extend(("--reference-manifest", str(reference_manifest)))
            commands[name] = replay_argv
            require_success(run_command(replay_argv, name), name)
            replay_manifest = prefix.with_suffix(".json")
            require_regular_file(replay_manifest, f"{name} manifest")
            _, replay_artifacts[name] = validate_replay_manifest(
                replay_manifest, prefix, trace, source, reference_document if source == "mgba-clone" else None,
                family_spec["solo_mask"], name,
            )

        gates = {
            "transaction_exact": comparator_result["transaction_exact"],
            "payload_exact": comparator_result["payload_exact"],
            "logical_state_exact": comparator_result["logical_state_exact"],
            "reference_native_exact": run_native_comparison(
                executables["native_compare"], reference_manifest, stage / "reference-mgba.json", reference_trace, reference_trace,
                stage / "reference-native-compare.json", reference_artifacts, replay_artifacts["reference_mgba_replay"], commands, "reference_native_compare",
            ),
            "reference_hardware_exact": run_native_comparison(
                executables["native_compare"], stage / "reference-mgba.json", stage / "reference-pory.json", reference_trace, reference_trace,
                stage / "reference-hardware-compare.json", replay_artifacts["reference_mgba_replay"], replay_artifacts["reference_pory_replay"], commands, "reference_hardware_compare",
            ),
            "candidate_hardware_exact": run_native_comparison(
                executables["native_compare"], stage / "candidate-mgba.json", stage / "candidate-pory.json", candidate_trace, candidate_trace,
                stage / "candidate-hardware-compare.json", replay_artifacts["candidate_mgba_replay"], replay_artifacts["candidate_pory_replay"], commands, "candidate_hardware_compare",
            ),
        }
        diagnostics = {
            "first_transaction_divergence": comparator_result["first_divergence"],
            "first_sample_divergences": {
                name: load_json(stage / filename, name)["first_mismatch"]
                for name, filename in (
                    ("reference_native", "reference-native-compare.json"),
                    ("reference_hardware", "reference-hardware-compare.json"),
                    ("candidate_hardware", "candidate-hardware-compare.json"),
                )
            },
        }
        manifest = validation_manifest(
            stage, decomp, rom, elf, executables, controls, args.scenario, reference_fixture, reference_timing,
            reference_document, commands, diagnostics, gates,
        )
        write_manifest(stage / "manifest.json", manifest)
        if not all(gates.values()):
            raise GateFailure("one or more required validation gates failed")
        publish(stage, output)
        return 0
    except GateFailure as error:
        return failed_publication(stage, resolved(args.output_dir), str(error), 1)
    except InfrastructureFailure as error:
        return failed_publication(stage, resolved(args.output_dir), str(error), 2)
    except Exception as error:
        return failed_publication(stage, resolved(args.output_dir), f"unexpected failure: {error}", 2)


if __name__ == "__main__":
    sys.exit(main())
