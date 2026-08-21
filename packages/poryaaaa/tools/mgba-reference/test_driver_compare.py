#!/usr/bin/env python3
"""Behavioral self-tests for the family-aware version-1 trace comparator."""
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


TOOL = Path(__file__).with_name("driver_compare.py")
DRIVER_COMPARE_SPEC = importlib.util.spec_from_file_location("driver_compare", TOOL)
DRIVER_COMPARE = importlib.util.module_from_spec(DRIVER_COMPARE_SPEC)
DRIVER_COMPARE_SPEC.loader.exec_module(DRIVER_COMPARE)
CLOCK_HZ = 16_777_216


# --- trace text helpers -----------------------------------------------------

def write_record(cycle, order, width, address, value):
    """Format one WRITE record in the canonical trace grammar."""
    return f"WRITE {cycle} {order} {width} 0x{address:08X} 0x{value:08X}"


def sample_record(cycle, order):
    """Format one SAMPLE record."""
    return f"SAMPLE {cycle} {order}"


def timer_record(cycle, order, value):
    """Format one TIMER record."""
    return f"TIMER {cycle} {order} {value}"


def begin_record(cycle, order):
    """Format one BEGIN marker."""
    return f"BEGIN {cycle} {order}"


def end_record(cycle, order):
    """Format one END marker."""
    return f"END {cycle} {order}"


def oracle_trace_lines():
    """The canonical PSW start scenario from plan section 2, plus excluded records.

    Retained: NR30=0x40, four 32-bit wave-RAM writes, NR30=0x00, NR31, NR33,
    NR34 trigger, NR51, NR32, NR30=0x80, NR34 trigger (13 records).  SAMPLE,
    TIMER, NR52, FIFO and a square-channel write are mixed in to prove they
    never participate in transaction equality.
    """
    return [
        begin_record(0, 0),
        write_record(64, 0, 2, 0x04000070, 0x00000040),  # NR30: dac off, bank bit set
        write_record(128, 0, 4, 0x04000090, 0x03020100),
        write_record(192, 0, 4, 0x04000094, 0x07060504),
        write_record(256, 0, 4, 0x04000098, 0x0B0A0908),
        write_record(320, 0, 4, 0x0400009C, 0x0F0E0D0C),
        sample_record(384, 0),
        write_record(448, 0, 2, 0x04000070, 0x00000000),  # NR30: bank 0
        write_record(512, 0, 1, 0x04000072, 0x000000FF),  # NR31: length 1
        write_record(576, 0, 1, 0x04000074, 0x00000034),  # NR33: freq low
        write_record(640, 0, 1, 0x04000075, 0x00000087),  # NR34: trigger
        write_record(704, 0, 2, 0x04000080, 0x000044FF),  # NR50/NR51 combined
        write_record(768, 0, 1, 0x04000073, 0x00000020),  # NR32: volume code 1
        write_record(832, 0, 2, 0x04000070, 0x00000080),  # NR30: dac on
        write_record(896, 0, 1, 0x04000075, 0x00000087),  # NR34: second trigger
        sample_record(960, 0),
        timer_record(1024, 0, 0),
        write_record(1088, 0, 2, 0x04000084, 0x00000080),  # NR52: excluded
        write_record(1152, 0, 4, 0x040000A0, 0xDEADBEEF),  # FIFO A: excluded
        write_record(1216, 0, 1, 0x04000063, 0x000000F0),  # square NR12: excluded
        end_record(1280, 0),
    ]


def trace_text(lines):
    """Render a complete version-1 trace text for a list of record lines."""
    return f"PORYAAAA_AUDIO_TRACE 1\nCLOCK {CLOCK_HZ}\n" + "\n".join(lines) + "\n"


def run_tool(reference_lines, candidate_lines, directory, family="psw"):
    """Run the comparator on two in-memory traces; return process and result path."""
    reference = directory / "reference.trace"
    candidate = directory / "candidate.trace"
    result = directory / "result.json"
    reference.write_text(trace_text(reference_lines), encoding="utf-8")
    candidate.write_text(trace_text(candidate_lines), encoding="utf-8")
    completed = subprocess.run(
        [sys.executable, str(TOOL), str(reference), str(candidate), "--family", family, "--output", str(result)],
        capture_output=True,
        text=True,
    )
    return completed, result


def shift_cycles(lines, shift):
    """Return the lines with every record cycle shifted by ``shift``."""
    shifted = []
    for line in lines:
        tokens = line.split(" ")
        tokens[1] = str(int(tokens[1]) + shift)
        shifted.append(" ".join(tokens))
    return shifted


def sq1_trace_lines():
    """Return an oracle-derived Sq1 register sequence with sweep and routing."""
    return [
        begin_record(0, 0),
        write_record(64, 0, 2, 0x04000060, 0x0000003D),
        write_record(128, 0, 1, 0x04000062, 0x00000080),
        write_record(192, 0, 1, 0x04000063, 0x000000A3),
        write_record(256, 0, 1, 0x04000064, 0x0000007A),
        write_record(320, 0, 1, 0x04000065, 0x000000C3),
        write_record(384, 0, 2, 0x04000080, 0x00001177),
        sample_record(448, 0),
        end_record(512, 0),
    ]


def sq2_trace_lines():
    """Return an oracle-derived Sq2 register sequence without Sq1 sweep state."""
    return [
        begin_record(0, 0),
        write_record(64, 0, 1, 0x04000068, 0x000000C0),
        write_record(128, 0, 1, 0x04000069, 0x000000A3),
        write_record(192, 0, 1, 0x0400006C, 0x0000007A),
        write_record(256, 0, 1, 0x0400006D, 0x000000C3),
        write_record(320, 0, 2, 0x04000080, 0x00002277),
        sample_record(384, 0),
        end_record(448, 0),
    ]


def directsound_trace_lines():
    """Return a timer-routed FIFO-A sequence with observed sample boundaries."""
    return [
        begin_record(0, 0),
        write_record(16, 0, 2, 0x04000084, 0x00000080),
        write_record(32, 0, 2, 0x04000082, 0x00000300),
        write_record(48, 0, 2, 0x04000088, 0x00000200),
        write_record(64, 0, 4, 0x040000A0, 0x04030201),
        timer_record(80, 0, 0),
        sample_record(96, 0),
        timer_record(112, 0, 0),
        sample_record(128, 0),
        end_record(144, 0),
    ]

