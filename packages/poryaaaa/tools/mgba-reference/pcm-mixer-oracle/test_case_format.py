#!/usr/bin/env python3
"""Focused tests for the hermetic PCM fixture descriptor contract."""

from __future__ import annotations

from collections.abc import Callable
import json
from pathlib import Path
import tempfile
import unittest


import case_format
import generate_pcm_mixer_oracles as generator

ROOT = Path(__file__).resolve().parent



class CaseFormatTests(unittest.TestCase):
    def setUp(self) -> None:
        self.source = ROOT / "inputs" / "ipatix-silence.s"
        self.values, self.descriptor = case_format.parse_source(self.source, "ipatix")

    def malformed_source(self, transform: Callable[[str], str]) -> Path:
        text = self.source.read_text(encoding="utf-8")
        changed = transform(text)
        temporary = tempfile.NamedTemporaryFile(mode="w", suffix=".s", delete=False, encoding="utf-8")
        self.addCleanup(Path(temporary.name).unlink, missing_ok=True)
        with temporary:
            temporary.write(changed)
        return Path(temporary.name)

    def test_checked_header_matches_declarative_contract(self) -> None:
        case_format.verify_c_header()

    def test_all_case_sources_are_canonical_and_match_inventory_settings(self) -> None:
        for mode, inventory_name in (("ipatix", "cases.json"), ("sappy", "cases-sappy.json")):
            inventory = json.loads((ROOT / inventory_name).read_text(encoding="utf-8"))
            for case in inventory["cases"]:
                values, descriptor = case_format.parse_source(ROOT / case["source"], mode)
                self.assertEqual(case_format.settings(values), case["settings"], case["id"])
                self.assertGreaterEqual(len(descriptor), case_format.DESCRIPTOR["offsets"]["wave_data"])

    def test_duplicate_descriptor_field_is_rejected(self) -> None:
        path = self.malformed_source(
            lambda text: text.replace(
                ".equ FIXTURE_BLOCK_COUNT, 1\n",
                ".equ FIXTURE_BLOCK_COUNT, 1\n.equ FIXTURE_BLOCK_COUNT, 1\n"))
        with self.assertRaisesRegex(case_format.CaseFormatError, "duplicate descriptor field"):
            case_format.parse_source(path, "ipatix")

    def test_malformed_descriptor_literal_is_rejected(self) -> None:
        path = self.malformed_source(
            lambda text: text.replace(".equ FIXTURE_BLOCK_COUNT, 1", ".equ FIXTURE_BLOCK_COUNT, 1 + 0"))
        with self.assertRaisesRegex(case_format.CaseFormatError, "malformed descriptor field"):
            case_format.parse_source(path, "ipatix")

    def test_missing_rom_tail_is_rejected(self) -> None:
        path = self.malformed_source(
            lambda text: text.replace("@ PORYAAAA_ROM_TAIL -\n", ""))
        with self.assertRaisesRegex(case_format.CaseFormatError, "missing its ROM-tail field"):
            case_format.parse_source(path, "ipatix")

    def test_raw_wave_rom_continuation_is_explicit(self) -> None:
        path = ROOT / "inputs" / "ipatix-bdpcm-block-boundary.s"
        _, descriptor = case_format.parse_source(path, "ipatix")
        embedded_size = case_format.embedded_descriptor_size(descriptor)
        self.assertEqual((embedded_size, len(descriptor)), (178, 240))
        self.assertEqual(
            descriptor[embedded_size:].hex(),
            "0000059444460094069ca0698446a07950340190636a2678c720304241d0700636"
            "d33cd4032618001030da68002a09d01d78ed07657814d4ed060dd4a169")

    def test_wrong_mode_include_is_rejected(self) -> None:
        path = self.malformed_source(lambda text: text.replace("ipatix_fixture.inc", "sappy_fixture.inc"))
        with self.assertRaisesRegex(case_format.CaseFormatError, "framing or fixture include"):
            case_format.parse_source(path, "ipatix")

    def test_generator_rejects_linked_descriptor_mismatch(self) -> None:
        case = next(case for case in json.loads((ROOT / "cases.json").read_text(encoding="utf-8"))["cases"]
                    if case["id"] == "silence")
        linked = bytearray(self.descriptor)
        linked[-1] ^= 1
        with self.assertRaisesRegex(generator.OracleError, "differs from the .fixture_case bytes"):
            generator.verify_descriptor(case, self.source, bytes(linked), self.source)

    def test_case_blob_uses_declared_envelope_and_schedule(self) -> None:
        rows = [(10, 20, 4, 0x040000A0, 0), (11, 21, 4, 0x040000A4, 0)]
        blob = case_format.make_case_blob("ipatix", self.descriptor, (1, 2), (30, 40), rows)
        header = case_format.CASE_HEADER.unpack_from(blob)
        self.assertEqual(header[:5], (b"IPCA", 1, case_format.CASE_HEADER.size, len(self.descriptor), 2))
        schedule = case_format.CASE_HEADER.size + len(self.descriptor)
        self.assertEqual(case_format.SCHEDULE_ROW.unpack_from(blob, schedule), (10, 20, 0x040000A0))
        self.assertEqual(case_format.SCHEDULE_ROW.unpack_from(
            blob, schedule + case_format.SCHEDULE_ROW.size), (11, 21, 0x040000A4))


if __name__ == "__main__":
    unittest.main()
