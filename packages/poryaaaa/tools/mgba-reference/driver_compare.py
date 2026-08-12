#!/usr/bin/env python3
"""Compare family-relevant version-1 audio trace events ordinally.

The required transaction, payload, and logical-state gates compare only events
inside the measured BEGIN/END interval.  Original event cycles and same-cycle
orders remain diagnostics: this harness verifies driver emissions, not ARM
instruction-cycle parity.
"""

import argparse
import hashlib
import json
import sys
from pathlib import Path


CLOCK_HZ = 16_777_216
HEADER_LINE = "PORYAAAA_AUDIO_TRACE 1"
UINT32_MAX = (1 << 32) - 1
UINT64_MAX = (1 << 64) - 1
MAX_LINE_CONTENT = 510  # 512-byte C line buffer minus LF and NUL
IO_BASE = 0x04000000
WAVE_BASE = IO_BASE + 0x90
FAMILY_NAMES = ("psw", "sq1", "sq2", "directsound")

FAMILY_RANGES = {
    "psw": (
        (IO_BASE + 0x70, IO_BASE + 0x75),
        (IO_BASE + 0x80, IO_BASE + 0x81),
        (IO_BASE + 0x90, IO_BASE + 0x9F),
    ),
    "sq1": (
        (IO_BASE + 0x60, IO_BASE + 0x65),
        (IO_BASE + 0x80, IO_BASE + 0x81),
    ),
    "sq2": (
        (IO_BASE + 0x68, IO_BASE + 0x6D),
        (IO_BASE + 0x80, IO_BASE + 0x81),
    ),
    "directsound": (
        (IO_BASE + 0x80, IO_BASE + 0x84),
        (IO_BASE + 0x88, IO_BASE + 0x89),
        (IO_BASE + 0xA0, IO_BASE + 0xA7),
    ),
}


class TraceError(Exception):
    """Malformed trace input with a stable fail-closed CLI diagnostic."""


class TraceRecord:
    """One parsed trace record, retaining its source location and ordering."""

    __slots__ = ("line", "cycle", "order", "kind", "width", "address", "value")

    def __init__(self, line, cycle, order, kind, width=0, address=0, value=0):
        self.line = line
        self.cycle = cycle
        self.order = order
        self.kind = kind
        self.width = width
        self.address = address
        self.value = value


def parse_unsigned_decimal(text, limit):
    """Parse one unsigned decimal token without accepting signs or spaces."""
    if not text or any(character < "0" or character > "9" for character in text):
        return None
    value = int(text)
    return value if value <= limit else None


def parse_unsigned_hex(text, limit):
    """Parse one canonical lower-prefix hexadecimal token."""
    if len(text) < 3 or text[:2] != "0x":
        return None
    digits = text[2:]
    if not digits or any(character not in "0123456789abcdefABCDEF" for character in digits):
        return None
    value = int(digits, 16)
    return value if value <= limit else None


def event_error(kind):
    """Keep marker parse failures distinguishable from ordinary event failures."""
    return "invalid BEGIN or END marker" if kind in ("BEGIN", "END") else "invalid trace event"


def trace_fail(path, line_number, message):
    """Raise one consistent parser diagnostic."""
    if line_number is None:
        raise TraceError(f"{path}: {message}")
    raise TraceError(f"{path}: line {line_number}: {message}")


