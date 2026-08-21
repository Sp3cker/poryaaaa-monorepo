#!/usr/bin/env python3
"""Strictly validate checked PCM oracles and compare a hermetic candidate twice."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

import case_format

HEADER = "PORYAAAA_AUDIO_TRACE 1"
CLOCK_HZ = 16_777_216
UINT32_MAX = (1 << 32) - 1
UINT64_MAX = (1 << 64) - 1
EXPECTED_FILES = {"manifest.json", "expected.trace"}
REFERENCES = {
    "ipatix": {
        "repository": "https://github.com/ipatix/gba-hq-mixer",
        "commit": "2bd31435101e4c3a4af568a6ed33d7cc9f1a6da9",
        "patch_sha256": "063dc98e03581bab797e2112aa42108201272b3710f0664f204539d70eecf38b",
        "feature_flags": {"POKE_CHN_INIT": 1, "ENABLE_STEREO": 1, "ENABLE_REVERB": 1, "ENABLE_DMA": 1},
        "generator": "generate_pcm_mixer_oracles.py",
        "cases": "cases.json",
    },
    "sappy": {
        "repository": "https://github.com/pret/pokeemerald",
        "commit": "9a83a2bbe8e097e62c00f1dbd56849766775d7b6",
        "patch_sha256": None,
        "feature_flags": {},
        "generator": "generate_sappy_pcm_mixer_oracles.py",
        "cases": "cases-sappy.json",
    },
}
DIVERGENCES_FILE = "divergences.json"


class ValidationError(RuntimeError):
    pass


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    try:
        return sha256_bytes(path.read_bytes())
    except OSError as error:
        raise ValidationError(f"cannot hash {path}: {error}") from error


def load_json(path: Path) -> Any:
    def reject_duplicate(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise ValidationError(f"{path}: duplicate key {key!r}")
            result[key] = value
        return result

    try:
        return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=reject_duplicate)
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValidationError(f"{path}: invalid JSON: {error}") from error


def validate_schema(value: Any, schema: dict[str, Any], where: str) -> None:
    expected = schema.get("type")
    if expected is not None:
        allowed = expected if isinstance(expected, list) else [expected]
        actual = (
            "null" if value is None else "boolean" if isinstance(value, bool) else
            "integer" if isinstance(value, int) else "string" if isinstance(value, str) else
            "array" if isinstance(value, list) else "object" if isinstance(value, dict) else "unknown"
        )
        if actual not in allowed:
            raise ValidationError(f"{where}: expected {expected}, got {actual}")
    if "const" in schema and value != schema["const"]:
        raise ValidationError(f"{where}: expected {schema['const']!r}")
    if "enum" in schema and value not in schema["enum"]:
        raise ValidationError(f"{where}: value is not allowed")
    if isinstance(value, str):
        if len(value) < schema.get("minLength", 0):
            raise ValidationError(f"{where}: string is too short")
        if "pattern" in schema and re.fullmatch(schema["pattern"], value) is None:
            raise ValidationError(f"{where}: string does not match schema")
    if isinstance(value, int) and not isinstance(value, bool):
        if value < schema.get("minimum", value) or value > schema.get("maximum", value):
            raise ValidationError(f"{where}: integer is outside schema range")
    if isinstance(value, dict):
        properties = schema.get("properties", {})
        missing = set(schema.get("required", [])) - set(value)
        if missing:
            raise ValidationError(f"{where}: missing fields {', '.join(sorted(missing))}")
        if schema.get("additionalProperties") is False:
            extras = set(value) - set(properties)
            if extras:
                raise ValidationError(f"{where}: unknown fields {', '.join(sorted(extras))}")
        for key, child in value.items():
            if key in properties:
                validate_schema(child, properties[key], f"{where}.{key}")
    if isinstance(value, list):
        if len(value) < schema.get("minItems", 0):
            raise ValidationError(f"{where}: array is too short")
        for index, child in enumerate(value):
            if "items" in schema:
                validate_schema(child, schema["items"], f"{where}[{index}]")


def parse_uint(token: str, limit: int) -> int | None:
    if not token or not token.isdecimal():
        return None
    value = int(token)
    return value if value <= limit else None


def parse_hex(token: str) -> int | None:
    if re.fullmatch(r"0x[0-9A-Fa-f]+", token) is None:
        return None
    value = int(token, 16)
    return value if value <= UINT32_MAX else None


def parse_trace(path: Path) -> tuple[tuple[int, int], tuple[int, int], list[tuple[int, int, int, int, int]]]:
    try:
        data = path.read_bytes()
    except OSError as error:
        raise ValidationError(f"cannot read {path}: {error}") from error
    prefix = (HEADER + "\n").encode("ascii")
    if not data.startswith(prefix) or not data.endswith(b"\n"):
        raise ValidationError(f"{path}: malformed trace framing")
    clock_seen = False
    opened = False
    closed = False
    begin: tuple[int, int] | None = None
    end: tuple[int, int] | None = None
    last: tuple[int, int] | None = None
    fifo: list[tuple[int, int, int, int, int]] = []
    for line_number, raw in enumerate(data[len(prefix):].splitlines(), 2):
        try:
            line = raw.decode("utf-8")
        except UnicodeDecodeError as error:
            raise ValidationError(f"{path}:{line_number}: invalid UTF-8") from error
        if not line or line.startswith("#"):
            continue
        if "  " in line or "\t" in line or line.startswith(" ") or line.endswith(" "):
            raise ValidationError(f"{path}:{line_number}: non-canonical spacing")
        tokens = line.split(" ")
        if tokens[0] == "CLOCK":
            if clock_seen or last is not None or tokens != ["CLOCK", str(CLOCK_HZ)]:
                raise ValidationError(f"{path}:{line_number}: invalid CLOCK")
            clock_seen = True
            continue
        if not clock_seen or len(tokens) < 3:
            raise ValidationError(f"{path}:{line_number}: event precedes CLOCK")
        cycle = parse_uint(tokens[1], UINT64_MAX)
        order = parse_uint(tokens[2], UINT32_MAX)
        if cycle is None or order is None or (last is not None and (cycle, order) <= last):
            raise ValidationError(f"{path}:{line_number}: invalid event position")
        kind = tokens[0]
        if kind == "BEGIN" and len(tokens) == 3 and not opened and not closed:
            opened = True
            begin = (cycle, order)
        elif kind == "END" and len(tokens) == 3 and opened and not closed:
            opened = False
            closed = True
            end = (cycle, order)
        elif not opened or closed:
            raise ValidationError(f"{path}:{line_number}: event outside interval")
        elif kind == "WRITE" and len(tokens) == 6:
            width = parse_uint(tokens[3], 4)
            address = parse_hex(tokens[4])
            value = parse_hex(tokens[5])
            if width is None or width == 0 or address is None or value is None:
                raise ValidationError(f"{path}:{line_number}: invalid WRITE")
            if width == 4 and address in (0x040000A0, 0x040000A4):
                fifo.append((cycle, order, width, address, value))
        elif kind == "TIMER" and len(tokens) == 4 and parse_uint(tokens[3], 1) is not None:
            pass
        elif kind == "SAMPLE" and len(tokens) == 3:
            pass
        else:
            raise ValidationError(f"{path}:{line_number}: invalid event")
        last = (cycle, order)
    if begin is None or end is None or opened or not closed:
        raise ValidationError(f"{path}: trace does not contain one closed interval")
    return begin, end, fifo


def event_hash(rows: list[tuple[int, int, int, int, int]]) -> str:
    payload = b"".join(
        f"WRITE {cycle} {order} {width} 0x{address:08X} 0x{value:08X}\n".encode("ascii")
        for cycle, order, width, address, value in rows
    )
    return sha256_bytes(payload)


def run_candidate(runner: Path, case_blob: Path, output: Path) -> None:
    try:
        completed = subprocess.run(
            [str(runner), "--input", str(case_blob), "--trace-output", str(output)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False, timeout=30)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise ValidationError(f"cannot execute candidate runner: {error}") from error
    if completed.returncode != 0:
        stderr = completed.stderr.decode("utf-8", errors="replace").strip()
        raise ValidationError(f"candidate runner failed with status {completed.returncode}: {stderr}")
    if completed.stdout:
        raise ValidationError("candidate runner wrote unexpected stdout")
    if not output.is_file():
        raise ValidationError("candidate runner did not create its trace")


def validate_one(case: dict[str, Any], directory: Path, manifest_schema: dict[str, Any], root: Path,
                 runner: Path | None, temporary: Path) -> None:
    if directory.is_symlink() or not directory.is_dir() or {
            path.name for path in directory.iterdir()} != EXPECTED_FILES:
        raise ValidationError(f"{directory}: fixture directory must contain exactly {sorted(EXPECTED_FILES)}")
    if any(path.is_symlink() or not path.is_file() for path in directory.iterdir()):
        raise ValidationError(f"{directory}: fixture artifacts must be regular files")
    mode = case["mode"]
    reference = REFERENCES[mode]
    manifest = load_json(directory / "manifest.json")
    if not isinstance(manifest, dict):
        raise ValidationError(f"{directory}: manifest must be an object")
    validate_schema(manifest, manifest_schema, str(directory / "manifest.json"))
    if manifest["case"] != case["id"] or manifest["mode"] != mode or manifest["settings"] != case["settings"]:
        raise ValidationError(f"{directory}: manifest identity/settings mismatch")
    actual_reference = manifest["reference"]
    for field in ("repository", "commit", "patch_sha256", "feature_flags"):
        if actual_reference[field] != reference[field]:
            raise ValidationError(f"{directory}: reference {field} mismatch")
    expected_tools = {"assembler", "linker", "objcopy", "readelf"}
    if mode == "sappy":
        expected_tools.add("compiler")
    if set(manifest["toolchain"]) != expected_tools:
        raise ValidationError(f"{directory}: toolchain provenance fields mismatch")

    checked_hashes = {
        "generator": root / reference["generator"],
        "cases": root / reference["cases"],
        "input": root / case["source"],
        "trace": directory / "expected.trace",
    }
    for name, path in checked_hashes.items():
        if sha256_file(path) != manifest["hashes"][name]:
            raise ValidationError(f"{directory}: {name} hash mismatch")

    source_path = root / case["source"]
    try:
        values, descriptor = case_format.parse_source(source_path, mode)
    except case_format.CaseFormatError as error:
        raise ValidationError(str(error)) from error
    if case_format.settings(values) != case["settings"]:
        raise ValidationError(f"{case['id']}: source descriptor settings differ from case inventory")

    begin, end, golden = parse_trace(directory / "expected.trace")
    block_count = values["FIXTURE_BLOCK_COUNT"]
    if len(golden) != block_count * 48:
        raise ValidationError(f"{directory}: FIFO row count does not match the source descriptor")
    if manifest["range"] != {
        "begin_cycle": begin[0], "end_cycle": end[0], "row_count": len(golden), "event_hash": event_hash(golden)
    }:
        raise ValidationError(f"{directory}: manifest range mismatch")
    if runner is None:
        return

    case_blob = temporary / f"{case['id']}.case"
    case_blob.write_bytes(case_format.make_case_blob(mode, descriptor, begin, end, golden))
    first = temporary / f"{case['id']}-first.trace"
    second = temporary / f"{case['id']}-second.trace"
    run_candidate(runner, case_blob, first)
    run_candidate(runner, case_blob, second)
    if first.read_bytes() != second.read_bytes():
        raise ValidationError(f"{case['id']}: candidate output is nondeterministic")
    first_begin, first_end, first_rows = parse_trace(first)
    second_begin, second_end, second_rows = parse_trace(second)
    if first_begin != begin or first_end != end or second_begin != begin or second_end != end:
        raise ValidationError(f"{case['id']}: candidate interval mismatch")
    if first_rows != golden or second_rows != golden:
        for index, (actual, expected) in enumerate(zip(first_rows, golden)):
            if actual != expected:
                raise ValidationError(
                    f"{case['id']}: FIFO row {index} mismatch: candidate={actual}, reference={expected}")
        raise ValidationError(
            f"{case['id']}: FIFO row count mismatch: candidate={len(first_rows)}, reference={len(golden)}")


def validate_divergences(root: Path, fixtures: Path,
                         cases_by_mode: dict[str, list[dict[str, Any]]]) -> int:
    path = root / DIVERGENCES_FILE
    inventory = load_json(path)
    if not isinstance(inventory, dict) or set(inventory) != {"schema", "version", "pairs"} or \
            inventory["schema"] != "poryaaaa.pcm-mixer-divergences" or inventory["version"] != 1 or \
            not isinstance(inventory["pairs"], list):
        raise ValidationError(f"{path}: invalid divergence inventory")
    cases = {mode: {case["id"]: case for case in entries} for mode, entries in cases_by_mode.items()}
    seen: set[str] = set()
    for pair in inventory["pairs"]:
        if not isinstance(pair, dict) or set(pair) != {"id", "ipatix", "sappy", "reason"}:
            raise ValidationError(f"{path}: invalid divergence pair")
        pair_id = pair["id"]
        if not isinstance(pair_id, str) or not pair_id or pair_id in seen:
            raise ValidationError(f"{path}: duplicate or invalid divergence id")
        seen.add(pair_id)
        ipatix_id = pair["ipatix"]
        sappy_id = pair["sappy"]
        if ipatix_id not in cases["ipatix"] or sappy_id not in cases["sappy"]:
            raise ValidationError(f"{path}: divergence pair {pair_id} references an unknown fixture")
        if not isinstance(pair["reason"], str) or not pair["reason"]:
            raise ValidationError(f"{path}: divergence pair {pair_id} has no reason")
        if cases["sappy"][sappy_id]["settings"]["max_channels"] > 12:
            raise ValidationError(f"{path}: extension-stress fixture {sappy_id} is not source parity")
        left = parse_trace(fixtures / "ipatix" / ipatix_id / "expected.trace")[2]
        right = parse_trace(fixtures / "sappy" / sappy_id / "expected.trace")[2]
        if [(row[2], row[3], row[4]) for row in left] == [(row[2], row[3], row[4]) for row in right]:
            raise ValidationError(f"{path}: divergence pair {pair_id} unexpectedly has equal FIFO bytes")
    return len(seen)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures", required=True, type=Path)
    parser.add_argument("--runner", type=Path)
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    options = parse_args(argv)
    root = Path(__file__).resolve().parent
    try:
        case_format.verify_c_header()
        cases_schema = load_json(root / "cases.schema.json")
        manifest_schema = load_json(root / "manifest.schema.json")
        if not isinstance(cases_schema, dict) or not isinstance(manifest_schema, dict):
            raise ValidationError("oracle schemas must be objects")
        cases_by_mode: dict[str, list[dict[str, Any]]] = {}
        all_ids: list[str] = []
        all_sources: list[str] = []
        for mode, reference in REFERENCES.items():
            cases_path = root / reference["cases"]
            cases = load_json(cases_path)
            if not isinstance(cases, dict):
                raise ValidationError(f"{cases_path}: case inventory must be an object")
            validate_schema(cases, cases_schema, str(cases_path))
            entries = cases["cases"]
            if any(case["mode"] != mode for case in entries):
                raise ValidationError(f"{cases_path}: every case must use mode {mode}")
            if mode == "sappy" and {case["settings"]["max_channels"] for case in entries} != set(range(16)):
                raise ValidationError("cases-sappy.json must exercise every voice count from 0 through 15")
            cases_by_mode[mode] = entries
            all_ids.extend(case["id"] for case in entries)
            all_sources.extend(case["source"] for case in entries)
        if len(all_ids) != len(set(all_ids)):
            raise ValidationError("case inventories have duplicate case IDs")
        if len(all_sources) != len(set(all_sources)):
            raise ValidationError("every case must own one unique descriptor source")
        source_inventory = {str(path.relative_to(root)) for path in (root / "inputs").glob("*.s")}
        if source_inventory != set(all_sources):
            raise ValidationError("inputs/*.s must exactly match the case inventories")

        fixtures = options.fixtures.resolve()
        if fixtures.is_symlink() or not fixtures.is_dir() or {
                path.name for path in fixtures.iterdir()} != set(REFERENCES):
            raise ValidationError(f"{fixtures}: fixture root must contain exactly {', '.join(sorted(REFERENCES))}")
        runner = options.runner.resolve() if options.runner is not None else None
        if runner is not None and not runner.is_file():
            raise ValidationError(f"candidate runner is missing: {runner}")
        for mode, entries in cases_by_mode.items():
            mode_root = fixtures / mode
            ids = {case["id"] for case in entries}
            if mode_root.is_symlink() or not mode_root.is_dir() or {
                    path.name for path in mode_root.iterdir()} != ids:
                raise ValidationError(f"{mode_root}: case directories do not exactly match {REFERENCES[mode]['cases']}")

        with tempfile.TemporaryDirectory(prefix="pcm-mixer-validation.") as temp:
            temporary = Path(temp)
            for mode, entries in cases_by_mode.items():
                mode_root = fixtures / mode
                for case in entries:
                    validate_one(case, mode_root / case["id"], manifest_schema, root, runner, temporary)
        divergence_count = validate_divergences(root, fixtures, cases_by_mode)
        comparison = " and compared the candidate twice" if runner is not None else ""
        print(f"strictly self-checked {len(all_ids)} pinned PCM mixer fixtures{comparison}; "
              f"asserted {divergence_count} cross-mode FIFO divergences")
    except (ValidationError, case_format.CaseFormatError, OSError) as error:
        print(f"pcm-mixer oracle validation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
