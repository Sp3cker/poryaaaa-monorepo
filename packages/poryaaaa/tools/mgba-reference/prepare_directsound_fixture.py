#!/usr/bin/env python3
"""Build a private se_pc_on fixture with one DirectSound voice at program 0."""

import argparse
import hashlib
import json
import shutil
import struct
import subprocess
from pathlib import Path


ROM_BASE = 0x08000000
TONE_DATA_SIZE = 12
VOICE_COMMAND = 0xBD
FIXTURE_VOICE_AGBMAIN_OFFSET = 128


class FixtureError(Exception):
    """Represent an invalid input that prevents an isolated fixture."""


def parse_args():
    """Describe the deliberately narrow fixture-building interface."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--decomp", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--nm", default="arm-none-eabi-nm")
    parser.add_argument("--song", default="se_pc_on")
    parser.add_argument("--voicegroup", default="voicegroup_rg_poke_center")
    parser.add_argument("--voice", type=int, default=4)
    parser.add_argument("--song-voice", type=int, default=4)
    parser.add_argument("--voicegroup-file", default="rg_poke_center")
    parser.add_argument("--sample-symbol", default="DirectSoundWaveData_sd90_classical_detuned_ep1_high")
    parser.add_argument("--sample-file", default="sd90_classical_detuned_ep1_high.bin")
    return parser.parse_args()


def sha256(path):
    """Return a stable provenance hash for one generated or source file."""
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_symbols(elf, nm):
    """Read ELF symbols needed to patch the copied ROM."""
    result = subprocess.run([nm, "-n", str(elf)], check=True, capture_output=True, text=True)
    symbols = {}
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) >= 3:
            symbols[fields[-1]] = int(fields[0], 16)
    return symbols


def require_symbol(symbols, name):
    """Resolve one symbol or explain why the fixture cannot be built."""
    try:
        return symbols[name]
    except KeyError as error:
        raise FixtureError(f"ELF symbol not found: {name}") from error


def rom_offset(address, rom_size, label):
    """Convert one mapped ROM address to a checked file offset."""
    offset = address - ROM_BASE
    if offset < 0 or offset >= rom_size:
        raise FixtureError(f"{label} address is outside the ROM: 0x{address:08X}")
    return offset


def read_vlq(data, offset, end):
    """Read one SMF variable-length quantity and return its value and end."""
    value = 0
    for _ in range(4):
        if offset >= end:
            raise FixtureError("truncated MIDI variable-length quantity")
        byte = data[offset]
        offset += 1
        value = (value << 7) | (byte & 0x7F)
        if not byte & 0x80:
            return value, offset
    raise FixtureError("MIDI variable-length quantity exceeds four bytes")


def remap_midi_programs(source, destination, expected_program):
    """Copy an SMF while changing every program change from expected to zero."""
    data = bytearray(source.read_bytes())
    if data[:4] != b"MThd" or len(data) < 14:
        raise FixtureError(f"not a standard MIDI file: {source}")
    header_size = struct.unpack_from(">I", data, 4)[0]
    offset = 8 + header_size
    remapped = 0
    while offset < len(data):
        if data[offset : offset + 4] != b"MTrk" or offset + 8 > len(data):
            raise FixtureError(f"invalid MIDI track chunk at byte {offset}")
        track_length = struct.unpack_from(">I", data, offset + 4)[0]
        pos = offset + 8
        end = pos + track_length
        if end > len(data):
            raise FixtureError("truncated MIDI track")
        running_status = None
        while pos < end:
            _, pos = read_vlq(data, pos, end)
            if pos >= end:
                raise FixtureError("truncated MIDI event")
            if data[pos] & 0x80:
                status = data[pos]
                pos += 1
                if status < 0xF0:
                    running_status = status
            elif running_status is not None:
                status = running_status
            else:
                raise FixtureError("MIDI running status has no prior channel status")

            if status == 0xFF:
                if pos >= end:
                    raise FixtureError("truncated MIDI meta event")
                pos += 1
                length, pos = read_vlq(data, pos, end)
                pos += length
            elif status in (0xF0, 0xF7):
                length, pos = read_vlq(data, pos, end)
                pos += length
            else:
                kind = status & 0xF0
                width = 1 if kind in (0xC0, 0xD0) else 2
                if pos + width > end:
                    raise FixtureError("truncated MIDI channel event")
                if kind == 0xC0:
                    if data[pos] != expected_program:
                        raise FixtureError(
                            f"unexpected MIDI program {data[pos]}; expected only {expected_program}"
                        )
                    data[pos] = 0
                    remapped += 1
                pos += width
        offset = end
    if remapped == 0:
        raise FixtureError("MIDI contains no program changes to remap")
    destination.write_bytes(data)
    return remapped


def write_project_overlay(output_dir, sample_source, sample_symbol, voice_line):
    """Create the minimum project tree accepted by the voicegroup loader."""
    sound = output_dir / "project" / "sound"
    samples = sound / "direct_sound_samples"
    samples.mkdir(parents=True, exist_ok=True)
    (sound / "voice_groups.inc").write_text(
        "directsound_fixture::\n" + voice_line + "\n\t.align 2\n", encoding="utf-8"
    )
    (sound / "direct_sound_data.inc").write_text(
        f"{sample_symbol}::\n"
        f'\t.incbin "sound/direct_sound_samples/{sample_source.name}"\n',
        encoding="utf-8",
    )
    shutil.copy2(sample_source, samples / sample_source.name)


def build_fixture(args):
    """Patch copied inputs and emit a manifest describing every transformation."""
    decomp = args.decomp.resolve()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    rom_source = decomp / "pokeemerald-hearth.gba"
    elf = decomp / "pokeemerald-hearth.elf"
    midi_source = decomp / "sound" / "songs" / "midi" / f"{args.song}.mid"
    voicegroup_source = decomp / "sound" / "voicegroups" / f"{args.voicegroup_file}.inc"
    sample_source = decomp / "sound" / "direct_sound_samples" / args.sample_file
    for path in (rom_source, elf, midi_source, voicegroup_source, sample_source):
        if not path.is_file():
            raise FixtureError(f"missing fixture input: {path}")
    if args.voice < 0:
        raise FixtureError("--voice must be nonnegative")

    symbols = load_symbols(elf, args.nm)
    song_address = require_symbol(symbols, args.song)
    track_address = require_symbol(symbols, f"{args.song}_1")
    voicegroup_address = require_symbol(symbols, args.voicegroup)
    agbmain_address = require_symbol(symbols, "AgbMain")
    rom = bytearray(rom_source.read_bytes())
    source_voice_offset = rom_offset(
        voicegroup_address + args.voice * TONE_DATA_SIZE, len(rom), "source voice"
    )
    voice_record = bytes(rom[source_voice_offset : source_voice_offset + TONE_DATA_SIZE])
    if len(voice_record) != TONE_DATA_SIZE or voice_record[0] != 0:
        raise FixtureError("selected ROM tone is not a normal DirectSound voice")
    sample_address = require_symbol(symbols, args.sample_symbol)
    if struct.unpack_from("<I", voice_record, 4)[0] != sample_address:
        raise FixtureError("selected ROM tone does not reference the requested sample")

    # build_song_rom.sh installs a 124-byte bootstrap at AgbMain.  Keep the
    # fixture tone immediately after it, inside AgbMain's verified 388-byte
    # allocation, because a full 32 MiB GBA ROM cannot be extended safely.
    fixture_voice_address = agbmain_address + FIXTURE_VOICE_AGBMAIN_OFFSET
    fixture_voice_offset = rom_offset(fixture_voice_address, len(rom), "fixture voice")
    rom[fixture_voice_offset : fixture_voice_offset + TONE_DATA_SIZE] = voice_record

    song_offset = rom_offset(song_address, len(rom), "song header")
    struct.pack_into("<I", rom, song_offset + 4, fixture_voice_address)
    track_offset = rom_offset(track_address, len(rom), "song track")
    track_end = song_offset
    matches = [
        index
        for index in range(track_offset, track_end - 1)
        if rom[index] == VOICE_COMMAND and rom[index + 1] == args.song_voice
    ]
    if len(matches) != 1:
        raise FixtureError(
            f"expected one VOICE {args.song_voice} command in {args.song}, found {len(matches)}"
        )
    rom[matches[0] + 1] = 0

    patched_rom = output_dir / "fixture-base.gba"
    patched_rom.write_bytes(rom)
    patched_midi = output_dir / "fixture.mid"
    midi_program_changes = remap_midi_programs(midi_source, patched_midi, args.song_voice)

    voice_line = (
        f"\tvoice_directsound {voice_record[1]}, {voice_record[3]}, {args.sample_symbol}, "
        f"{voice_record[8]}, {voice_record[9]}, {voice_record[10]}, {voice_record[11]}"
    )
    write_project_overlay(output_dir, sample_source, args.sample_symbol, voice_line)
    (output_dir / "fixture_voicegroup.inc").write_text(
        "voice_group directsound_fixture\n" + voice_line + "\n", encoding="utf-8"
    )

    manifest = {
        "manifest_version": 1,
        "song": args.song,
        "source_program": args.voice,
        "source_song_program": args.song_voice,
        "fixture_program": 0,
        "voice_count": 1,
        "voice_type": "voice_directsound",
        "source_voice_address": f"0x{voicegroup_address + args.voice * TONE_DATA_SIZE:08X}",
        "fixture_voice_address": f"0x{fixture_voice_address:08X}",
        "rom_voice_command_offset": matches[0] - track_offset,
        "midi_program_changes_remapped": midi_program_changes,
        "inputs": {
            "rom": {"path": str(rom_source), "sha256": sha256(rom_source)},
            "elf": {"path": str(elf), "sha256": sha256(elf)},
            "midi": {"path": str(midi_source), "sha256": sha256(midi_source)},
            "voicegroup": {"path": str(voicegroup_source), "sha256": sha256(voicegroup_source)},
            "sample": {"path": str(sample_source), "sha256": sha256(sample_source)},
        },
        "outputs": {
            "rom": {"path": str(patched_rom), "sha256": sha256(patched_rom)},
            "midi": {"path": str(patched_midi), "sha256": sha256(patched_midi)},
        },
    }
    (output_dir / "fixture_manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )


def main():
    """Build the fixture and turn expected input failures into diagnostics."""
    try:
        build_fixture(parse_args())
    except (FixtureError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"error: {error}") from error


if __name__ == "__main__":
    main()