DIRECTSOUND_TIMER = object()
DIRECTSOUND_REFILL_A = (
    (4, 0x040000A0, 0x04030201),
    (4, 0x040000A0, 0x08070605),
    (4, 0x040000A0, 0x0C0B0A09),
    (4, 0x040000A0, 0x100F0E0D),
)
DIRECTSOUND_REFILL_B = (
    (4, 0x040000A4, 0x14131211),
    (4, 0x040000A4, 0x18171615),
    (4, 0x040000A4, 0x1C1B1A19),
    (4, 0x040000A4, 0x201F1E1D),
)


def directsound_refill_trace_lines(events):
    """Render one paired DirectSound refill after the ordinary setup writes."""
    lines = [
        begin_record(0, 0),
        write_record(16, 0, 2, 0x04000084, 0x00000080),
        write_record(32, 0, 2, 0x04000082, 0x00000300),
        write_record(48, 0, 2, 0x04000088, 0x00000200),
    ]
    for index, event in enumerate(events):
        cycle = 64 + index * 16
        if event is DIRECTSOUND_TIMER:
            lines.append(timer_record(cycle, 0, 0))
        else:
            width, address, value = event
            lines.append(write_record(cycle, 0, width, address, value))
    lines.append(end_record(64 + len(events) * 16, 0))
    return lines



# --- tests ------------------------------------------------------------------