def parse_trace(path):
    """Strictly parse the shared version-1 text grammar without normalization."""
    try:
        data = path.read_bytes()
    except OSError as error:
        raise TraceError(f"{path}: {error}") from error
    header = HEADER_LINE.encode("ascii") + b"\n"
    if not data.startswith(header):
        trace_fail(path, 1, f"trace must begin with {HEADER_LINE}")
    body = data[len(header):]
    if body == b"":
        lines = []
    elif not body.endswith(b"\n"):
        trace_fail(path, 2 + body.count(b"\n"), "trace line exceeds the maximum length")
    else:
        lines = body.split(b"\n")[:-1]

    records = []
    clock_seen = False
    measurement_open = False
    measurement_closed = False
    last_position = None
    line_number = 1
    for line_bytes in lines:
        line_number += 1
        if len(line_bytes) > MAX_LINE_CONTENT:
            trace_fail(path, line_number, "trace line exceeds the maximum length")
        try:
            line = line_bytes.decode("utf-8")
        except UnicodeDecodeError:
            trace_fail(path, line_number, "invalid trace event")
        if not line or line[0] == "#":
            continue
        tokens = line.split(" ")
        if len(tokens) > 6 or any(token == "" for token in tokens) or "\t" in line or "\r" in line:
            trace_fail(path, line_number, "invalid trace event")
        if tokens[0] == "CLOCK":
            if (
                len(tokens) != 2
                or parse_unsigned_decimal(tokens[1], UINT64_MAX) != CLOCK_HZ
                or clock_seen
                or last_position is not None
            ):
                trace_fail(path, line_number, "invalid CLOCK declaration")
            clock_seen = True
            continue
        if not clock_seen:
            trace_fail(path, line_number, "trace event precedes CLOCK declaration")
        cycle = parse_unsigned_decimal(tokens[1], UINT64_MAX) if len(tokens) >= 3 else None
        order = parse_unsigned_decimal(tokens[2], UINT32_MAX) if len(tokens) >= 3 else None
        if cycle is None or order is None:
            trace_fail(path, line_number, event_error(tokens[0]))
        position = (cycle, order)
        if last_position is not None and position <= last_position:
            trace_fail(path, line_number, event_error(tokens[0]))

        if tokens[0] == "BEGIN":
            if len(tokens) != 3 or measurement_open or measurement_closed:
                trace_fail(path, line_number, "invalid BEGIN or END marker")
            measurement_open = True
            records.append(TraceRecord(line_number, cycle, order, "BEGIN"))
        elif tokens[0] == "END":
            if len(tokens) != 3 or not measurement_open or measurement_closed:
                trace_fail(path, line_number, "invalid BEGIN or END marker")
            measurement_open = False
            measurement_closed = True
            records.append(TraceRecord(line_number, cycle, order, "END"))
        elif measurement_closed:
            trace_fail(path, line_number, "trace event follows the END marker")
        elif tokens[0] == "WRITE" and len(tokens) == 6:
            width = parse_unsigned_decimal(tokens[3], 0xFF)
            address = parse_unsigned_hex(tokens[4], UINT32_MAX)
            value = parse_unsigned_hex(tokens[5], UINT32_MAX)
            if (
                width is None
                or address is None
                or value is None
                or width == 0
                or address + width - 1 > UINT32_MAX
            ):
                trace_fail(path, line_number, "invalid trace event")
            records.append(TraceRecord(line_number, cycle, order, "WRITE", width, address, value))
        elif tokens[0] == "SAMPLE" and len(tokens) == 3:
            records.append(TraceRecord(line_number, cycle, order, "SAMPLE"))
        elif tokens[0] == "TIMER" and len(tokens) == 4:
            timer = parse_unsigned_decimal(tokens[3], 1)
            if timer is None:
                trace_fail(path, line_number, "invalid trace event")
            records.append(TraceRecord(line_number, cycle, order, "TIMER", value=timer))
        else:
            trace_fail(path, line_number, "invalid trace event")
        last_position = position
    if not clock_seen or measurement_open or not measurement_closed:
        trace_fail(path, None, "trace ends without a closed BEGIN/END interval")
    return records


def require_family(family):
    """Reject unknown projections instead of silently selecting a nearby family."""
    if family not in FAMILY_NAMES:
        raise TraceError(f"unsupported family {family!r}")
    return family


def touches_ranges(record, ranges):
    """Return whether one little-endian write overlaps a retained byte range."""
    if record.kind != "WRITE":
        return False
    write_end = record.address + record.width - 1
    return any(record.address <= high and low <= write_end for low, high in ranges)


def is_retained(record, family="psw"):
    """Return whether a record participates in this family's transaction gate."""
    family = require_family(family)
    if family == "directsound" and record.kind == "TIMER":
        return True
    return touches_ranges(record, FAMILY_RANGES[family])


def directsound_fifo_channel(record):
    """Return the FIFO named by one canonical 32-bit DMA word, if any."""
    if record.kind != "WRITE" or record.width != 4:
        return None
    if record.address == IO_BASE + 0xA0:
        return "a"
    if record.address == IO_BASE + 0xA4:
        return "b"
    return None


def canonicalize_directsound_refills(projected, interval_samples):
    """Canonicalize complete paired DMA refills without changing FIFO payloads.

    mGBA can observe the timer callback that requests a refill before or during
    the asynchronous words, while the candidate emits the completed refill
    before the callback.  Associate a leading TIMER only when it is immediately
    followed by FIFO words.  Otherwise associate the closest TIMER immediately
    before the refill, unless the eight words have an interleaved or trailing
    TIMER of their own.  Payload order inside each FIFO remains strict, and
    setup/control writes, malformed words, and incomplete refills are barriers.
    """
    canonical = []
    canonical_intervals = []
    index = 0
    while index < len(projected):
        if directsound_fifo_channel(projected[index]) is None:
            canonical.append(projected[index])
            canonical_intervals.append(interval_samples[index])
            index += 1
            continue

        fifo_a = []
        fifo_b = []
        timer = None
        cursor = index
        complete = False
        while cursor < len(projected):
            record = projected[cursor]
            channel = directsound_fifo_channel(record)
            if channel == "a":
                if len(fifo_a) == 4:
                    break
                fifo_a.append((record, interval_samples[cursor]))
            elif channel == "b":
                if len(fifo_b) == 4:
                    break
                fifo_b.append((record, interval_samples[cursor]))
            elif record.kind == "TIMER":
                if timer is not None:
                    break
                timer = (record, interval_samples[cursor])
            else:
                break
            cursor += 1
            if len(fifo_a) + len(fifo_b) == 8:
                if timer is None and cursor < len(projected) and projected[cursor].kind == "TIMER" and (
                    projected[cursor].cycle <= fifo_b[-1][0].cycle
                    or not canonical
                    or canonical[-1].kind != "TIMER"
                ):
                    timer = (projected[cursor], interval_samples[cursor])
                    cursor += 1
                complete = True
                break

        if not complete:
            canonical.append(projected[index])
            canonical_intervals.append(interval_samples[index])
            index += 1
            continue

        if timer is None and canonical and canonical[-1].kind == "TIMER":
            timer = (canonical.pop(), canonical_intervals.pop())
        for record, samples in fifo_a + fifo_b + ([] if timer is None else [timer]):
            canonical.append(record)
            canonical_intervals.append(samples)
        index = cursor
    return canonical, canonical_intervals


