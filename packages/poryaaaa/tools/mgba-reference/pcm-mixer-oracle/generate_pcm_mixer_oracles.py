#!/usr/bin/env python3
"""Build pinned iPatix fixtures and publish canonical mGBA FIFO traces."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any, Iterable

import case_format

IPATIX_COMMIT = "2bd31435101e4c3a4af568a6ed33d7cc9f1a6da9"
MGBA_COMMIT = "afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9"
MGBA_OBSERVATION_PATCH_SHA256 = "3e5dec217b04917733767339e95dbb2b1eb64f292345603e69c57c47ce64a017"
IPATIX_REVERB_PATCH_SHA256 = "063dc98e03581bab797e2112aa42108201272b3710f0664f204539d70eecf38b"
IPATIX_REPOSITORY = "https://github.com/ipatix/gba-hq-mixer"
CLOCK_HZ = 16_777_216
UINT32_MAX = (1 << 32) - 1
UINT64_MAX = (1 << 64) - 1
ROM_SIZE = 1 << 20
HEADER = "PORYAAAA_AUDIO_TRACE 1"



class OracleError(RuntimeError):
    """A fail-closed generation error."""


class ParsedRow:
    __slots__ = ("kind", "cycle", "order", "width", "address", "value", "timer")

    def __init__(self, kind: str, cycle: int, order: int, width: int = 0,
                 address: int = 0, value: int = 0, timer: int = 0) -> None:
        self.kind = kind
        self.cycle = cycle
        self.order = order
        self.width = width
        self.address = address
        self.value = value
        self.timer = timer


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    try:
        return sha256_bytes(path.read_bytes())
    except OSError as error:
        raise OracleError(f"cannot hash {path}: {error}") from error


def load_json(path: Path) -> Any:
    def reject_duplicate(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise OracleError(f"{path}: duplicate JSON key {key!r}")
            result[key] = value
        return result

    try:
        return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=reject_duplicate)
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise OracleError(f"{path}: invalid JSON: {error}") from error


def validate_schema_value(value: Any, schema: dict[str, Any], where: str) -> None:
    expected = schema.get("type")
    if expected is not None:
        expected_types = expected if isinstance(expected, list) else [expected]
        actual = (
            "null" if value is None else
            "boolean" if isinstance(value, bool) else
            "integer" if isinstance(value, int) else
            "number" if isinstance(value, float) else
            "string" if isinstance(value, str) else
            "array" if isinstance(value, list) else
            "object" if isinstance(value, dict) else "unknown"
        )
        if actual not in expected_types and not (actual == "integer" and "number" in expected_types):
            raise OracleError(f"{where}: expected {expected}, got {actual}")
    if "const" in schema and value != schema["const"]:
        raise OracleError(f"{where}: expected {schema['const']!r}")
    if "enum" in schema and value not in schema["enum"]:
        raise OracleError(f"{where}: value is not in the allowed enum")
    if isinstance(value, str):
        if "minLength" in schema and len(value) < schema["minLength"]:
            raise OracleError(f"{where}: string is too short")
        if "pattern" in schema and re.fullmatch(schema["pattern"], value) is None:
            raise OracleError(f"{where}: string does not match {schema['pattern']!r}")
    if isinstance(value, int) and not isinstance(value, bool):
        if "minimum" in schema and value < schema["minimum"]:
            raise OracleError(f"{where}: number is below minimum")
        if "maximum" in schema and value > schema["maximum"]:
            raise OracleError(f"{where}: number is above maximum")
    if isinstance(value, dict):
        properties = schema.get("properties", {})
        for key in schema.get("required", []):
            if key not in value:
                raise OracleError(f"{where}: missing required field {key!r}")
        if schema.get("additionalProperties") is False:
            extras = set(value) - set(properties)
            if extras:
                raise OracleError(f"{where}: unknown field(s): {', '.join(sorted(extras))}")
        for key, child in value.items():
            if key in properties:
                validate_schema_value(child, properties[key], f"{where}.{key}")
    if isinstance(value, list):
        if "minItems" in schema and len(value) < schema["minItems"]:
            raise OracleError(f"{where}: array is too short")
        for index, child in enumerate(value):
            if "items" in schema:
                validate_schema_value(child, schema["items"], f"{where}[{index}]")


def validate_cases(cases: Any, schema: Any, cases_path: Path, root: Path) -> list[dict[str, Any]]:
    if not isinstance(cases, dict) or not isinstance(schema, dict):
        raise OracleError("cases and cases.schema must be JSON objects")
    validate_schema_value(cases, schema, str(cases_path))
    seen: set[str] = set()
    for case in cases["cases"]:
        if case["id"] in seen:
            raise OracleError(f"{cases_path}: duplicate case id {case['id']!r}")
        seen.add(case["id"])
        relative = Path(case["source"])
        source = root / relative
        if relative.is_absolute() or ".." in relative.parts or relative.parent != Path("inputs"):
            raise OracleError(f"{cases_path}: source must be a direct inputs/*.s path")
        if source.is_symlink() or not source.is_file():
            raise OracleError(f"{cases_path}: missing regular source {case['source']}")
    return cases["cases"]


def run_capture(argv: list[str], cwd: Path | None = None) -> bytes:
    try:
        completed = subprocess.run(argv, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    except OSError as error:
        raise OracleError(f"cannot execute {' '.join(argv)}: {error}") from error
    if completed.returncode != 0:
        stderr = completed.stderr.decode("utf-8", errors="replace").strip()
        raise OracleError(f"{' '.join(argv)} failed with status {completed.returncode}: {stderr}")
    return completed.stdout


def command_path(value: str | None, environment: str, default: str) -> str:
    candidate = value or os.environ.get(environment, default)
    found = shutil.which(candidate)
    if found is None:
        raise OracleError(f"required tool is unavailable: {candidate}")
    return found


def tool_version(tool: str) -> str:
    try:
        text = run_capture([tool, "--version"]).decode("utf-8")
    except UnicodeDecodeError as error:
        raise OracleError(f"{tool} --version did not produce UTF-8") from error
    result = text.replace("\r\n", "\n").replace("\r", "\n").rstrip("\n")
    if not result:
        raise OracleError(f"{tool} --version produced empty output")
    return result


def git_output(repo: Path, *args: str) -> str:
    try:
        return run_capture(["git", *args], cwd=repo).decode("utf-8").strip()
    except UnicodeDecodeError as error:
        raise OracleError(f"git output for {repo} was not UTF-8") from error


def source_tree_hash(repo: Path) -> str:
    digest = hashlib.sha256()
    for raw_path in run_capture(["git", "ls-files", "-z"], cwd=repo).split(b"\0"):
        if not raw_path:
            continue
        path = repo / raw_path.decode("utf-8")
        digest.update(raw_path)
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def require_clean_ipatix(repo: Path) -> str:
    if git_output(repo, "rev-parse", "HEAD") != IPATIX_COMMIT:
        raise OracleError(f"iPatix source is not pinned to {IPATIX_COMMIT}")
    status = git_output(repo, "status", "--porcelain", "--untracked-files=all")
    if status:
        raise OracleError(f"iPatix source is not clean: {status}")
    return source_tree_hash(repo)


def require_patched_mgba(repo: Path, patch_path: Path) -> None:
    if git_output(repo, "rev-parse", "HEAD") != MGBA_COMMIT:
        raise OracleError(f"mGBA source is not pinned to {MGBA_COMMIT}")
    status = git_output(repo, "status", "--porcelain", "--untracked-files=all")
    if any(line.startswith("?? ") for line in status.splitlines()):
        raise OracleError(f"mGBA source has untracked files: {status}")
    expected = {"include/mgba/internal/gba/audio.h", "src/gba/audio.c", "src/gba/io.c"}
    changed = set(git_output(repo, "diff", "--name-only", MGBA_COMMIT).splitlines())
    if changed != expected:
        raise OracleError("mGBA changes are not exactly the observation patch")
    if sha256_bytes(run_capture(["git", "diff", "--binary", MGBA_COMMIT], cwd=repo)) != sha256_file(patch_path):
        raise OracleError("mGBA worktree delta does not match mgba-audio-observation.patch")


def apply_ipatix_patch(source: Path, destination: Path, patch_path: Path) -> Path:
    if sha256_file(patch_path) != IPATIX_REVERB_PATCH_SHA256:
        raise OracleError("ipatix-enable-reverb.patch hash mismatch")
    shutil.copytree(source, destination)
    mixer = destination / "m4a_hq_mixer.s"
    text = mixer.read_text(encoding="utf-8")
    patched, count = re.subn(r"(?m)^(\s*\.equ\s+ENABLE_REVERB\s*,\s*)0(\s*(?:@.*)?)$", r"\g<1>1\g<2>", text, count=1)
    if count != 1:
        raise OracleError("pinned iPatix source does not have the expected ENABLE_REVERB definition")
    mixer.write_text(patched, encoding="utf-8", newline="")
    return mixer


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


def parse_trace(data: bytes, source_name: str) -> list[ParsedRow]:
    prefix = (HEADER + "\n").encode("ascii")
    if not data.startswith(prefix) or not data.endswith(b"\n"):
        raise OracleError(f"{source_name}: malformed trace header or final line")
    clock_seen = False
    open_interval = False
    closed_interval = False
    last: tuple[int, int] | None = None
    rows: list[ParsedRow] = []
    for line_number, raw in enumerate(data[len(prefix):].splitlines(), 2):
        try:
            line = raw.decode("utf-8")
        except UnicodeDecodeError as error:
            raise OracleError(f"{source_name}:{line_number}: invalid UTF-8") from error
        if not line or line.startswith("#"):
            continue
        if "  " in line or "\t" in line or line.startswith(" ") or line.endswith(" "):
            raise OracleError(f"{source_name}:{line_number}: non-canonical spacing")
        tokens = line.split(" ")
        if tokens[0] == "CLOCK":
            if clock_seen or last is not None or tokens != ["CLOCK", str(CLOCK_HZ)]:
                raise OracleError(f"{source_name}:{line_number}: invalid CLOCK")
            clock_seen = True
            continue
        if not clock_seen or len(tokens) < 3:
            raise OracleError(f"{source_name}:{line_number}: event precedes CLOCK")
        cycle = parse_uint(tokens[1], UINT64_MAX)
        order = parse_uint(tokens[2], UINT32_MAX)
        if cycle is None or order is None or (last is not None and (cycle, order) <= last):
            raise OracleError(f"{source_name}:{line_number}: invalid event position")
        kind = tokens[0]
        if kind == "BEGIN" and len(tokens) == 3 and not open_interval and not closed_interval:
            open_interval = True
            rows.append(ParsedRow(kind, cycle, order))
        elif kind == "END" and len(tokens) == 3 and open_interval and not closed_interval:
            open_interval = False
            closed_interval = True
            rows.append(ParsedRow(kind, cycle, order))
        elif not open_interval or closed_interval:
            raise OracleError(f"{source_name}:{line_number}: event outside capture interval")
        elif kind == "WRITE" and len(tokens) == 6:
            width = parse_uint(tokens[3], 4)
            address = parse_hex(tokens[4])
            value = parse_hex(tokens[5])
            if width is None or width == 0 or address is None or value is None:
                raise OracleError(f"{source_name}:{line_number}: invalid WRITE")
            rows.append(ParsedRow(kind, cycle, order, width, address, value))
        elif kind == "TIMER" and len(tokens) == 4 and parse_uint(tokens[3], 1) is not None:
            rows.append(ParsedRow(kind, cycle, order, timer=int(tokens[3])))
        elif kind == "SAMPLE" and len(tokens) == 3:
            rows.append(ParsedRow(kind, cycle, order))
        else:
            raise OracleError(f"{source_name}:{line_number}: invalid event")
        last = (cycle, order)
    if not clock_seen or open_interval or not closed_interval:
        raise OracleError(f"{source_name}: trace does not contain one closed interval")
    return rows


def canonical_trace(rows: Iterable[ParsedRow]) -> bytes:
    lines = [HEADER, f"CLOCK {CLOCK_HZ}"]
    for row in rows:
        if row.kind in ("BEGIN", "END", "SAMPLE"):
            lines.append(f"{row.kind} {row.cycle} {row.order}")
        elif row.kind == "WRITE":
            lines.append(f"WRITE {row.cycle} {row.order} {row.width} 0x{row.address:08X} 0x{row.value:08X}")
        else:
            lines.append(f"TIMER {row.cycle} {row.order} {row.timer}")
    return ("\n".join(lines) + "\n").encode("utf-8")


def fifo_rows(rows: Iterable[ParsedRow]) -> list[ParsedRow]:
    return [row for row in rows if row.kind == "WRITE" and row.width == 4 and row.address in (0x040000A0, 0x040000A4)]


def fifo_event_hash(rows: Iterable[ParsedRow]) -> str:
    data = b"".join(
        f"WRITE {row.cycle} {row.order} 4 0x{row.address:08X} 0x{row.value:08X}\n".encode("ascii")
        for row in fifo_rows(rows)
    )
    return sha256_bytes(data)


def find_recorder(explicit: str | None) -> str:
    candidates = [explicit, os.environ.get("PORYAAAA_MGBA_ORACLE_RECORDER")]
    package = Path(__file__).resolve().parents[3]
    candidates.append(str(package / "build" / "mgba_mp2k_reference"))
    for candidate in candidates:
        if candidate and Path(candidate).is_file() and os.access(candidate, os.X_OK):
            return str(Path(candidate).resolve())
    raise OracleError("fixture-aware mGBA recorder is unavailable; pass --recorder")


def build_mixer(mixer_source: Path, tools: dict[str, str], build: Path) -> Path:
    original = build / "mixer.o"
    writable = build / "mixer-iwram.o"
    run_capture([tools["assembler"], "-mcpu=arm7tdmi", "--defsym", "SoundMainRAM_MixBuffer=0x02012000",
                 "-o", str(original), str(mixer_source)])
    run_capture([tools["objcopy"], "--rename-section", ".text=.iwram_mixer,alloc,load,code,contents",
                 str(original), str(writable)])
    return writable


def build_fixture(source: Path, mixer: Path, oracle: Path, inputs: Path,
                  tools: dict[str, str], build: Path) -> tuple[Path, Path, bytes]:
    fixture_object = build / "fixture.o"
    elf = build / "fixture.elf"
    rom = build / "fixture.rom"
    descriptor_path = build / "fixture-case.bin"
    run_capture([tools["assembler"], "-mcpu=arm7tdmi", "-mthumb-interwork", "-I", str(inputs),
                 "-o", str(fixture_object), str(source)])
    run_capture([tools["linker"], "--build-id=none", "-T", str(oracle), "-o", str(elf),
                 str(fixture_object), str(mixer)])
    run_capture([tools["readelf"], "-S", str(elf)])
    run_capture([tools["objcopy"], "--dump-section", f".fixture_case={descriptor_path}", str(elf)])
    run_capture([tools["objcopy"], "-O", "binary", str(elf), str(rom)])
    image = rom.read_bytes()
    if len(image) > ROM_SIZE:
        raise OracleError(f"fixture ROM exceeds {ROM_SIZE} bytes")
    rom.write_bytes(image + bytes(ROM_SIZE - len(image)))
    descriptor = descriptor_path.read_bytes()
    return elf, rom, descriptor


def run_observation(recorder: str, rom: Path, trace: Path, prefix: Path) -> dict[str, Any]:
    run_capture([recorder, "--rom", str(rom), "--fixture", "--capture-stage", "native",
                 "--duration-seconds", "0.12", "--trace-output", str(trace),
                 "--native-output-prefix", str(prefix)])
    report = load_json(prefix.with_suffix(".json"))
    if not isinstance(report, dict) or report.get("mgba_commit") != MGBA_COMMIT:
        raise OracleError("recorder report has the wrong mGBA commit")
    if report.get("mgba_observation_patch_sha256") != MGBA_OBSERVATION_PATCH_SHA256:
        raise OracleError("recorder report has the wrong observation patch")
    if report.get("rom_sha256") != sha256_file(rom) or report.get("trace_sha256") != sha256_file(trace):
        raise OracleError("recorder report artifact hashes do not match")
    return report


def verify_descriptor(case: dict[str, Any], source: Path, linked: bytes, rom: Path) -> dict[str, int]:
    values, declared = case_format.parse_source(source, case["mode"])
    embedded_size = case_format.embedded_descriptor_size(declared)
    if len(linked) != embedded_size or linked != declared[:embedded_size]:
        raise OracleError(
            f"{case['id']}: source descriptor differs from the .fixture_case bytes in the linked reference ELF")
    image = rom.read_bytes()
    offset = image.find(linked)
    if offset < 0 or image.find(linked, offset + 1) >= 0:
        raise OracleError(f"{case['id']}: linked descriptor is not unique in the reference ROM")
    if image[offset:offset + len(declared)] != declared:
        raise OracleError(
            f"{case['id']}: candidate descriptor memory differs from the linked reference ROM")
    actual = case_format.settings(values)
    if actual != case["settings"]:
        raise OracleError(
            f"{case['id']}: case settings differ from the source descriptor: {actual}")
    return values


def make_manifest(case: dict[str, Any], source_tree: str, source: Path,
                  elf: Path, rom: Path, trace: bytes, rows: list[ParsedRow],
                  tools: dict[str, str], generator: Path, cases_path: Path) -> dict[str, Any]:
    begin = next(row for row in rows if row.kind == "BEGIN")
    end = next(row for row in rows if row.kind == "END")
    return {
        "schema": "poryaaaa.pcm-mixer-oracle",
        "version": 1,
        "case": case["id"],
        "mode": "ipatix",
        "reference": {
            "repository": IPATIX_REPOSITORY,
            "commit": IPATIX_COMMIT,
            "source_tree_sha256": source_tree,
            "patch_sha256": IPATIX_REVERB_PATCH_SHA256,
            "feature_flags": {"POKE_CHN_INIT": 1, "ENABLE_STEREO": 1, "ENABLE_REVERB": 1, "ENABLE_DMA": 1},
        },
        "toolchain": {name: tool_version(path) for name, path in tools.items()},
        "observation": {"mgba_commit": MGBA_COMMIT, "patch_sha256": MGBA_OBSERVATION_PATCH_SHA256},
        "hashes": {
            "generator": sha256_file(generator),
            "cases": sha256_file(cases_path),
            "input": sha256_file(source),
            "elf": sha256_file(elf),
            "rom": sha256_file(rom),
            "trace": sha256_bytes(trace),
        },
        "settings": case["settings"],
        "range": {
            "begin_cycle": begin.cycle,
            "end_cycle": end.cycle,
            "row_count": len(fifo_rows(rows)),
            "event_hash": fifo_event_hash(rows),
        },
    }


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ipatix-source", required=True, type=Path)
    parser.add_argument("--mgba-source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--recorder")
    parser.add_argument("--assembler")
    parser.add_argument("--linker")
    parser.add_argument("--objcopy")
    parser.add_argument("--readelf")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    options = parse_args(argv)
    root = Path(__file__).resolve().parent
    cases_path = root / "cases.json"
    schema_path = root / "cases.schema.json"
    manifest_schema_path = root / "manifest.schema.json"
    linker = root / "oracle.ld"
    reverb_patch = root / "ipatix-enable-reverb.patch"
    mgba_patch = root.parent / "mgba-audio-observation.patch"
    temp_parent: Path | None = None
    try:
        entries = validate_cases(load_json(cases_path), load_json(schema_path), cases_path, root)
        manifest_schema = load_json(manifest_schema_path)
        if not isinstance(manifest_schema, dict):
            raise OracleError("manifest.schema.json must be an object")
        if sha256_file(mgba_patch) != MGBA_OBSERVATION_PATCH_SHA256:
            raise OracleError("mgba-audio-observation.patch hash mismatch")
        ipatix = options.ipatix_source.resolve()
        mgba = options.mgba_source.resolve()
        source_tree = require_clean_ipatix(ipatix)
        require_patched_mgba(mgba, mgba_patch)
        tools = {
            "assembler": command_path(options.assembler, "ARM_NONE_EABI_AS", "arm-none-eabi-as"),
            "linker": command_path(options.linker, "ARM_NONE_EABI_LD", "arm-none-eabi-ld"),
            "objcopy": command_path(options.objcopy, "ARM_NONE_EABI_OBJCOPY", "arm-none-eabi-objcopy"),
            "readelf": command_path(options.readelf, "ARM_NONE_EABI_READELF", "arm-none-eabi-readelf"),
        }
        recorder = find_recorder(options.recorder)
        output = options.output.resolve()
        if output.exists() and any(output.iterdir()):
            raise OracleError(f"refusing to overwrite non-empty fixture directory {output}")
        output.parent.mkdir(parents=True, exist_ok=True)
        temp_parent = Path(tempfile.mkdtemp(prefix="pcm-mixer-oracles.", dir=output.parent))
        staged = temp_parent / "fixtures"
        staged.mkdir()
        patched = temp_parent / "ipatix"
        mixer_source = apply_ipatix_patch(ipatix, patched, reverb_patch)
        mixer_build = temp_parent / "mixer"
        mixer_build.mkdir()
        mixer = build_mixer(mixer_source, tools, mixer_build)
        for case in entries:
            case_build = temp_parent / case["id"]
            case_build.mkdir()
            source = root / case["source"]
            elf, rom, descriptor = build_fixture(source, mixer, linker, root / "inputs", tools, case_build)
            descriptor_values = verify_descriptor(case, source, descriptor, rom)
            raw_trace = case_build / "raw.trace"
            run_observation(recorder, rom, raw_trace, case_build / "native")
            rows = parse_trace(raw_trace.read_bytes(), str(raw_trace))
            fifo = fifo_rows(rows)
            block_count = descriptor_values["FIXTURE_BLOCK_COUNT"]
            if len(fifo) != block_count * 48:
                raise OracleError(f"{case['id']}: expected {block_count * 48} FIFO writes, got {len(fifo)}")
            values = [row.value for row in fifo]
            expect_nonzero = bool(descriptor_values["FIXTURE_FLAGS"] & 1)
            if expect_nonzero != any(value != 0 for value in values):
                raise OracleError(f"{case['id']}: FIFO nonzero expectation does not match the reference")
            trace = canonical_trace(rows)
            rows = parse_trace(trace, f"{case['id']}/expected.trace")
            destination = staged / "ipatix" / case["id"]
            destination.mkdir(parents=True)
            (destination / "expected.trace").write_bytes(trace)
            manifest = make_manifest(case, source_tree, source, elf, rom, trace, rows, tools,
                                     Path(__file__).resolve(), cases_path)
            validate_schema_value(manifest, manifest_schema, f"{case['id']}/manifest.json")
            (destination / "manifest.json").write_text(
                json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        if output.exists():
            output.rmdir()
        os.replace(staged, output)
    except (OracleError, case_format.CaseFormatError, OSError, UnicodeError) as error:
        print(f"pcm-mixer oracle generation failed: {error}", file=sys.stderr)
        return 1
    finally:
        if temp_parent is not None:
            shutil.rmtree(temp_parent, ignore_errors=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