class DriverCompareTest(unittest.TestCase):
    """Exercise family projections and all strict comparator gates end to end."""

    def compare(self, reference_lines, candidate_lines):
        """Run the PSW profile in a scratch directory."""
        return self.compare_family("psw", reference_lines, candidate_lines)

    def compare_family(self, family, reference_lines, candidate_lines):
        """Run one resolved profile through the public CLI."""
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        return run_tool(reference_lines, candidate_lines, Path(directory.name), family)

    def test_sq1_exact_tracks_sweep_separately(self):
        """Sq1 retains NR10 and exposes sweep state without cross-family registers."""
        lines = sq1_trace_lines()
        completed, _ = self.compare_family("sq1", lines, lines)
        self.assertEqual(completed.returncode, 0)
        result = json.loads(completed.stdout)
        self.assertEqual(result["family"], "sq1")
        self.assertTrue(result["transaction_exact"])
        self.assertTrue(result["payload_exact"])
        self.assertTrue(result["logical_state_exact"])
        state = DRIVER_COMPARE.SquareState("sq1")
        for record in (
            DRIVER_COMPARE.TraceRecord(1, 0, 0, "WRITE", 2, 0x04000060, 0x3D),
            DRIVER_COMPARE.TraceRecord(2, 0, 0, "WRITE", 1, 0x04000062, 0x80),
            DRIVER_COMPARE.TraceRecord(3, 0, 0, "WRITE", 1, 0x04000063, 0xA3),
            DRIVER_COMPARE.TraceRecord(4, 0, 0, "WRITE", 1, 0x04000064, 0x7A),
            DRIVER_COMPARE.TraceRecord(5, 0, 0, "WRITE", 1, 0x04000065, 0xC3),
        ):
            state.apply(record)
        snapshot = state.snapshot()
        self.assertEqual(snapshot["nr10_sweep_time"], 3)
        self.assertTrue(snapshot["nr10_sweep_decrease"])
        self.assertEqual(snapshot["nr10_sweep_shift"], 5)

    def test_sq2_exact_has_no_sweep_field(self):
        """Sq2 projects its own registers and never fabricates Sq1 sweep state."""
        lines = sq2_trace_lines()
        completed, _ = self.compare_family("sq2", lines, lines)
        self.assertEqual(completed.returncode, 0)
        result = json.loads(completed.stdout)
        self.assertTrue(result["transaction_exact"])
        state = DRIVER_COMPARE.SquareState("sq2")
        state.apply(DRIVER_COMPARE.TraceRecord(1, 0, 0, "WRITE", 1, 0x04000068, 0xC0))
        self.assertNotIn("nr10", state.snapshot())
    def test_cross_family_register_replaces_sq1_transaction(self):
        """A Sq2 register in the Sq1 sequence fails at the first missing Sq1 ordinal."""
        reference = sq1_trace_lines()
        candidate = [
            write_record(64, 0, 1, 0x04000069, 0x000000F2)
            if line.startswith("WRITE 64 ") else line
            for line in reference
        ]
        completed, _ = self.compare_family("sq1", reference, candidate)
        self.assertEqual(completed.returncode, 1)
        result = json.loads(completed.stdout)
        self.assertFalse(result["transaction_exact"])
        self.assertFalse(result["logical_state_exact"])
        self.assertEqual(result["first_divergence"]["ordinal"], 0)
        self.assertEqual(result["first_divergence"]["reference"]["register"], "NR10")
        self.assertEqual(result["first_divergence"]["candidate"]["register"], "NR11")

    def test_extra_cross_family_register_is_excluded(self):
        """Sq1 ignores an unrelated inserted Sq2 write under its resolved profile."""
        reference = sq1_trace_lines()
        candidate = list(reference)
        candidate.insert(-2, write_record(400, 0, 1, 0x04000069, 0x000000F2))
        completed, _ = self.compare_family("sq1", reference, candidate)
        self.assertEqual(completed.returncode, 0)
        result = json.loads(completed.stdout)
        self.assertTrue(result["transaction_exact"])
        self.assertEqual(result["reference_event_count"], result["candidate_event_count"])

    def test_directsound_exact_tracks_fifo_timer_and_routing(self):
        """DirectSound gates FIFO words, TIMER events, and routing but not SAMPLE records."""
        reference = directsound_trace_lines()
        candidate = [line for line in reference if not line.startswith("SAMPLE ")]
        completed, _ = self.compare_family("directsound", reference, candidate)
        self.assertEqual(completed.returncode, 0)
        result = json.loads(completed.stdout)
        self.assertEqual(result["family"], "directsound")
        self.assertTrue(result["transaction_exact"])
        self.assertTrue(result["payload_exact"])
        self.assertTrue(result["logical_state_exact"])
        self.assertEqual(result["reference_event_count"], 6)

    def test_directsound_fifo_corruption_reports_first_transaction(self):
        """One FIFO payload byte corrupts the first WRITE transaction and queue state."""
        reference = directsound_trace_lines()
        candidate = [
            write_record(64, 0, 4, 0x040000A0, 0x0403FF01)
            if line.startswith("WRITE 64 ") else line
            for line in reference
        ]
        completed, _ = self.compare_family("directsound", reference, candidate)
        self.assertEqual(completed.returncode, 1)
        result = json.loads(completed.stdout)
        self.assertFalse(result["transaction_exact"])
        self.assertFalse(result["payload_exact"])
        self.assertFalse(result["logical_state_exact"])
        self.assertEqual(result["first_divergence"]["ordinal"], 3)
        self.assertEqual(result["first_divergence"]["reference"]["register"], "FIFO_A")
        self.assertEqual(result["logical_state_divergence"]["field"], "fifo_a_head")

    def test_directsound_timer_loss_reports_first_transaction(self):
        """Removing an overflow cannot be hidden by equal FIFO payload bytes."""
        reference = directsound_trace_lines()
        candidate = [line for line in reference if line != "TIMER 112 0 0"]
        completed, _ = self.compare_family("directsound", reference, candidate)
        self.assertEqual(completed.returncode, 1)
        result = json.loads(completed.stdout)
        self.assertFalse(result["transaction_exact"])
        self.assertTrue(result["payload_exact"])
        self.assertFalse(result["logical_state_exact"])
        self.assertEqual(result["first_divergence"]["ordinal"], 5)
        self.assertEqual(result["first_divergence"]["reference"]["kind"], "TIMER")

    def assert_directsound_refill_failure(self, reference_events, candidate_events):
        """Require a refill mutation to remain visible to the public comparator."""
        completed, _ = self.compare_family(
            "directsound",
            directsound_refill_trace_lines(reference_events),
            directsound_refill_trace_lines(candidate_events),
        )
        self.assertEqual(completed.returncode, 1)
        result = json.loads(completed.stdout)
        self.assertFalse(result["transaction_exact"])
        return result

    def test_directsound_timer_interleaved_in_paired_refill_canonicalizes(self):
        """mGBA's TIMER-in-burst order compares as the same paired DMA refill."""
        canonical = [*DIRECTSOUND_REFILL_A, *DIRECTSOUND_REFILL_B, DIRECTSOUND_TIMER]
        mgba_interleaved = [
            DIRECTSOUND_REFILL_A[0],
            DIRECTSOUND_REFILL_B[0],
            DIRECTSOUND_REFILL_A[1],
            DIRECTSOUND_TIMER,
            DIRECTSOUND_REFILL_B[1],
            DIRECTSOUND_REFILL_A[2],
            DIRECTSOUND_REFILL_B[2],
            DIRECTSOUND_REFILL_A[3],
            DIRECTSOUND_REFILL_B[3],
        ]
        completed, _ = self.compare_family(
            "directsound",
            directsound_refill_trace_lines(canonical),
            directsound_refill_trace_lines(mgba_interleaved),
        )
        self.assertEqual(completed.returncode, 0)
        result = json.loads(completed.stdout)
        self.assertTrue(result["transaction_exact"])
        self.assertTrue(result["payload_exact"])
        self.assertTrue(result["logical_state_exact"])
        self.assertEqual(result["reference_event_count"], 12)
        self.assertEqual(result["candidate_event_count"], 12)

    def test_directsound_timer_before_paired_refill_canonicalizes(self):
        """mGBA's asynchronous DMA request compares with candidate completion order."""
        candidate = [*DIRECTSOUND_REFILL_A, *DIRECTSOUND_REFILL_B, DIRECTSOUND_TIMER]
        mgba_requested = [DIRECTSOUND_TIMER, *DIRECTSOUND_REFILL_A, *DIRECTSOUND_REFILL_B]
        completed, _ = self.compare_family(
            "directsound",
            directsound_refill_trace_lines(mgba_requested),
            directsound_refill_trace_lines(candidate),
        )
        self.assertEqual(completed.returncode, 0)
        result = json.loads(completed.stdout)
        self.assertTrue(result["transaction_exact"])
        self.assertTrue(result["payload_exact"])
        self.assertTrue(result["logical_state_exact"])

    def test_directsound_refill_mutations_remain_visible_after_canonicalization(self):
        """Only the legal cross-FIFO/TIMER interleave is normalized."""
        canonical = [*DIRECTSOUND_REFILL_A, *DIRECTSOUND_REFILL_B, DIRECTSOUND_TIMER]
        interleaved = [
            DIRECTSOUND_REFILL_A[0],
            DIRECTSOUND_REFILL_B[0],
            DIRECTSOUND_REFILL_A[1],
            DIRECTSOUND_TIMER,
            DIRECTSOUND_REFILL_B[1],
            DIRECTSOUND_REFILL_A[2],
            DIRECTSOUND_REFILL_B[2],
            DIRECTSOUND_REFILL_A[3],
            DIRECTSOUND_REFILL_B[3],
        ]
        extra_word = [
            DIRECTSOUND_REFILL_A[0],
            DIRECTSOUND_REFILL_B[0],
            DIRECTSOUND_REFILL_A[1],
            DIRECTSOUND_TIMER,
            DIRECTSOUND_REFILL_B[1],
            DIRECTSOUND_REFILL_A[2],
            DIRECTSOUND_REFILL_B[2],
            DIRECTSOUND_REFILL_B[3],
            (4, 0x040000A4, 0x24232221),
            DIRECTSOUND_REFILL_A[3],
        ]
        extra_timer = list(interleaved)
        extra_timer.insert(7, DIRECTSOUND_TIMER)
        cases = (
            (
                "missing FIFO word",
                [event for event in interleaved if event != DIRECTSOUND_REFILL_B[2]],
                -1,
                False,
            ),
            ("extra FIFO word", extra_word, 1, False),
            (
                "reordered FIFO-A words",
                [
                    DIRECTSOUND_REFILL_A[1],
                    DIRECTSOUND_REFILL_B[0],
                    DIRECTSOUND_REFILL_A[0],
                    DIRECTSOUND_TIMER,
                    DIRECTSOUND_REFILL_B[1],
                    DIRECTSOUND_REFILL_A[2],
                    DIRECTSOUND_REFILL_B[2],
                    DIRECTSOUND_REFILL_A[3],
                    DIRECTSOUND_REFILL_B[3],
                ],
                0,
                False,
            ),
            (
                "wrong FIFO payload",
                [
                    (4, 0x040000A4, 0x1C1B1A18)
                    if event == DIRECTSOUND_REFILL_B[2] else event
                    for event in interleaved
                ],
                0,
                False,
            ),
            (
                "wrong FIFO channel",
                [
                    (4, 0x040000A0, DIRECTSOUND_REFILL_B[3][2])
                    if event == DIRECTSOUND_REFILL_B[3] else event
                    for event in interleaved
                ],
                0,
                False,
            ),
            ("extra TIMER", extra_timer, 1, True),
        )
        for name, candidate, count_delta, payload_exact in cases:
            with self.subTest(name=name):
                result = self.assert_directsound_refill_failure(canonical, candidate)
                self.assertEqual(
                    result["candidate_event_count"] - result["reference_event_count"],
                    count_delta,
                )
                self.assertEqual(result["payload_exact"], payload_exact)
                self.assertFalse(result["logical_state_exact"])

    def test_directsound_refill_control_write_is_a_hard_barrier(self):
        """A setup/control write prevents paired-refill reordering across it."""
        control = (2, 0x04000082, 0x00000300)
        canonical = [
            *DIRECTSOUND_REFILL_A,
            control,
            *DIRECTSOUND_REFILL_B,
            DIRECTSOUND_TIMER,
        ]
        mgba_interleaved = [
            DIRECTSOUND_REFILL_A[0],
            DIRECTSOUND_REFILL_B[0],
            DIRECTSOUND_REFILL_A[1],
            control,
            DIRECTSOUND_TIMER,
            DIRECTSOUND_REFILL_B[1],
            DIRECTSOUND_REFILL_A[2],
            DIRECTSOUND_REFILL_B[2],
            DIRECTSOUND_REFILL_A[3],
            DIRECTSOUND_REFILL_B[3],
        ]
        result = self.assert_directsound_refill_failure(canonical, mgba_interleaved)
        self.assertTrue(result["payload_exact"])

    def run_malformed(self, text, directory):
        """Run the comparator against a raw malformed candidate trace text."""
        reference = directory / "reference.trace"
        candidate = directory / "candidate.trace"
        result = directory / "result.json"
        reference.write_text(trace_text(oracle_trace_lines()), encoding="utf-8")
        candidate.write_text(text, encoding="utf-8")
        completed = subprocess.run(
            [sys.executable, str(TOOL), str(reference), str(candidate), "--family", "psw", "--output", str(result)],
            capture_output=True,
            text=True,
        )
        return completed, result

    def test_identical_traces_pass(self):
        """Identical traces pass every gate, write deterministic JSON, exit 0."""
        lines = oracle_trace_lines()
        completed, result_path = self.compare(lines, lines)
        self.assertEqual(completed.returncode, 0)
        result = json.loads(completed.stdout)
        for key in (
            "transaction_exact",
            "payload_exact",
            "logical_state_exact",
            "cycle_exact",
            "reference_event_count",
            "candidate_event_count",
            "first_divergence",
            "hashes",
        ):
            self.assertIn(key, result)
        self.assertTrue(result["transaction_exact"])
        self.assertTrue(result["payload_exact"])
        self.assertTrue(result["logical_state_exact"])
        self.assertTrue(result["cycle_exact"])
        self.assertEqual(result["reference_event_count"], 13)
        self.assertEqual(result["candidate_event_count"], 13)
        self.assertIsNone(result["first_divergence"])
        self.assertIsNone(result["logical_state_divergence"])
        self.assertEqual(result["payload"]["first_differing_byte"], None)
        self.assertEqual(result["payload"]["reference"]["wave_bytes"], list(range(16)))
        self.assertEqual(result["timing"]["cycle_deltas"], [])
        self.assertEqual(result["timing"]["same_cycle_order_mismatches"], [])
        self.assertIsNone(result["timing"]["first_same_cycle_order_mismatch"])
        self.assertEqual(result["timing"]["sample_boundary_crossings"], [])
        self.assertIn("driver_compare: family=psw transaction_exact=true payload_exact=true", completed.stderr)
        self.assertTrue(result_path.exists())
        self.assertEqual(result_path.read_text(encoding="utf-8"), completed.stdout)
        # The same command must produce byte-identical output on a second run.
        with tempfile.TemporaryDirectory() as directory:
            once_more = run_tool(lines, lines, Path(directory))
        self.assertEqual(completed.stdout, once_more[0].stdout)

    def test_cycle_only_difference_keeps_required_gates(self):
        """Shifting every cycle fails cycle_exact but never the required gates."""
        lines = oracle_trace_lines()
        completed, _ = self.compare(lines, shift_cycles(lines, 4096))
        self.assertEqual(completed.returncode, 0)
        result = json.loads(completed.stdout)
        self.assertTrue(result["transaction_exact"])
        self.assertTrue(result["payload_exact"])
        self.assertTrue(result["logical_state_exact"])
        self.assertFalse(result["cycle_exact"])
        self.assertEqual(
            result["timing"]["first_cycle_mismatch"],
            {"ordinal": 0, "reference_cycle": 64, "candidate_cycle": 4160},
        )
        self.assertEqual(len(result["timing"]["cycle_deltas"]), 13)
        self.assertTrue(all(delta["delta"] == 4096 for delta in result["timing"]["cycle_deltas"]))
        self.assertEqual(result["timing"]["same_cycle_order_mismatches"], [])
        self.assertIsNone(result["timing"]["first_same_cycle_order_mismatch"])
        self.assertEqual(result["timing"]["sample_boundary_crossings"], [])
        self.assertIn("cycle_exact=false", completed.stderr)

    def test_reordered_nr30_nr32_fails(self):
        """Swapping the NR30=0x80 and NR32 writes fails transaction and state."""
        lines = oracle_trace_lines()
        nr32_index = next(index for index, line in enumerate(lines) if line.startswith("WRITE 768 "))
        nr30_index = next(index for index, line in enumerate(lines) if line.startswith("WRITE 832 "))
        candidate = list(lines)
        candidate[nr32_index] = "WRITE 768 0 2 0x04000070 0x00000080"
        candidate[nr30_index] = "WRITE 832 0 1 0x04000073 0x00000020"
        completed, _ = self.compare(lines, candidate)
        self.assertEqual(completed.returncode, 1)
        result = json.loads(completed.stdout)
        self.assertFalse(result["transaction_exact"])
        self.assertFalse(result["logical_state_exact"])
        divergence = result["first_divergence"]
        self.assertEqual(divergence["kind"], "transaction")
        self.assertEqual(divergence["ordinal"], 10)
        self.assertEqual(divergence["reference"]["address"], "0x04000073")
        self.assertEqual(divergence["candidate"]["address"], "0x04000070")
        state = result["logical_state_divergence"]
        self.assertEqual(state["ordinal"], 10)
        self.assertEqual(state["field"], "nr30")
        self.assertEqual(state["reference"], 0)
        self.assertEqual(state["candidate"], 0x80)
        self.assertEqual(state["causal_record"]["reference"]["address"], "0x04000073")
        self.assertIn("driver_compare: family=psw transaction_exact=false", completed.stderr)

    def test_word_vs_byte_wave_writes_payload_still_exact(self):
        """Four word writes versus sixteen byte writes: the payload gate passes alone.

        This is the known oracle-versus-current mismatch shape: equal expanded
        waveform bytes must not excuse the wrong bus width.
        """
        lines = oracle_trace_lines()
        byte_lines = [
            write_record(128 + 8 * index, 0, 1, 0x04000090 + index, index) for index in range(16)
        ]
        candidate = []
        for line in lines:
            if line.startswith("WRITE 128 0 4 "):
                candidate.extend(byte_lines)
            elif not (line.startswith("WRITE 192 0 4 ") or line.startswith("WRITE 256 0 4 ")
                      or line.startswith("WRITE 320 0 4 ")):
                candidate.append(line)
        completed, _ = self.compare(lines, candidate)
        self.assertEqual(completed.returncode, 1)
        result = json.loads(completed.stdout)
        self.assertFalse(result["transaction_exact"])
        self.assertTrue(result["payload_exact"])
        self.assertFalse(result["logical_state_exact"])
        self.assertEqual(result["reference_event_count"], 13)
        self.assertEqual(result["candidate_event_count"], 25)
        divergence = result["first_divergence"]
        self.assertEqual(divergence["kind"], "transaction")
        self.assertEqual(divergence["ordinal"], 1)
        self.assertEqual(divergence["reference"]["width"], 4)
        self.assertEqual(divergence["candidate"]["width"], 1)
        self.assertEqual(divergence["reference"]["address"], "0x04000090")
        self.assertIsNone(result["payload"]["first_differing_byte"])
        self.assertEqual(
            result["hashes"]["reference_payload_sha256"],
            result["hashes"]["candidate_payload_sha256"],
        )
        self.assertIn("transaction_exact=false payload_exact=true", completed.stderr)

    def test_swapped_nibbles_identify_first_byte_and_nibble(self):
        """A true per-byte nibble swap reports byte 1, high nibble index 2."""
        lines = oracle_trace_lines()
        swapped = [
            write_record(128, 0, 4, 0x04000090, 0x30201000),
            write_record(192, 0, 4, 0x04000094, 0x70605040),
            write_record(256, 0, 4, 0x04000098, 0xB0A09080),
            write_record(320, 0, 4, 0x0400009C, 0xF0E0D0C0),
        ]
        candidate = []
        for line in lines:
            if line.startswith("WRITE 128 0 4 "):
                candidate.extend(swapped)
            elif not (line.startswith("WRITE 192 0 4 ") or line.startswith("WRITE 256 0 4 ")
                      or line.startswith("WRITE 320 0 4 ")):
                candidate.append(line)
        completed, _ = self.compare(lines, candidate)
        self.assertEqual(completed.returncode, 1)
        result = json.loads(completed.stdout)
        self.assertFalse(result["transaction_exact"])
        self.assertFalse(result["payload_exact"])
        divergence = result["first_divergence"]
        self.assertEqual(divergence["kind"], "transaction")
        self.assertEqual(divergence["ordinal"], 1)
        self.assertEqual(divergence["candidate"]["value"], "0x30201000")
        payload = result["payload"]
        self.assertEqual(payload["first_differing_byte"], 1)
        self.assertEqual(payload["first_differing_nibble"], 2)
        self.assertEqual(payload["reference_entry"], {"section": "wave", "index": 1, "value": 0x01})
        self.assertEqual(payload["candidate_entry"], {"section": "wave", "index": 1, "value": 0x10})
        self.assertEqual(payload["reference_nibble"], 0)
        self.assertEqual(payload["candidate_nibble"], 1)
        self.assertEqual(payload["reference"]["wave_bytes"][:4], [0x00, 0x01, 0x02, 0x03])
        self.assertEqual(payload["candidate"]["wave_bytes"][:4], [0x00, 0x10, 0x20, 0x30])

    def test_reversed_words_identify_first_byte_and_nibble(self):
        """Reversing word 1 reports the exact byte 4 / low-nibble mismatch."""
        lines = oracle_trace_lines()
        candidate = []
        for line in lines:
            if line.startswith("WRITE 192 0 4 "):
                candidate.append(write_record(192, 0, 4, 0x04000094, 0x04050607))
            else:
                candidate.append(line)
        completed, _ = self.compare(lines, candidate)
        self.assertEqual(completed.returncode, 1)
        result = json.loads(completed.stdout)
        payload = result["payload"]
        self.assertEqual(payload["first_differing_byte"], 4)
        self.assertEqual(payload["first_differing_nibble"], 9)
        self.assertEqual(payload["reference_entry"], {"section": "wave", "index": 4, "value": 0x04})
        self.assertEqual(payload["candidate_entry"], {"section": "wave", "index": 4, "value": 0x07})
        self.assertEqual(payload["reference_nibble"], 4)
        self.assertEqual(payload["candidate_nibble"], 7)

    def test_missing_nr34_trigger_fails(self):
        """Dropping the first NR34 trigger fails transaction and state gates."""
        lines = oracle_trace_lines()
        candidate = [line for line in lines if not line.startswith("WRITE 640 ")]
        completed, _ = self.compare(lines, candidate)
        self.assertEqual(completed.returncode, 1)
        result = json.loads(completed.stdout)
        self.assertFalse(result["transaction_exact"])
        self.assertFalse(result["logical_state_exact"])
        divergence = result["first_divergence"]
        self.assertEqual(divergence["ordinal"], 8)
        self.assertEqual(divergence["reference"]["register"], "NR34")
        self.assertEqual(divergence["candidate"]["register"], "NR50/NR51")

    def test_duplicate_nr34_trigger_fails(self):
        """An extra identical NR34 trigger fails at the following ordinal."""
        lines = oracle_trace_lines()
        candidate = []
        for line in lines:
            candidate.append(line)
            if line.startswith("WRITE 640 "):
                candidate.append("WRITE 656 0 1 0x04000075 0x00000087")
        completed, _ = self.compare(lines, candidate)
        self.assertEqual(completed.returncode, 1)
        result = json.loads(completed.stdout)
        self.assertFalse(result["transaction_exact"])
        self.assertFalse(result["logical_state_exact"])
        divergence = result["first_divergence"]
        self.assertEqual(divergence["ordinal"], 9)
        self.assertEqual(divergence["reference"]["register"], "NR50/NR51")
        self.assertEqual(divergence["candidate"]["register"], "NR34")

    def test_extra_nr34_different_value_fails(self):
        """An extra NR34 trigger with a different value fails both gates."""
        lines = oracle_trace_lines()
        candidate = []
        for line in lines:
            candidate.append(line)
            if line.startswith("WRITE 640 "):
                candidate.append("WRITE 656 0 1 0x04000075 0x000000C7")
        completed, _ = self.compare(lines, candidate)
        self.assertEqual(completed.returncode, 1)
        result = json.loads(completed.stdout)
        self.assertFalse(result["transaction_exact"])
        self.assertFalse(result["logical_state_exact"])
        self.assertEqual(result["first_divergence"]["kind"], "transaction")
        state = result["logical_state_divergence"]
        self.assertEqual(state["ordinal"], 9)
        self.assertEqual(state["field"], "nr34")
        self.assertEqual(state["reference"], 0x87)
        self.assertEqual(state["candidate"], 0xC7)

    def test_halfword_nr33_nr34_applies_high_byte(self):
        """A halfword NR33/NR34 write applies both bytes; NR34 high byte gates."""
        lines = oracle_trace_lines()
        reference = []
        candidate = []
        for line in lines:
            if line.startswith("WRITE 640 "):
                reference.append("WRITE 640 0 2 0x04000074 0x00008734")
                candidate.append("WRITE 640 0 2 0x04000074 0x00000734")
            else:
                reference.append(line)
                candidate.append(line)
        completed, _ = self.compare(reference, candidate)
        self.assertEqual(completed.returncode, 1)
        result = json.loads(completed.stdout)
        self.assertFalse(result["transaction_exact"])
        self.assertFalse(result["logical_state_exact"])
        divergence = result["first_divergence"]
        self.assertEqual(divergence["kind"], "transaction")
        self.assertEqual(divergence["ordinal"], 8)
        self.assertEqual(divergence["reference"]["value"], "0x00008734")
        self.assertEqual(divergence["candidate"]["value"], "0x00000734")
        state = result["logical_state_divergence"]
        self.assertEqual(state["ordinal"], 8)
        self.assertEqual(state["field"], "nr34")
        self.assertEqual(state["reference"], 0x87)
        self.assertEqual(state["candidate"], 0x07)

    def test_halfword_nr31_nr32_applies_high_byte(self):
        """A halfword NR31/NR32 write applies both bytes; NR32 volume differs."""
        lines = oracle_trace_lines()
        reference = []
        candidate = []
        for line in lines:
            if line.startswith("WRITE 512 "):
                reference.append("WRITE 512 0 2 0x04000072 0x000030FF")
                candidate.append("WRITE 512 0 2 0x04000072 0x000010FF")
            else:
                reference.append(line)
                candidate.append(line)
        completed, _ = self.compare(reference, candidate)
        self.assertEqual(completed.returncode, 1)
        result = json.loads(completed.stdout)
        self.assertFalse(result["transaction_exact"])
        self.assertFalse(result["logical_state_exact"])
        state = result["logical_state_divergence"]
        self.assertEqual(state["ordinal"], 6)
        self.assertEqual(state["field"], "nr32")
        self.assertEqual(state["reference"], 0x30)
        self.assertEqual(state["candidate"], 0x10)

    def test_direct_nr51_byte_write_participates_in_state(self):
        """A direct NR51 byte write is retained as a distinct transaction."""
        reference = list(oracle_trace_lines())
        candidate = list(reference)
        insertion = next(index for index, line in enumerate(reference) if line.startswith("WRITE 768 "))
        reference.insert(insertion, write_record(736, 0, 1, 0x04000081, 0x00000044))
        candidate.insert(insertion, write_record(736, 0, 1, 0x04000081, 0x00000045))
        completed, _ = self.compare(reference, candidate)
        self.assertEqual(completed.returncode, 1)
        result = json.loads(completed.stdout)
        self.assertFalse(result["transaction_exact"])
        self.assertFalse(result["logical_state_exact"])
        divergence = result["first_divergence"]
        self.assertEqual(divergence["ordinal"], 10)
        self.assertEqual(divergence["reference"]["address"], "0x04000081")
        self.assertEqual(divergence["candidate"]["address"], "0x04000081")
        state = result["logical_state_divergence"]
        self.assertEqual(state["ordinal"], 10)
        self.assertEqual(state["field"], "nr51")
        self.assertEqual(state["reference"], 0x44)
        self.assertEqual(state["candidate"], 0x45)

    def test_nr32_bit7_decodes_as_forced_75_percent_volume(self):
        """NR32 bit 7 is part of the GBA three-bit logical volume field."""
        reference = list(oracle_trace_lines())
        candidate = [
            "WRITE 768 0 1 0x04000073 0x000000A0"
            if line.startswith("WRITE 768 ")
            else line
            for line in reference
        ]
        completed, _ = self.compare(reference, candidate)
        self.assertEqual(completed.returncode, 1)
        result = json.loads(completed.stdout)
        self.assertFalse(result["logical_state_exact"])

        state = DRIVER_COMPARE.PswState()
        state.apply(DRIVER_COMPARE.TraceRecord(1, 0, 0, "WRITE", 1, 0x04000073, 0x20))
        self.assertEqual(state.snapshot()["nr32_volume_code"], 1)
        state.apply(DRIVER_COMPARE.TraceRecord(2, 0, 0, "WRITE", 1, 0x04000073, 0xA0))
        self.assertEqual(state.snapshot()["nr32_volume_code"], 5)

    def test_missing_trailing_nr34_trigger_fails_state(self):
        """Removing the final NR34 leaves an identical prefix but fails state."""
        lines = oracle_trace_lines()
        candidate = [line for line in lines if line != "WRITE 896 0 1 0x04000075 0x00000087"]
        completed, _ = self.compare(lines, candidate)
        self.assertEqual(completed.returncode, 1)
        result = json.loads(completed.stdout)
        self.assertFalse(result["transaction_exact"])
        self.assertFalse(result["logical_state_exact"])
        divergence = result["first_divergence"]
        self.assertEqual(divergence["kind"], "transaction")
        self.assertEqual(divergence["ordinal"], 12)
        self.assertEqual(divergence["reference"]["register"], "NR34")
        self.assertIsNone(divergence["candidate"])
        state = result["logical_state_divergence"]
        self.assertEqual(state["ordinal"], 12)
        self.assertEqual(state["field"], "record_count")
        self.assertEqual(state["causal_record"]["reference"]["register"], "NR34")
        self.assertIsNone(state["causal_record"]["candidate"])

    def test_extra_trailing_nr34_trigger_fails_state(self):
        """A trailing NR34 after an identical prefix fails the ordinal gate."""
        lines = oracle_trace_lines()
        candidate = []
        for line in lines:
            if line == "END 1280 0":
                candidate.append("WRITE 1240 0 1 0x04000075 0x00000087")
            candidate.append(line)
        completed, _ = self.compare(lines, candidate)
        self.assertEqual(completed.returncode, 1)
        result = json.loads(completed.stdout)
        self.assertFalse(result["transaction_exact"])
        self.assertFalse(result["logical_state_exact"])
        divergence = result["first_divergence"]
        self.assertEqual(divergence["kind"], "transaction")
        self.assertEqual(divergence["ordinal"], 13)
        self.assertIsNone(divergence["reference"])
        self.assertEqual(divergence["candidate"]["register"], "NR34")
        state = result["logical_state_divergence"]
        self.assertEqual(state["ordinal"], 13)
        self.assertEqual(state["field"], "record_count")
        self.assertIsNone(state["causal_record"]["reference"])
        self.assertEqual(state["causal_record"]["candidate"]["register"], "NR34")

    def test_pre_begin_psw_setup_writes_are_ignored(self):
        """PSW writes before BEGIN establish state and do not participate."""
        lines = oracle_trace_lines()
        pre_begin = "WRITE 32 0 4 0x04000090 0x03020100"
        candidate = [pre_begin] + shift_cycles(lines, 64)
        completed, _ = self.compare(lines, candidate)
        self.assertEqual(completed.returncode, 0)
        result = json.loads(completed.stdout)
        self.assertTrue(result["transaction_exact"])
        self.assertTrue(result["payload_exact"])
        self.assertTrue(result["logical_state_exact"])
        self.assertEqual(result["reference_event_count"], 13)
        self.assertEqual(result["candidate_event_count"], 13)

    def test_sample_before_first_write_crossing_reported(self):
        """A SAMPLE after BEGIN before the first write is an ordinal-zero crossing."""
        lines = oracle_trace_lines()
        reference = [lines[0], "SAMPLE 32 0"] + lines[1:]
        completed, _ = self.compare(reference, lines)
        self.assertEqual(completed.returncode, 0)
        result = json.loads(completed.stdout)
        self.assertEqual(
            result["timing"]["sample_boundary_crossings"],
            [
                {
                    "severity": "high",
                    "ordinal": 0,
                    "reference_crosses_sample": True,
                    "candidate_crosses_sample": False,
                    "reference_sample_cycle": 32,
                    "candidate_sample_cycle": None,
                    "reference": {
                        "line": 5,
                        "cycle": 64,
                        "order": 0,
                        "kind": "WRITE",
                        "register": "NR30",
                        "width": 2,
                        "address": "0x04000070",
                        "value": "0x00000040",
                    },
                    "candidate": {
                        "line": 4,
                        "cycle": 64,
                        "order": 0,
                        "kind": "WRITE",
                        "register": "NR30",
                        "width": 2,
                        "address": "0x04000070",
                        "value": "0x00000040",
                    },
                }
            ],
        )

    def test_maximum_content_byte_line_accepted(self):
        """The canonical 510-content-byte line (511 with LF) parses cleanly."""
        lines = oracle_trace_lines()
        candidate = ["# " + "X" * 508] + lines
        completed, _ = self.compare(lines, candidate)
        self.assertEqual(completed.returncode, 0)

    def test_malformed_traces_fail_closed(self):
        """Every grammar violation exits 2 with a diagnostic, no JSON, no file."""
        base = oracle_trace_lines()
        variants = [
            ("bad header", "PORYAAAA_AUDIO_TRACE 2\nCLOCK %d\n" % CLOCK_HZ + "\n".join(base) + "\n",
             "trace must begin"),
            ("empty file", "", "trace must begin"),
            ("missing clock", "PORYAAAA_AUDIO_TRACE 1\n" + "\n".join(base) + "\n",
             "precedes CLOCK"),
            ("wrong clock", trace_text(base).replace("CLOCK %d" % CLOCK_HZ, "CLOCK 16777217", 1),
             "invalid CLOCK"),
            ("duplicate clock",
             "PORYAAAA_AUDIO_TRACE 1\nCLOCK %d\nCLOCK %d\n" % (CLOCK_HZ, CLOCK_HZ) + "\n".join(base) + "\n",
             "invalid CLOCK"),
            ("event before clock",
             "PORYAAAA_AUDIO_TRACE 1\nWRITE 1 0 1 0x04000070 0x00000040\nCLOCK %d\n" % CLOCK_HZ
             + "\n".join(base) + "\n",
             "precedes CLOCK"),
            ("wrong write field count",
             trace_text(base).replace("WRITE 64 0 2 0x04000070 0x00000040", "WRITE 64 0 2 0x04000070", 1),
             "invalid trace event"),
            ("non-numeric cycle",
             trace_text(base).replace("WRITE 64 0 2", "WRITE abc 0 2", 1), "invalid trace event"),
            ("signed cycle",
             trace_text(base).replace("WRITE 64 0 2", "WRITE -1 0 2", 1), "invalid trace event"),
            ("order overflow",
             trace_text(base).replace("WRITE 64 0 2", "WRITE 64 4294967296 2", 1), "invalid trace event"),
            ("width overflow",
             trace_text(base).replace("WRITE 64 0 2", "WRITE 64 0 256", 1), "invalid trace event"),
            ("address without prefix",
             trace_text(base).replace("0x04000070", "04000070", 1), "invalid trace event"),
            ("uppercase hex prefix",
             trace_text(base).replace("0x04000070", "0X00000070", 1), "invalid trace event"),
            ("bad hex digit",
             trace_text(base).replace("0x00000040", "0x0000000G", 1), "invalid trace event"),
            ("duplicate position",
             trace_text(base).replace("WRITE 192 0 4", "WRITE 128 0 4", 1), "invalid trace event"),
            ("decreasing cycle",
             trace_text(base).replace("WRITE 448 0 2", "WRITE 60 0 2", 1), "invalid trace event"),
            ("tab separator",
             trace_text(base).replace("WRITE 64 0 2", "WRITE\t64 0 2", 1), "invalid trace event"),
            ("trailing space",
             trace_text(base).replace("0x00000040\n", "0x00000040 \n", 1), "invalid trace event"),
            ("two begin markers", trace_text(["BEGIN 0 0", "BEGIN 1 0", "END 2 0"]),
             "invalid BEGIN or END marker"),
            ("record after end", trace_text(base) + "WRITE 2000 0 1 0x04000073 0x00000020\n",
             "follows the END marker"),
            ("missing end", trace_text([line for line in base if line != "END 1280 0"]),
             "closed BEGIN/END interval"),
            ("overlong 511-byte line", trace_text(["# " + "X" * 509]), "exceeds the maximum length"),
            ("overlong multibyte comment", trace_text(["# " + "é" * 300]), "exceeds the maximum length"),
            ("line too long", trace_text(["# " + "X" * 600]), "exceeds the maximum length"),
            ("crlf line endings", trace_text(base).replace("\n", "\r\n"), "trace must begin"),
            ("crlf body line",
             trace_text(base).replace("WRITE 64 0 2 0x04000070 0x00000040\n",
                                      "WRITE 64 0 2 0x04000070 0x00000040\r\n", 1),
             "invalid trace event"),
            ("lone carriage return",
             trace_text(base).replace("0x00000040", "0x0000\r0040", 1),
             "invalid trace event"),
            ("no trailing newline",
             "PORYAAAA_AUDIO_TRACE 1\nCLOCK %d\n" % CLOCK_HZ + "\n".join(base),
             "exceeds the maximum length"),
        ]
        for name, text, fragment in variants:
            with self.subTest(name=name):
                with tempfile.TemporaryDirectory() as directory:
                    completed, result = self.run_malformed(text, Path(directory))
                    self.assertEqual(completed.returncode, 2, name)
                    self.assertEqual(completed.stdout, "")
                    self.assertIn("driver_compare:", completed.stderr)
                    self.assertIn(fragment, completed.stderr)
                    self.assertFalse(result.exists(), name)

    def test_sample_boundary_crossing_reported(self):
        """A SAMPLE between two writes in one trace only is reported, not fatal."""
        lines = oracle_trace_lines()
        candidate = [line for line in lines if line != "SAMPLE 384 0"]
        completed, _ = self.compare(lines, candidate)
        self.assertEqual(completed.returncode, 0)
        result = json.loads(completed.stdout)
        self.assertEqual(
            result["timing"]["sample_boundary_crossings"],
            [
                {
                    "severity": "high",
                    "ordinal": 5,
                    "reference_crosses_sample": True,
                    "candidate_crosses_sample": False,
                    "reference_sample_cycle": 384,
                    "candidate_sample_cycle": None,
                    "reference": {
                        "line": 10,
                        "cycle": 448,
                        "order": 0,
                        "kind": "WRITE",
                        "register": "NR30",
                        "width": 2,
                        "address": "0x04000070",
                        "value": "0x00000000",
                    },
                    "candidate": {
                        "line": 9,
                        "cycle": 448,
                        "order": 0,
                        "kind": "WRITE",
                        "register": "NR30",
                        "width": 2,
                        "address": "0x04000070",
                        "value": "0x00000000",
                    },
                }
            ],
        )

    def test_same_cycle_order_mismatch_reported(self):
        """Collapsing two cycles into one reports the order shift, keeps gates."""
        lines = oracle_trace_lines()
        candidate = []
        for line in lines:
            if line.startswith("WRITE 768 "):
                candidate.append("WRITE 704 1 1 0x04000073 0x00000020")
            else:
                candidate.append(line)
        completed, _ = self.compare(lines, candidate)
        self.assertEqual(completed.returncode, 0)
        result = json.loads(completed.stdout)
        self.assertFalse(result["cycle_exact"])
        self.assertTrue(result["transaction_exact"])
        expected_order_mismatch = {
            "ordinal": 10,
            "reference_same_cycle": False,
            "candidate_same_cycle": True,
            "reference_cycles": [704, 768],
            "candidate_cycles": [704, 704],
            "reference_orders": [0, 0],
            "candidate_orders": [0, 1],
            "reference_order_delta": None,
            "candidate_order_delta": 1,
        }
        self.assertEqual(result["timing"]["same_cycle_order_mismatches"], [expected_order_mismatch])
        self.assertEqual(result["timing"]["first_same_cycle_order_mismatch"], expected_order_mismatch)
        self.assertEqual(
            result["timing"]["first_cycle_mismatch"],
            {"ordinal": 10, "reference_cycle": 768, "candidate_cycle": 704},
        )


    def test_same_cycle_order_delta_mismatch_reported(self):
        """Different same-cycle order deltas are visible without gating semantics."""
        lines = oracle_trace_lines()
        reference = []
        candidate = []
        for line in lines:
            if line.startswith("WRITE 704 "):
                reference.append("WRITE 704 10 2 0x04000080 0x00004477")
                candidate.append("WRITE 704 10 2 0x04000080 0x00004477")
            elif line.startswith("WRITE 768 "):
                reference.append("WRITE 704 11 1 0x04000073 0x00000020")
                candidate.append("WRITE 704 12 1 0x04000073 0x00000020")
            else:
                reference.append(line)
                candidate.append(line)

        completed, _ = self.compare(reference, candidate)
        self.assertEqual(completed.returncode, 0)
        result = json.loads(completed.stdout)
        mismatch = result["timing"]["first_same_cycle_order_mismatch"]
        self.assertEqual(mismatch["ordinal"], 10)
        self.assertEqual(mismatch["reference_orders"], [10, 11])
        self.assertEqual(mismatch["candidate_orders"], [10, 12])
        self.assertEqual(mismatch["reference_order_delta"], 1)
        self.assertEqual(mismatch["candidate_order_delta"], 2)

    def test_non_retained_records_are_excluded(self):
        """FIFO, TIMER, NR52, and square-channel records never affect equality."""
        lines = oracle_trace_lines()
        excluded = {
            "WRITE 1088 0 2 0x04000084 0x00000080",
            "WRITE 1152 0 4 0x040000A0 0xDEADBEEF",
            "WRITE 1216 0 1 0x04000063 0x000000F0",
            "TIMER 1024 0 0",
        }
        candidate = [line for line in lines if line not in excluded]
        completed, _ = self.compare(lines, candidate)
        self.assertEqual(completed.returncode, 0)
        result = json.loads(completed.stdout)
        self.assertTrue(result["transaction_exact"])
        self.assertTrue(result["payload_exact"])
        self.assertEqual(result["reference_event_count"], 13)
        self.assertEqual(result["candidate_event_count"], 13)

    def test_comments_and_blank_lines_accepted(self):
        """Grammar comment and blank lines are skipped without penalty."""
        lines = oracle_trace_lines()
        candidate = [lines[0], "# setup comment", ""] + lines[1:]
        completed, _ = self.compare(lines, candidate)
        self.assertEqual(completed.returncode, 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)