def project_trace(records, family="psw"):
    """Select measured family transactions and collect SAMPLE diagnostics.

    Events before BEGIN establish replay state and records after END are rejected
    by the grammar, so only the open interval can contribute to parity gates.
    SAMPLE records annotate transaction intervals but never become transactions.
    """
    family = require_family(family)
    begin = next(record for record in records if record.kind == "BEGIN")
    end = next(record for record in records if record.kind == "END")
    begin_position = (begin.cycle, begin.order)
    end_position = (end.cycle, end.order)
    projected = []
    interval_samples = []
    pending_samples = []
    for record in records:
        position = (record.cycle, record.order)
        if not begin_position < position < end_position:
            continue
        if record.kind == "SAMPLE":
            pending_samples.append(record)
        elif is_retained(record, family):
            projected.append(record)
            interval_samples.append(pending_samples)
            pending_samples = []
    if family == "directsound":
        return canonicalize_directsound_refills(projected, interval_samples)
    return projected, interval_samples



def byte_writes(record):
    """Yield the observed little-endian bytes of one WRITE record."""
    for index in range(record.width):
        yield record.address + index, (record.value >> (8 * index)) & 0xFF


def register_name(address, family="psw"):
    """Name a retained hardware byte without inventing a cross-family register."""
    family = require_family(family)
    common = {
        IO_BASE + 0x80: "NR50/NR51",
        IO_BASE + 0x81: "NR50/NR51",
    }
    if family == "psw":
        if address in (IO_BASE + 0x70, IO_BASE + 0x71):
            return "NR30"
        if IO_BASE + 0x72 <= address <= IO_BASE + 0x75:
            return ("NR31", "NR32", "NR33", "NR34")[address - (IO_BASE + 0x72)]
        if WAVE_BASE <= address <= WAVE_BASE + 0x0F:
            return "WAVE_RAM"
    elif family == "sq1":
        if address in (IO_BASE + 0x60, IO_BASE + 0x61):
            return "NR10"
        if IO_BASE + 0x62 <= address <= IO_BASE + 0x65:
            return ("NR11", "NR12", "NR13", "NR14")[address - (IO_BASE + 0x62)]
    elif family == "sq2":
        if address in (IO_BASE + 0x68, IO_BASE + 0x69, IO_BASE + 0x6A, IO_BASE + 0x6B):
            return ("NR21", "NR22", "NR21_UNUSED", "NR21_UNUSED")[address - (IO_BASE + 0x68)]
        if address in (IO_BASE + 0x6C, IO_BASE + 0x6D):
            return ("NR23", "NR24")[address - (IO_BASE + 0x6C)]
    else:
        if address in (IO_BASE + 0x80, IO_BASE + 0x81):
            return "SOUNDCNT_L"
        if address in (IO_BASE + 0x82, IO_BASE + 0x83):
            return "SOUNDCNT_H"
        if address == IO_BASE + 0x84:
            return "NR52"
        if address in (IO_BASE + 0x88, IO_BASE + 0x89):
            return "SOUNDBIAS"
        if IO_BASE + 0xA0 <= address <= IO_BASE + 0xA3:
            return "FIFO_A"
        if IO_BASE + 0xA4 <= address <= IO_BASE + 0xA7:
            return "FIFO_B"
    return common.get(address, "0x%08X" % address)


def record_dict(record, family="psw"):
    """Return a deterministic source-level record diagnostic."""
    result = {
        "line": record.line,
        "cycle": record.cycle,
        "order": record.order,
        "kind": record.kind,
    }
    if record.kind == "WRITE":
        result.update(
            {
                "register": register_name(record.address, family),
                "width": record.width,
                "address": "0x%08X" % record.address,
                "value": "0x%08X" % record.value,
            }
        )
    elif record.kind == "TIMER":
        result["timer"] = record.value
    return result


def transaction_key(record):
    """Compare transaction shape without treating source cycles as a gate."""
    if record.kind == "WRITE":
        return record.kind, record.address, record.width, record.value
    if record.kind == "TIMER":
        return record.kind, record.value
    return record.kind,


