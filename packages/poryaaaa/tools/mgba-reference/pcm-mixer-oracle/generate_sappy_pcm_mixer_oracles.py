#!/usr/bin/env python3
"""Build pinned vanilla Sappy fixtures and publish canonical mGBA FIFO traces."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
from typing import Any

import generate_pcm_mixer_oracles as common

SAPPY_COMMIT = "9a83a2bbe8e097e62c00f1dbd56849766775d7b6"
SAPPY_REPOSITORY = "https://github.com/pret/pokeemerald"
DELTA_BYTES = bytes((0, 1, 4, 9, 16, 25, 36, 49, 192, 207, 220, 231, 240, 247, 252, 255))


def require_clean_sappy(repo: Path) -> str:
    if common.git_output(repo, "rev-parse", "HEAD") != SAPPY_COMMIT:
        raise common.OracleError(f"Sappy source is not pinned to {SAPPY_COMMIT}")
    status = common.git_output(repo, "status", "--porcelain", "--untracked-files=all")
    if status:
        raise common.OracleError(f"Sappy source is not clean: {status}")
    required = (repo / "src" / "m4a_1.s", repo / "src" / "m4a_tables.c")
    if not all(path.is_file() and not path.is_symlink() for path in required):
        raise common.OracleError("pinned Sappy mixer sources are missing")
    return common.source_tree_hash(repo)


def extract_mixer_source(repo: Path, destination: Path) -> None:
    source = repo / "src" / "m4a_1.s"
    text = source.read_text(encoding="utf-8")
    first_function = "\tthumb_func_start umul3232H32\n"
    mixer_start = "\tthumb_func_start SoundMainRAM\n"
    mixer_end = "\tarm_func_end SoundMainRAM_Unk2\n"
    scratch_start = "\t.bss\nsDecodingBuffer: @ Used as a buffer for audio decoded from compressed DPCM\n"
    scratch_end = "\t.size sDecodingBuffer, .-sDecodingBuffer\n"
    positions = tuple(text.find(marker) for marker in (first_function, mixer_start, mixer_end, scratch_start, scratch_end))
    if any(position < 0 for position in positions):
        raise common.OracleError("pinned src/m4a_1.s does not contain the expected mixer boundaries")
    prefix_end, start, end_start, bss_start, bss_end_start = positions
    end = end_start + len(mixer_end)
    bss_end = bss_end_start + len(scratch_end)
    if not (prefix_end < start < end <= bss_start < bss_end) or text[bss_end:].strip():
        raise common.OracleError("pinned src/m4a_1.s mixer/scratch ordering changed")
    destination.write_text(text[:prefix_end] + text[start:end] + "\n" + text[bss_start:bss_end],
                           encoding="utf-8", newline="")


def build_reference(sappy: Path, tools: dict[str, str], build: Path) -> tuple[Path, Path]:
    extracted = build / "m4a_1-mixer.s"
    original = build / "m4a_1-mixer.o"
    mixer = build / "m4a_1-mixer-iwram.o"
    extract_mixer_source(sappy, extracted)
    common.run_capture([tools["assembler"], "-mcpu=arm7tdmi", "-I", str(sappy),
                        "-o", str(original), str(extracted)], cwd=sappy)
    common.run_capture([tools["objcopy"], "--rename-section",
                        ".text=.iwram_mixer,alloc,load,code,contents", str(original), str(mixer)])

    tables = build / "m4a_tables.o"
    delta = build / "m4a_delta_table.o"
    delta_bytes = build / "m4a_delta_table.bin"
    common.run_capture([
        tools["compiler"], "-mcpu=arm7tdmi", "-mthumb", "-ffreestanding", "-fdata-sections",
        "-ffunction-sections", "-fno-common", "-I", str(sappy / "include"), "-I", str(sappy),
        "-c", str(sappy / "src" / "m4a_tables.c"), "-o", str(tables),
    ], cwd=sappy)
    common.run_capture([tools["objcopy"], "--only-section=.rodata.gDeltaEncodingTable",
                        str(tables), str(delta)])
    common.run_capture([tools["objcopy"], "--dump-section",
                        f".rodata.gDeltaEncodingTable={delta_bytes}", str(delta)])
    if delta_bytes.read_bytes() != DELTA_BYTES:
        raise common.OracleError("pinned gDeltaEncodingTable bytes differ from vanilla Sappy")
    return mixer, delta


def defined_symbols(readelf: bytes) -> dict[str, int]:
    try:
        text = readelf.decode("utf-8")
    except UnicodeDecodeError as error:
        raise common.OracleError("readelf symbol output was not UTF-8") from error
    symbols: dict[str, int] = {}
    for line in text.splitlines():
        fields = line.split()
        if len(fields) >= 8 and fields[0].rstrip(":").isdecimal() and fields[6] != "UND":
            try:
                symbols[fields[7]] = int(fields[1], 16)
            except ValueError:
                continue
    return symbols


def build_fixture(source: Path, mixer: Path, delta: Path, oracle: Path, inputs: Path,
                  tools: dict[str, str], build: Path) -> tuple[Path, Path, bytes]:
    fixture_object = build / "fixture.o"
    elf = build / "fixture.elf"
    rom = build / "fixture.rom"
    descriptor_path = build / "fixture-case.bin"
    common.run_capture([tools["assembler"], "-mcpu=arm7tdmi", "-mthumb-interwork", "-I", str(inputs),
                        "-o", str(fixture_object), str(source)])
    common.run_capture([tools["linker"], "--build-id=none", "-T", str(oracle), "-o", str(elf),
                        str(fixture_object), str(mixer), str(delta)])
    symbols = defined_symbols(common.run_capture([tools["readelf"], "-Ws", str(elf)]))
    required = {
        "SoundMainRAM", "SoundMainRAM_Reverb", "SoundMainRAM_Unk1", "SoundMainRAM_Unk2",
        "gDeltaEncodingTable", "sDecodingBuffer",
    }
    if not required.issubset(symbols):
        raise common.OracleError(f"{source}: linked ELF lacks real vanilla symbols {sorted(required - symbols.keys())}")
    mixer_symbols = required - {"gDeltaEncodingTable", "sDecodingBuffer"}
    if not all(0x03000000 <= symbols[name] < 0x03008000 for name in mixer_symbols):
        raise common.OracleError(f"{source}: vanilla mixer symbols are not linked in IWRAM")
    if not 0x08000000 <= symbols["gDeltaEncodingTable"] < 0x08100000:
        raise common.OracleError(f"{source}: vanilla decoder table is not linked in ROM")
    if not 0x02000000 <= symbols["sDecodingBuffer"] < 0x02040000:
        raise common.OracleError(f"{source}: vanilla decoder cache is not linked in EWRAM")
    common.run_capture([tools["objcopy"], "--dump-section", f".fixture_case={descriptor_path}", str(elf)])
    common.run_capture([tools["objcopy"], "-O", "binary", str(elf), str(rom)])
    image = rom.read_bytes()
    if len(image) > common.ROM_SIZE:
        raise common.OracleError(f"fixture ROM exceeds {common.ROM_SIZE} bytes")
    rom.write_bytes(image + bytes(common.ROM_SIZE - len(image)))
    descriptor = descriptor_path.read_bytes()
    return elf, rom, descriptor

def make_manifest(case: dict[str, Any], source_tree: str, source: Path,
                  elf: Path, rom: Path, trace: bytes, rows: list[common.ParsedRow],
                  tools: dict[str, str], generator: Path, cases_path: Path) -> dict[str, Any]:
    begin = next(row for row in rows if row.kind == "BEGIN")
    end = next(row for row in rows if row.kind == "END")
    return {
        "schema": "poryaaaa.pcm-mixer-oracle",
        "version": 1,
        "case": case["id"],
        "mode": "sappy",
        "reference": {
            "repository": SAPPY_REPOSITORY,
            "commit": SAPPY_COMMIT,
            "source_tree_sha256": source_tree,
            "patch_sha256": None,
            "feature_flags": {},
        },
        "toolchain": {name: common.tool_version(path) for name, path in tools.items()},
        "observation": {"mgba_commit": common.MGBA_COMMIT,
                        "patch_sha256": common.MGBA_OBSERVATION_PATCH_SHA256},
        "hashes": {
            "generator": common.sha256_file(generator),
            "cases": common.sha256_file(cases_path),
            "input": common.sha256_file(source),
            "elf": common.sha256_file(elf),
            "rom": common.sha256_file(rom),
            "trace": common.sha256_bytes(trace),
        },
        "settings": case["settings"],
        "range": {
            "begin_cycle": begin.cycle,
            "end_cycle": end.cycle,
            "row_count": len(common.fifo_rows(rows)),
            "event_hash": common.fifo_event_hash(rows),
        },
    }


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sappy-source", required=True, type=Path)
    parser.add_argument("--mgba-source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--recorder")
    parser.add_argument("--assembler")
    parser.add_argument("--compiler")
    parser.add_argument("--linker")
    parser.add_argument("--objcopy")
    parser.add_argument("--readelf")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    options = parse_args(argv)
    root = Path(__file__).resolve().parent
    cases_path = root / "cases-sappy.json"
    schema_path = root / "cases.schema.json"
    manifest_schema_path = root / "manifest.schema.json"
    linker = root / "oracle-sappy.ld"
    mgba_patch = root.parent / "mgba-audio-observation.patch"
    temporary: Path | None = None
    try:
        entries = common.validate_cases(common.load_json(cases_path), common.load_json(schema_path), cases_path, root)
        if any(case["mode"] != "sappy" for case in entries):
            raise common.OracleError("cases-sappy.json contains a non-Sappy case")
        if {case["settings"]["max_channels"] for case in entries} != set(range(16)):
            raise common.OracleError("cases-sappy.json must exercise every voice count from 0 through 15")
        manifest_schema = common.load_json(manifest_schema_path)
        if not isinstance(manifest_schema, dict):
            raise common.OracleError("manifest.schema.json must be an object")
        if common.sha256_file(mgba_patch) != common.MGBA_OBSERVATION_PATCH_SHA256:
            raise common.OracleError("mgba-audio-observation.patch hash mismatch")
        sappy = options.sappy_source.resolve()
        mgba = options.mgba_source.resolve()
        source_tree = require_clean_sappy(sappy)
        common.require_patched_mgba(mgba, mgba_patch)
        tools = {
            "assembler": common.command_path(options.assembler, "ARM_NONE_EABI_AS", "arm-none-eabi-as"),
            "compiler": common.command_path(options.compiler, "ARM_NONE_EABI_GCC", "arm-none-eabi-gcc"),
            "linker": common.command_path(options.linker, "ARM_NONE_EABI_LD", "arm-none-eabi-ld"),
            "objcopy": common.command_path(options.objcopy, "ARM_NONE_EABI_OBJCOPY", "arm-none-eabi-objcopy"),
            "readelf": common.command_path(options.readelf, "ARM_NONE_EABI_READELF", "arm-none-eabi-readelf"),
        }
        recorder = common.find_recorder(options.recorder)
        output = options.output.resolve()
        destination_root = output / "sappy"
        if destination_root.exists():
            raise common.OracleError(f"refusing to overwrite Sappy fixtures in {destination_root}")
        output.mkdir(parents=True, exist_ok=True)
        temporary = Path(tempfile.mkdtemp(prefix="sappy-pcm-oracles.", dir=output.parent))
        staged = temporary / "sappy"
        staged.mkdir()
        reference_build = temporary / "reference"
        reference_build.mkdir()
        mixer, delta = build_reference(sappy, tools, reference_build)
        for case in entries:
            case_build = temporary / case["id"]
            case_build.mkdir()
            source = root / case["source"]
            elf, rom, descriptor = build_fixture(source, mixer, delta, linker, root / "inputs", tools, case_build)
            descriptor_values = common.verify_descriptor(case, source, descriptor, rom)
            raw_trace = case_build / "raw.trace"
            common.run_observation(recorder, rom, raw_trace, case_build / "native")
            rows = common.parse_trace(raw_trace.read_bytes(), str(raw_trace))
            fifo = common.fifo_rows(rows)
            block_count = descriptor_values["FIXTURE_BLOCK_COUNT"]
            if len(fifo) != block_count * 48:
                raise common.OracleError(f"{case['id']}: expected {block_count * 48} FIFO writes, got {len(fifo)}")
            expect_nonzero = bool(descriptor_values["FIXTURE_FLAGS"] & 1)
            if expect_nonzero != any(row.value != 0 for row in fifo):
                raise common.OracleError(f"{case['id']}: FIFO nonzero expectation does not match the reference")
            trace = common.canonical_trace(rows)
            rows = common.parse_trace(trace, f"{case['id']}/expected.trace")
            destination = staged / case["id"]
            destination.mkdir()
            (destination / "expected.trace").write_bytes(trace)
            manifest = make_manifest(case, source_tree, source, elf, rom, trace, rows, tools,
                                     Path(__file__).resolve(), cases_path)
            common.validate_schema_value(manifest, manifest_schema, f"{case['id']}/manifest.json")
            (destination / "manifest.json").write_text(
                json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        os.replace(staged, destination_root)
    except (common.OracleError, common.case_format.CaseFormatError, OSError, UnicodeError) as error:
        print(f"Sappy PCM mixer oracle generation failed: {error}", file=sys.stderr)
        return 1
    finally:
        if temporary is not None:
            shutil.rmtree(temporary, ignore_errors=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
