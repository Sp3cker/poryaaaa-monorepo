#!/usr/bin/env python3
"""Canonical source descriptor and binary transaction format for PCM fixtures."""

from __future__ import annotations

import json
import re
import struct
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent
FORMAT_PATH = ROOT / "case-format.json"
C_HEADER_PATH = ROOT.parents[2] / "test" / "pcm_mixer_case_format.h"
BEGIN_MARKER = "@ PORYAAAA_PCM_DESCRIPTOR 1 BEGIN"
END_MARKER = "@ PORYAAAA_PCM_DESCRIPTOR 1 END"
EQU_RE = re.compile(r"\.equ ([A-Z][A-Z0-9_]*), (-?(?:0|[1-9][0-9]*|0x[0-9A-F]+))")


class CaseFormatError(RuntimeError):
    """The declarative case contract or one source descriptor is invalid."""


def _load_format() -> dict[str, Any]:
    try:
        value = json.loads(FORMAT_PATH.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise CaseFormatError(f"cannot load {FORMAT_PATH}: {error}") from error
    if not isinstance(value, dict) or value.get("schema") != "poryaaaa.pcm-mixer-case-format" or value.get("version") != 1:
        raise CaseFormatError(f"{FORMAT_PATH}: invalid case format identity")
    return value


FORMAT = _load_format()
DESCRIPTOR = FORMAT["descriptor"]
CASE = FORMAT["case"]
CASE_HEADER = struct.Struct(CASE["header_struct"])
SCHEDULE_ROW = struct.Struct(CASE["schedule_struct"])


def _all_source_fields() -> list[dict[str, Any]]:
    fields: dict[str, dict[str, Any]] = {}
    for field in [*DESCRIPTOR["fields"], *DESCRIPTOR["source_only_fields"]]:
        symbol = field["symbol"]
        previous = fields.get(symbol)
        if previous is not None:
            if previous["minimum"] != field["minimum"] or previous["maximum"] != field["maximum"]:
                raise CaseFormatError(f"{FORMAT_PATH}: conflicting limits for {symbol}")
            continue
        fields[symbol] = field
    return list(fields.values())


SOURCE_FIELDS = _all_source_fields()
SOURCE_FIELDS_BY_SYMBOL = {field["symbol"]: field for field in SOURCE_FIELDS}


def _validate_contract() -> None:
    if CASE_HEADER.size != CASE["header_size"] or SCHEDULE_ROW.size != CASE["schedule_row_size"]:
        raise CaseFormatError(f"{FORMAT_PATH}: binary struct sizes disagree with the declarative contract")
    if DESCRIPTOR["offsets"]["wave_data"] != DESCRIPTOR["header_size"] + DESCRIPTOR["wave_header_size"]:
        raise CaseFormatError(f"{FORMAT_PATH}: wave-data offset disagrees with descriptor sizes")
    occupied: set[int] = set()
    sizes = {"u8": 1, "u16": 2, "u32": 4, "u64": 8}
    for field in DESCRIPTOR["fields"]:
        size = sizes.get(field["type"])
        if size is None or field["offset"] < 0 or field["offset"] + size > DESCRIPTOR["header_size"]:
            raise CaseFormatError(f"{FORMAT_PATH}: invalid descriptor field {field['name']}")
        span = set(range(field["offset"], field["offset"] + size))
        if occupied & span:
            raise CaseFormatError(f"{FORMAT_PATH}: overlapping descriptor field {field['name']}")
        occupied |= span


_validate_contract()


def parse_source(path: Path, mode: str) -> tuple[dict[str, int], bytes]:
    if mode not in DESCRIPTOR["magic"]:
        raise CaseFormatError(f"unknown fixture mode {mode!r}")
    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        raise CaseFormatError(f"cannot read {path}: {error}") from error
    if "\r" in text or not text.endswith("\n"):
        raise CaseFormatError(f"{path}: descriptor source must use LF and end with LF")
    lines = text.splitlines()
    if lines.count(BEGIN_MARKER) != 1 or lines.count(END_MARKER) != 1:
        raise CaseFormatError(f"{path}: descriptor must contain exactly one begin/end marker")
    begin = lines.index(BEGIN_MARKER)
    end = lines.index(END_MARKER)
    if begin != 0 or end <= begin or lines[end + 1:] != [f'.include "{mode}_fixture.inc"']:
        raise CaseFormatError(f"{path}: descriptor framing or fixture include is not canonical")
    descriptor_lines = lines[begin + 1:end]
    tail_marker = DESCRIPTOR["memory_tail"]["marker"]
    if not descriptor_lines or not descriptor_lines[-1].startswith(tail_marker):
        raise CaseFormatError(f"{path}: descriptor is missing its ROM-tail field")
    tail_token = descriptor_lines[-1][len(tail_marker):]
    if tail_token == "-":
        tail = b""
    elif re.fullmatch(r"(?:[0-9a-f]{2})+", tail_token) is not None:
        tail = bytes.fromhex(tail_token)
    else:
        raise CaseFormatError(f"{path}: malformed descriptor ROM tail")
    if len(tail) > DESCRIPTOR["memory_tail"]["maximum_bytes"]:
        raise CaseFormatError(f"{path}: descriptor ROM tail is too large")
    values: dict[str, int] = {}
    for line_number, line in enumerate(descriptor_lines[:-1], begin + 2):
        match = EQU_RE.fullmatch(line)
        if match is None:
            raise CaseFormatError(f"{path}:{line_number}: malformed descriptor field")
        symbol, token = match.groups()
        field = SOURCE_FIELDS_BY_SYMBOL.get(symbol)
        if field is None:
            raise CaseFormatError(f"{path}:{line_number}: unknown descriptor field {symbol}")
        if symbol in values:
            raise CaseFormatError(f"{path}:{line_number}: duplicate descriptor field {symbol}")
        value = int(token, 0)
        if value < field["minimum"] or value > field["maximum"]:
            raise CaseFormatError(f"{path}:{line_number}: {symbol} is outside its declared range")
        values[symbol] = value
    missing = set(SOURCE_FIELDS_BY_SYMBOL) - set(values)
    if missing:
        raise CaseFormatError(f"{path}: missing descriptor fields {', '.join(sorted(missing))}")
    return values, build_descriptor(mode, values, tail)


def _wave_data(mode: str, values: dict[str, int]) -> bytes:
    pattern = values["FIXTURE_PATTERN"]
    if pattern == 0:
        data = [127, 96, 64, 32, 0, -32, -64, -96] * 128
    elif pattern == 1:
        data = [127] * 1024
    elif pattern == 2:
        data = [-128] * 1024
    elif pattern == 3:
        data = [-120, -32, 48, 120, 96, 24, -48, -112] * 128
    elif pattern == 4:
        data = [0] + [0x11] * 32 + [64] + [0xFF] * 32
        if mode == "sappy" and values["FIXTURE_WAVE_LENGTH"] > 128:
            data += [-64] + [0] * 32
    elif mode == "ipatix" and pattern == 5:
        data = [0, values["FIXTURE_SYNTH_TYPE"], 0x40, 0x01, 0x80, 0xFF, 0, 0]
    elif mode == "sappy" and pattern == 5:
        data = [7, 15, 31, 63, -7, -15, -31, -63] * 128
    elif mode == "sappy" and pattern == 6:
        data = [127] + [0] * 1023
    else:
        raise CaseFormatError(f"{mode}: unsupported FIXTURE_PATTERN {pattern}")
    return bytes(value & 0xFF for value in data)


def build_descriptor(mode: str, values: dict[str, int], tail: bytes) -> bytes:
    offsets = DESCRIPTOR["offsets"]
    wave = _wave_data(mode, values)
    required_tail = 0
    if mode == "ipatix" and values["FIXTURE_WAVE_TYPE"] & 1 == 0:
        required_tail = max(0, values["FIXTURE_WAVE_LENGTH"] - len(wave))
    if len(tail) != required_tail:
        raise CaseFormatError(
            f"{mode}: descriptor ROM tail has {len(tail)} bytes; expected {required_tail}")
    embedded_size = offsets["wave_data"] + len(wave)
    result = bytearray(embedded_size + len(tail))
    magic = offsets["magic"]
    result[magic:magic + 4] = DESCRIPTOR["magic"][mode].encode("ascii")
    struct.pack_into("<H", result, offsets["version"], DESCRIPTOR["version"])
    struct.pack_into("<H", result, offsets["header_size"], DESCRIPTOR["header_size"])
    struct.pack_into("<I", result, offsets["frame_size"], DESCRIPTOR["frame_size"])
    struct.pack_into("<I", result, offsets["logical_ring_size"], DESCRIPTOR["logical_ring_size"])
    struct.pack_into("<I", result, offsets["wave_data_size"], len(wave))
    for field in DESCRIPTOR["fields"]:
        value = values[field["symbol"]]
        formats = {"u8": "<B", "u16": "<H", "u32": "<I", "u64": "<Q"}
        struct.pack_into(formats[field["type"]], result, field["offset"], value & ((1 << (8 * struct.calcsize(formats[field["type"]]))) - 1))
    wave_header = offsets["wave_header"]
    result[wave_header] = values["FIXTURE_WAVE_TYPE"] & 0xFF
    result[offsets["wave_header_status"]] = values["FIXTURE_WAVE_STATUS"] & 0xFF
    struct.pack_into("<III", result, offsets["wave_frequency"], 0,
                     values["FIXTURE_LOOP_START"], values["FIXTURE_WAVE_LENGTH"])
    result[offsets["wave_data"]:embedded_size] = wave
    result[embedded_size:] = tail
    return bytes(result)


def embedded_descriptor_size(descriptor: bytes) -> int:
    offsets = DESCRIPTOR["offsets"]
    minimum = offsets["wave_data"]
    if len(descriptor) < minimum:
        raise CaseFormatError("descriptor is truncated")
    return minimum + struct.unpack_from("<I", descriptor, offsets["wave_data_size"])[0]


def settings(values: dict[str, int]) -> dict[str, int]:
    return {
        "pcm_rate_hz": values["FIXTURE_PCM_RATE_HZ"],
        "max_channels": values["FIXTURE_VOICE_COUNT"],
        "master_volume": values["FIXTURE_MASTER_VOLUME"],
        "voice_volume": values["FIXTURE_VOICE_VOLUME"],
        "reverb": values["FIXTURE_REVERB"],
    }


def make_case_blob(mode: str, descriptor: bytes, begin: tuple[int, int], end: tuple[int, int],
                   rows: list[tuple[int, int, int, int, int]]) -> bytes:
    result = bytearray(CASE_HEADER.pack(CASE["magic"][mode].encode("ascii"), CASE["version"],
                                       CASE["header_size"], len(descriptor), len(rows),
                                       begin[0], end[0], begin[1], end[1]))
    result.extend(descriptor)
    for cycle, order, _, address, _ in rows:
        result.extend(SCHEDULE_ROW.pack(cycle, order, address))
    return bytes(result)


def c_header() -> str:
    def macro(name: str) -> str:
        return re.sub(r"[^A-Z0-9]+", "_", name.upper())

    lines = [
        "#ifndef PORYAAAA_PCM_MIXER_CASE_FORMAT_H",
        "#define PORYAAAA_PCM_MIXER_CASE_FORMAT_H",
        "",
        "/* Generated from tools/mgba-reference/pcm-mixer-oracle/case-format.json. */",
        f"#define PCM_CASE_HEADER_SIZE {CASE['header_size']}u",
        f"#define PCM_CASE_SCHEDULE_ROW_SIZE {CASE['schedule_row_size']}u",
        f"#define PCM_CASE_VERSION {CASE['version']}u",
        f"#define PCM_DESCRIPTOR_HEADER_SIZE {DESCRIPTOR['header_size']}u",
        f"#define PCM_DESCRIPTOR_WAVE_HEADER_SIZE {DESCRIPTOR['wave_header_size']}u",
        f"#define PCM_DESCRIPTOR_FRAME_SIZE {DESCRIPTOR['frame_size']}u",
        f"#define PCM_DESCRIPTOR_VERSION {DESCRIPTOR['version']}u",
        f"#define PCM_DESCRIPTOR_LOGICAL_RING_SIZE {DESCRIPTOR['logical_ring_size']}u",
    ]
    for name, offset in CASE["offsets"].items():
        lines.append(f"#define PCM_CASE_{macro(name)}_OFFSET {offset}u")
    for name, offset in CASE["schedule_offsets"].items():
        lines.append(f"#define PCM_CASE_SCHEDULE_{macro(name)}_OFFSET {offset}u")
    for field in DESCRIPTOR["fields"]:
        lines.append(f"#define PCM_DESCRIPTOR_{macro(field['name'])}_OFFSET {field['offset']}u")
    for name, offset in DESCRIPTOR["offsets"].items():
        lines.append(f"#define PCM_DESCRIPTOR_{macro(name)}_OFFSET {offset}u")
    lines.extend(["", "#endif", ""])
    return "\n".join(lines)


def verify_c_header() -> None:
    expected = c_header()
    try:
        actual = C_HEADER_PATH.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        raise CaseFormatError(f"cannot read generated C contract {C_HEADER_PATH}: {error}") from error
    if actual != expected:
        raise CaseFormatError(f"{C_HEADER_PATH} is stale; regenerate it from {FORMAT_PATH}")