def compare_transactions(reference, candidate, family="psw"):
    """Fail at the first missing, extra, reordered, wide, address, or value event."""
    for ordinal in range(min(len(reference), len(candidate))):
        if transaction_key(reference[ordinal]) != transaction_key(candidate[ordinal]):
            return False, {
                "ordinal": ordinal,
                "reference": record_dict(reference[ordinal], family),
                "candidate": record_dict(candidate[ordinal], family),
            }
    if len(reference) != len(candidate):
        ordinal = min(len(reference), len(candidate))
        return False, {
            "ordinal": ordinal,
            "reference": record_dict(reference[ordinal], family) if ordinal < len(reference) else None,
            "candidate": record_dict(candidate[ordinal], family) if ordinal < len(candidate) else None,
        }
    return True, None


class PswState:
    """Replay PSW writes, including banked wave RAM, for ordinal state checks."""

    __slots__ = ("nr30", "nr31", "nr32", "nr33", "nr34", "nr50", "nr51", "wave_ram")

    def __init__(self):
        self.nr30 = 0
        self.nr31 = 0
        self.nr32 = 0
        self.nr33 = 0
        self.nr34 = 0
        self.nr50 = 0
        self.nr51 = 0
        self.wave_ram = bytearray(32)

    def apply(self, record):
        """Apply each written bus byte in its observed little-endian order."""
        if record.kind != "WRITE":
            return
        for address, value in byte_writes(record):
            if address == IO_BASE + 0x70:
                self.nr30 = value
            elif IO_BASE + 0x72 <= address <= IO_BASE + 0x75:
                setattr(self, ("nr31", "nr32", "nr33", "nr34")[address - (IO_BASE + 0x72)], value)
            elif address == IO_BASE + 0x80:
                self.nr50 = value
            elif address == IO_BASE + 0x81:
                self.nr51 = value
            elif WAVE_BASE <= address <= WAVE_BASE + 0x0F:
                bank = 0 if self.nr30 & 0x40 else 1
                self.wave_ram[bank * 16 + address - WAVE_BASE] = value

    def snapshot(self):
        """Return hardware-visible PSW state after one retained ordinal."""
        return {
            "nr30": self.nr30,
            "nr30_dac_on": bool(self.nr30 & 0x80),
            "nr30_bank": bool(self.nr30 & 0x40),
            "nr30_wave_size": bool(self.nr30 & 0x20),
            "nr31": self.nr31,
            "nr31_length": 256 - self.nr31,
            "nr32": self.nr32,
            "nr32_volume_code": (self.nr32 >> 5) & 7,
            "nr33": self.nr33,
            "nr34": self.nr34,
            "nr34_length_enable": bool(self.nr34 & 0x40),
            "nr34_trigger": bool(self.nr34 & 0x80),
            "freq11": ((self.nr34 & 7) << 8) | self.nr33,
            "nr50": self.nr50,
            "nr51": self.nr51,
            "wave_bank_0": self.wave_ram[:16].hex(),
            "wave_bank_1": self.wave_ram[16:].hex(),
        }

    def payload(self):
        """Expose the selected 16-byte payload independently of transaction shape."""
        return [("wave", index, value) for index, value in enumerate(self.wave_ram[:16])]


class SquareState:
    """Replay one square channel without granting Sq2 a fabricated sweep field."""

    def __init__(self, family):
        self.family = family
        self.bytes = {address: 0 for low, high in FAMILY_RANGES[family] for address in range(low, high + 1)}

    def apply(self, record):
        """Store only bytes in this channel's real retained register window."""
        if record.kind != "WRITE":
            return
        for address, value in byte_writes(record):
            if address in self.bytes:
                self.bytes[address] = value

    def snapshot(self):
        """Expose duty, envelope, frequency, trigger, routing, and Sq1 sweep separately."""
        if self.family == "sq1":
            nr10 = self.bytes[IO_BASE + 0x60]
            nr11 = self.bytes[IO_BASE + 0x62]
            nr12 = self.bytes[IO_BASE + 0x63]
            nr13 = self.bytes[IO_BASE + 0x64]
            nr14 = self.bytes[IO_BASE + 0x65]
            result = {
                "nr10": nr10,
                "nr10_sweep_time": (nr10 >> 4) & 7,
                "nr10_sweep_decrease": bool(nr10 & 8),
                "nr10_sweep_shift": nr10 & 7,
                "nr11": nr11,
                "duty": (nr11 >> 6) & 3,
                "length": 64 - (nr11 & 0x3F),
                "nr12": nr12,
            }
        else:
            nr11 = self.bytes[IO_BASE + 0x68]
            nr12 = self.bytes[IO_BASE + 0x69]
            nr13 = self.bytes[IO_BASE + 0x6C]
            nr14 = self.bytes[IO_BASE + 0x6D]
            result = {
                "nr21": nr11,
                "duty": (nr11 >> 6) & 3,
                "length": 64 - (nr11 & 0x3F),
                "nr22": nr12,
            }
        result.update(
            {
                "envelope_initial_volume": (nr12 >> 4) & 0xF,
                "envelope_increase": bool(nr12 & 8),
                "envelope_period": nr12 & 7,
                "frequency_low": nr13,
                "frequency_high": nr14 & 7,
                "frequency": ((nr14 & 7) << 8) | nr13,
                "length_enable": bool(nr14 & 0x40),
                "trigger": bool(nr14 & 0x80),
                "nr50": self.bytes[IO_BASE + 0x80],
                "nr51": self.bytes[IO_BASE + 0x81],
            }
        )
        return result

    def payload(self):
        """Extract the square fixture configuration rather than a shared bus sequence."""
        if self.family == "sq1":
            addresses = (IO_BASE + 0x60, IO_BASE + 0x62, IO_BASE + 0x63, IO_BASE + 0x64, IO_BASE + 0x65,
                         IO_BASE + 0x80, IO_BASE + 0x81)
            names = ("nr10", "nr11", "nr12", "nr13", "nr14", "nr50", "nr51")
        else:
            addresses = (IO_BASE + 0x68, IO_BASE + 0x69, IO_BASE + 0x6C, IO_BASE + 0x6D,
                         IO_BASE + 0x80, IO_BASE + 0x81)
            names = ("nr21", "nr22", "nr23", "nr24", "nr50", "nr51")
        return [(name, 0, self.bytes[address]) for name, address in zip(names, addresses)]


class DirectSoundFifo:
    """Mirror the trace-replay modulo-eight FIFO and byte shift-register state."""

    __slots__ = (
        "words",
        "read_index",
        "write_index",
        "internal_sample",
        "internal_remaining",
        "held_sample",
        "underflows",
        "write_wraps",
    )

    def __init__(self):
        self.words = [0] * 8
        self.read_index = 0
        self.write_index = 0
        self.internal_sample = 0
        self.internal_remaining = 0
        self.held_sample = 0
        self.underflows = 0
        self.write_wraps = 0

    def write_word(self, value):
        """Advance the hardware's modulo-eight write pointer without an invented full flag."""
        self.words[self.write_index] = value & UINT32_MAX
        self.write_index = (self.write_index + 1) % 8
        if self.write_index == 0:
            self.write_wraps += 1

    def reset(self):
        """Reset only FIFO pointers, matching SOUNDCNT_H reset-bit behavior."""
        self.read_index = 0
        self.write_index = 0

    def clock(self):
        """Clock one selected timer overflow through the real little-endian shifter."""
        if not self.internal_remaining and self.read_index != self.write_index:
            self.internal_sample = self.words[self.read_index]
            self.read_index = (self.read_index + 1) % 8
            self.internal_remaining = 4
        elif not self.internal_remaining:
            self.underflows += 1
        signed = self.internal_sample & 0xFF
        self.held_sample = signed - 256 if signed >= 128 else signed
        if self.internal_remaining:
            self.internal_sample >>= 8
            self.internal_remaining -= 1

    def snapshot(self, name):
        """Provide queue-head, byte-shifter, and underflow context for diagnostics."""
        return {
            f"{name}_depth": (self.write_index - self.read_index) % 8,
            f"{name}_read_index": self.read_index,
            f"{name}_write_index": self.write_index,
            f"{name}_head": "0x%08X" % self.words[self.read_index],
            f"{name}_words": "".join("%08X" % word for word in self.words),
            f"{name}_internal_word": "0x%08X" % self.internal_sample,
            f"{name}_internal_remaining": self.internal_remaining,
            f"{name}_held_sample": self.held_sample,
            f"{name}_underflows": self.underflows,
            f"{name}_write_wraps": self.write_wraps,
        }


class DirectSoundState:
    """Replay FIFO writes, timer overflows, and DirectSound routing without retiming."""

    def __init__(self):
        self.bytes = {address: 0 for address in (*range(IO_BASE + 0x80, IO_BASE + 0x85), *range(IO_BASE + 0x88, IO_BASE + 0x8A))}
        self.fifo_a = DirectSoundFifo()
        self.fifo_b = DirectSoundFifo()

    def soundcnt_h(self):
        """Decode the little-endian SOUNDCNT_H image used by FIFO selection and reset."""
        return self.bytes[IO_BASE + 0x82] | (self.bytes[IO_BASE + 0x83] << 8)

    def apply(self, record):
        """Apply one observed event; SAMPLE observes state and does not mutate it."""
        if record.kind == "TIMER":
            soundcnt_h = self.soundcnt_h()
            master_enabled = bool(self.bytes[IO_BASE + 0x84] & 0x80)
            if master_enabled:
                if soundcnt_h & 0x0300 and ((soundcnt_h >> 10) & 1) == record.value:
                    self.fifo_a.clock()
                if soundcnt_h & 0x3000 and ((soundcnt_h >> 14) & 1) == record.value:
                    self.fifo_b.clock()
            return
        if record.kind != "WRITE":
            return
        for address, value in byte_writes(record):
            if address in self.bytes:
                self.bytes[address] = value
        soundcnt_h = self.soundcnt_h()
        if any(address in (IO_BASE + 0x82, IO_BASE + 0x83) for address, _ in byte_writes(record)):
            if soundcnt_h & 0x0800:
                self.fifo_a.reset()
            if soundcnt_h & 0x8000:
                self.fifo_b.reset()
        if record.width == 4 and record.address == IO_BASE + 0xA0:
            self.fifo_a.write_word(record.value)
        elif record.width == 4 and record.address == IO_BASE + 0xA4:
            self.fifo_b.write_word(record.value)

    def snapshot(self):
        """Return DirectSound configuration plus both concrete FIFO machine states."""
        soundcnt_h = self.soundcnt_h()
        result = {
            "soundcnt_l": "0x%04X" % (self.bytes[IO_BASE + 0x80] | (self.bytes[IO_BASE + 0x81] << 8)),
            "soundcnt_h": "0x%04X" % soundcnt_h,
            "soundbias": "0x%04X" % (self.bytes[IO_BASE + 0x88] | (self.bytes[IO_BASE + 0x89] << 8)),
            "master_enabled": bool(self.bytes[IO_BASE + 0x84] & 0x80),
            "fifo_a_timer": (soundcnt_h >> 10) & 1,
            "fifo_b_timer": (soundcnt_h >> 14) & 1,
            "fifo_a_routed": bool(soundcnt_h & 0x0300),
            "fifo_b_routed": bool(soundcnt_h & 0x3000),
        }
        result.update(self.fifo_a.snapshot("fifo_a"))
        result.update(self.fifo_b.snapshot("fifo_b"))
        return result

    def payload(self):
        """Extract FIFO byte streams and final setup independently of event widths/order."""
        raise AssertionError("DirectSound payload is extracted from its projection")


def state_for(family):
    """Choose only the concrete state machine owned by the resolved family."""
    if family == "psw":
        return PswState()
    if family in ("sq1", "sq2"):
        return SquareState(family)
    return DirectSoundState()


def directsound_payload(records):
    """Expand FIFO words little-endian while retaining per-FIFO payload identity."""
    fifo_a = []
    fifo_b = []
    configuration = {address: 0 for address in (*range(IO_BASE + 0x80, IO_BASE + 0x85), *range(IO_BASE + 0x88, IO_BASE + 0x8A))}
    for record in records:
        if record.kind != "WRITE":
            continue
        for address, value in byte_writes(record):
            if IO_BASE + 0xA0 <= address <= IO_BASE + 0xA3:
                fifo_a.append(value)
            elif IO_BASE + 0xA4 <= address <= IO_BASE + 0xA7:
                fifo_b.append(value)
            elif address in configuration:
                configuration[address] = value
    entries = [("fifo_a", index, value) for index, value in enumerate(fifo_a)]
    entries.extend(("fifo_b", index, value) for index, value in enumerate(fifo_b))
    config_names = (
        (IO_BASE + 0x80, "soundcnt_l_low"), (IO_BASE + 0x81, "soundcnt_l_high"),
        (IO_BASE + 0x82, "soundcnt_h_low"), (IO_BASE + 0x83, "soundcnt_h_high"),
        (IO_BASE + 0x84, "nr52"), (IO_BASE + 0x88, "soundbias_low"), (IO_BASE + 0x89, "soundbias_high"),
    )
    entries.extend((name, 0, configuration[address]) for address, name in config_names)
    view = {
        "fifo_a_bytes": fifo_a,
        "fifo_b_bytes": fifo_b,
        "configuration": {name: configuration[address] for address, name in config_names},
    }
    return entries, view


def profile_payload(records, family):
    """Build family payload evidence from a fresh state, independent of transaction shape."""
    if family == "directsound":
        return directsound_payload(records)
    state = state_for(family)
    for record in records:
        state.apply(record)
    entries = state.payload()
    if family == "psw":
        view = {"wave_bytes": [value for _, _, value in entries]}
    else:
        view = {section: value for section, _, value in entries}
    return entries, view


def payload_bytes(family, entries):
    """Hash a canonical family-tagged payload representation without host paths."""
    text = json.dumps(
        {"family": family, "entries": entries}, separators=(",", ":"), ensure_ascii=True, allow_nan=False
    )
    return text.encode("ascii")


def entry_dict(entry):
    """Describe one extracted payload byte or configuration field."""
    if entry is None:
        return None
    section, index, value = entry
    return {"section": section, "index": index, "value": value}


def payload_diagnostics(reference_entries, candidate_entries, family, reference_view, candidate_view):
    """Locate the first payload byte/configuration difference deterministically."""
    ordinal = None
    reference_entry = None
    candidate_entry = None
    for index in range(min(len(reference_entries), len(candidate_entries))):
        if reference_entries[index] != candidate_entries[index]:
            ordinal = index
            reference_entry = reference_entries[index]
            candidate_entry = candidate_entries[index]
            break
    if ordinal is None and len(reference_entries) != len(candidate_entries):
        ordinal = min(len(reference_entries), len(candidate_entries))
        reference_entry = reference_entries[ordinal] if ordinal < len(reference_entries) else None
        candidate_entry = candidate_entries[ordinal] if ordinal < len(candidate_entries) else None
    first_nibble = None
    reference_nibble = None
    candidate_nibble = None
    if family == "psw" and reference_entry and candidate_entry and reference_entry[0] == candidate_entry[0] == "wave":
        reference_byte = reference_entry[2]
        candidate_byte = candidate_entry[2]
        if (reference_byte >> 4) != (candidate_byte >> 4):
            first_nibble = reference_entry[1] * 2
            reference_nibble = reference_byte >> 4
            candidate_nibble = candidate_byte >> 4
        else:
            first_nibble = reference_entry[1] * 2 + 1
            reference_nibble = reference_byte & 0xF
            candidate_nibble = candidate_byte & 0xF
    return {
        "first_differing_byte": ordinal,
        "first_differing_nibble": first_nibble,
        "reference_entry": entry_dict(reference_entry),
        "candidate_entry": entry_dict(candidate_entry),
        "reference_nibble": reference_nibble,
        "candidate_nibble": candidate_nibble,
        "reference": reference_view,
        "candidate": candidate_view,
    }


def compare_states(reference, candidate, family="psw"):
    """Replay measured events and identify the first logical state difference."""
    reference_state = state_for(family)
    candidate_state = state_for(family)
    for ordinal in range(min(len(reference), len(candidate))):
        reference_state.apply(reference[ordinal])
        candidate_state.apply(candidate[ordinal])
        reference_snapshot = reference_state.snapshot()
        candidate_snapshot = candidate_state.snapshot()
        if reference_snapshot != candidate_snapshot:
            field = next(key for key in reference_snapshot if reference_snapshot[key] != candidate_snapshot[key])
            result = {
                "ordinal": ordinal,
                "field": field,
                "reference": reference_snapshot[field],
                "candidate": candidate_snapshot[field],
                "causal_record": {
                    "reference": record_dict(reference[ordinal], family),
                    "candidate": record_dict(candidate[ordinal], family),
                },
            }
            if family == "directsound":
                result["fifo_context"] = {
                    "reference": reference_snapshot,
                    "candidate": candidate_snapshot,
                }
            return False, result
    if len(reference) != len(candidate):
        ordinal = min(len(reference), len(candidate))
        return False, {
            "ordinal": ordinal,
            "field": "record_count",
            "reference": len(reference),
            "candidate": len(candidate),
            "causal_record": {
                "reference": record_dict(reference[ordinal], family) if ordinal < len(reference) else None,
                "candidate": record_dict(candidate[ordinal], family) if ordinal < len(candidate) else None,
            },
        }
    return True, None


def timing_report(reference, candidate, reference_intervals, candidate_intervals, family="psw"):
    """Report cycles, same-cycle order, and one-sided SAMPLE crossings without gating."""
    count = min(len(reference), len(candidate))
    first_cycle_mismatch = None
    cycle_deltas = []
    first_same_cycle_order_mismatch = None
    same_cycle_order = []
    sample_crossings = []
    for ordinal in range(count):
        reference_record = reference[ordinal]
        candidate_record = candidate[ordinal]
        if reference_record.cycle != candidate_record.cycle:
            if first_cycle_mismatch is None:
                first_cycle_mismatch = {
                    "ordinal": ordinal,
                    "reference_cycle": reference_record.cycle,
                    "candidate_cycle": candidate_record.cycle,
                }
            cycle_deltas.append(
                {
                    "ordinal": ordinal,
                    "reference_cycle": reference_record.cycle,
                    "candidate_cycle": candidate_record.cycle,
                    "delta": candidate_record.cycle - reference_record.cycle,
                }
            )
        if ordinal > 0:
            reference_previous = reference[ordinal - 1]
            candidate_previous = candidate[ordinal - 1]
            reference_same_cycle = reference_previous.cycle == reference_record.cycle
            candidate_same_cycle = candidate_previous.cycle == candidate_record.cycle
            reference_order_delta = reference_record.order - reference_previous.order if reference_same_cycle else None
            candidate_order_delta = candidate_record.order - candidate_previous.order if candidate_same_cycle else None
            if reference_same_cycle != candidate_same_cycle or (
                reference_same_cycle and reference_order_delta != candidate_order_delta
            ):
                mismatch = {
                    "ordinal": ordinal,
                    "reference_same_cycle": reference_same_cycle,
                    "candidate_same_cycle": candidate_same_cycle,
                    "reference_cycles": [reference_previous.cycle, reference_record.cycle],
                    "candidate_cycles": [candidate_previous.cycle, candidate_record.cycle],
                    "reference_orders": [reference_previous.order, reference_record.order],
                    "candidate_orders": [candidate_previous.order, candidate_record.order],
                    "reference_order_delta": reference_order_delta,
                    "candidate_order_delta": candidate_order_delta,
                }
                same_cycle_order.append(mismatch)
                if first_same_cycle_order_mismatch is None:
                    first_same_cycle_order_mismatch = mismatch
        if reference_record.kind == candidate_record.kind == "WRITE":
            reference_crosses = bool(reference_intervals[ordinal])
            candidate_crosses = bool(candidate_intervals[ordinal])
            if reference_crosses != candidate_crosses:
                sample_crossings.append(
                    {
                        "severity": "high",
                        "ordinal": ordinal,
                        "reference_crosses_sample": reference_crosses,
                        "candidate_crosses_sample": candidate_crosses,
                        "reference_sample_cycle": reference_intervals[ordinal][0].cycle if reference_crosses else None,
                        "candidate_sample_cycle": candidate_intervals[ordinal][0].cycle if candidate_crosses else None,
                        "reference": record_dict(reference_record, family),
                        "candidate": record_dict(candidate_record, family),
                    }
                )
    return {
        "cycle_exact": len(reference) == len(candidate) and first_cycle_mismatch is None,
        "first_cycle_mismatch": first_cycle_mismatch,
        "cycle_deltas": cycle_deltas,
        "same_cycle_order_mismatches": same_cycle_order,
        "first_same_cycle_order_mismatch": first_same_cycle_order_mismatch,
        "sample_boundary_crossings": sample_crossings,
    }


def sha256_file(path):
    """Hash one input trace without copying the capture into memory."""
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def compare(reference_path, candidate_path, family="psw"):
    """Return one deterministic family comparison result or raise TraceError."""
    family = require_family(family)
    reference_records = parse_trace(reference_path)
    candidate_records = parse_trace(candidate_path)
    reference, reference_intervals = project_trace(reference_records, family)
    candidate, candidate_intervals = project_trace(candidate_records, family)
    transaction_exact, transaction_divergence = compare_transactions(reference, candidate, family)
    reference_entries, reference_payload = profile_payload(reference, family)
    candidate_entries, candidate_payload = profile_payload(candidate, family)
    payload_exact = reference_entries == candidate_entries
    payload = payload_diagnostics(reference_entries, candidate_entries, family, reference_payload, candidate_payload)
    state_exact, state_divergence = compare_states(reference, candidate, family)
    timing = timing_report(reference, candidate, reference_intervals, candidate_intervals, family)
    if transaction_divergence is not None:
        first_divergence = dict(transaction_divergence)
        first_divergence["kind"] = "transaction"
    elif not payload_exact:
        first_divergence = {
            "kind": "payload",
            "byte_index": payload["first_differing_byte"],
            "reference": payload["reference_entry"],
            "candidate": payload["candidate_entry"],
        }
    elif state_divergence is not None:
        first_divergence = dict(state_divergence)
        first_divergence["kind"] = "logical_state"
    else:
        first_divergence = None
    reference_payload_bytes = payload_bytes(family, reference_entries)
    candidate_payload_bytes = payload_bytes(family, candidate_entries)
    return {
        "family": family,
        "transaction_exact": transaction_exact,
        "payload_exact": payload_exact,
        "logical_state_exact": state_exact,
        "cycle_exact": timing["cycle_exact"],
        "reference_event_count": len(reference),
        "candidate_event_count": len(candidate),
        "first_divergence": first_divergence,
        "payload": payload,
        "timing": {
            key: timing[key]
            for key in (
                "first_cycle_mismatch",
                "cycle_deltas",
                "first_same_cycle_order_mismatch",
                "same_cycle_order_mismatches",
                "sample_boundary_crossings",
            )
        },
        "logical_state_divergence": state_divergence,
        "hashes": {
            "reference_trace_sha256": sha256_file(reference_path),
            "candidate_trace_sha256": sha256_file(candidate_path),
            "reference_payload_sha256": hashlib.sha256(reference_payload_bytes).hexdigest(),
            "candidate_payload_sha256": hashlib.sha256(candidate_payload_bytes).hexdigest(),
        },
    }


def parse_args():
    """Parse the single generic comparator CLI without a PSW compatibility alias."""
    parser = argparse.ArgumentParser(prog="driver_compare", description=__doc__)
    parser.add_argument("reference", type=Path, help="reference version-1 trace")
    parser.add_argument("candidate", type=Path, help="candidate version-1 trace")
    parser.add_argument("--family", choices=FAMILY_NAMES, required=True, help="resolved driver family")
    parser.add_argument("--output", type=Path, required=True, help="result JSON path")
    return parser.parse_args()


def main():
    """Run all required gates, writing JSON for semantic and diagnostic failures alike."""
    args = parse_args()
    try:
        result = compare(args.reference, args.candidate, args.family)
    except TraceError as error:
        print(f"driver_compare: {error}", file=sys.stderr)
        return 2
    text = json.dumps(result, indent=2, sort_keys=True, allow_nan=False) + "\n"
    try:
        args.output.write_text(text, encoding="utf-8")
    except OSError as error:
        print(f"driver_compare: cannot write {args.output}: {error}", file=sys.stderr)
        return 2
    sys.stdout.write(text)
    print(
        "driver_compare: family=%s transaction_exact=%s payload_exact=%s logical_state_exact=%s cycle_exact=%s"
        % (
            args.family,
            str(result["transaction_exact"]).lower(),
            str(result["payload_exact"]).lower(),
            str(result["logical_state_exact"]).lower(),
            str(result["cycle_exact"]).lower(),
        ),
        file=sys.stderr,
    )
    return 0 if result["transaction_exact"] and result["payload_exact"] and result["logical_state_exact"] else 1


if __name__ == "__main__":
    sys.exit(main())
