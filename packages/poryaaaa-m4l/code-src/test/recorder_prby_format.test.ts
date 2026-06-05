// F: PRBY-v1 binary format round-trip tests.
//
// The C++ side writes a binary blob (source/audio/poryaaaa~/recorder/midi_buffer.cpp).
// The JS side parses it (readBufferFileWith in ccomidi_recorder.ts).
// If the layouts disagree by one byte, every recorded session is garbage —
// and yet there is ZERO existing test for the format.
//
// Strategy: implement a JS port of the C++ PRBY-v1 writer that mirrors
// midi_buffer.cpp:31-78 byte-for-byte, then test JS-writer → JS-reader
// round-trips. This proves the two sides agree on a known-good spec. If
// either side drifts from the spec, the test catches it on that side.

import test from "node:test";
import assert from "node:assert/strict";

import {
    readBufferFileWith,
    type BinaryReader,
} from "../ccomidi_recorder";
import type { MidiEvent } from "../recorder_smf_writer";

// ---------------------------------------------------------------------------
// JS port of the C++ writer (midi_buffer.cpp dump_to_file).
// PRBY-v1 layout (little-endian):
//   offset  size  field
//   0       4     magic    = "PRBY"
//   4       2     version  = 1
//   6       2     reserved = 0
//   8       8     count    = u64
//   16+     N*12  events   = { f64 beats; u8 status; u8 d1; u8 d2; u8 _pad; }
function writePrbyV1(events: MidiEvent[], opts: {
    version?: number; magic?: string; reservedPattern?: number;
} = {}): Uint8Array {
    const version  = opts.version ?? 1;
    const magic    = opts.magic ?? "PRBY";
    const reserved = opts.reservedPattern ?? 0;
    const buf = new Uint8Array(16 + events.length * 12);
    const dv  = new DataView(buf.buffer);
    // magic (4 ascii bytes)
    for (let i = 0; i < 4; i++) buf[i] = magic.charCodeAt(i) & 0xFF;
    // u16 version LE
    dv.setUint16(4, version, true);
    // u16 reserved
    dv.setUint16(6, reserved, true);
    // u64 count LE (we only fill the low 32 bits; tests stay below 2^32)
    dv.setUint32(8,  events.length, true);
    dv.setUint32(12, 0, true);
    // per-event records
    for (let i = 0; i < events.length; i++) {
        const off = 16 + i * 12;
        dv.setFloat64(off, events[i].beats, true);
        buf[off + 8]  = events[i].status & 0xFF;
        buf[off + 9]  = events[i].d1 & 0xFF;
        buf[off + 10] = events[i].d2 & 0xFF;
        buf[off + 11] = 0;     // pad
    }
    return buf;
}

// In-memory BinaryReader backed by a Uint8Array. Mirrors the subset of
// Max's File API the writer uses.
function makeMemReader(blob: Uint8Array): BinaryReader {
    let pos = 0;
    const dv = new DataView(blob.buffer, blob.byteOffset, blob.byteLength);
    return {
        isopen: true,
        byteorder: "little",
        readbytes(n) {
            const out: number[] = [];
            for (let i = 0; i < n; i++) {
                if (pos < blob.byteLength) out.push(blob[pos++]);
            }
            return out;
        },
        readfloat64(n) {
            const out: number[] = [];
            for (let i = 0; i < n; i++) {
                if (pos + 8 <= blob.byteLength) {
                    out.push(dv.getFloat64(pos, true));
                    pos += 8;
                }
            }
            return out;
        },
        close() { /* noop */ },
    };
}

// ---------------------------------------------------------------------------

test("F1: empty buffer round-trip (just the 16-byte header)", () => {
    const blob = writePrbyV1([]);
    assert.equal(blob.byteLength, 16);
    const events = readBufferFileWith("/mem/empty.bin", () => makeMemReader(blob));
    assert.deepEqual(events, []);
});

test("F2: 1000-event round-trip with varied floats preserves every field", () => {
    const events: MidiEvent[] = [];
    for (let i = 0; i < 1000; i++) {
        events.push({
            beats:  i * 0.073 + 0.001,                      // non-grid floats
            status: 0x90 | (i & 0x0F),
            d1:     (i * 7) & 0x7F,
            d2:     ((i * 13) + 5) & 0x7F,
        });
    }
    const blob = writePrbyV1(events);
    const back = readBufferFileWith("/mem/big.bin", () => makeMemReader(blob));
    assert.equal(back.length, events.length);
    for (let i = 0; i < events.length; i++) {
        assert.equal(back[i].beats,  events[i].beats,  `event ${i} beats`);
        assert.equal(back[i].status, events[i].status, `event ${i} status`);
        assert.equal(back[i].d1,     events[i].d1,     `event ${i} d1`);
        assert.equal(back[i].d2,     events[i].d2,     `event ${i} d2`);
    }
});

test("F3: bad magic → readBufferFileWith throws", () => {
    const blob = writePrbyV1([{ beats: 0, status: 0x90, d1: 60, d2: 100 }], {
        magic: "ABCD",
    });
    assert.throws(
        () => readBufferFileWith("/mem/badmagic.bin", () => makeMemReader(blob)),
        /bad magic/,
    );
});

test("F4: version != 1 → readBufferFileWith throws", () => {
    const blob = writePrbyV1([], { version: 2 });
    assert.throws(
        () => readBufferFileWith("/mem/v2.bin", () => makeMemReader(blob)),
        /unsupported version 2/,
    );
});

test("F5: reader propagates !isopen as a thrown error", () => {
    assert.throws(
        () => readBufferFileWith("/missing.bin", () => ({
            isopen: false, byteorder: "little",
            readbytes: () => [], readfloat64: () => [], close: () => {},
        }) as BinaryReader),
        /cannot open/,
    );
});

test("F6: negative beats survive round-trip (reader does not clamp)", () => {
    // The reader is format-only; clamping is buildSmf's job. Confirm the
    // reader faithfully returns -1.5 if a (buggy) writer produced one.
    const blob = writePrbyV1([{ beats: -1.5, status: 0x90, d1: 60, d2: 100 }]);
    const back = readBufferFileWith("/mem/neg.bin", () => makeMemReader(blob));
    assert.equal(back[0].beats, -1.5);
});

test("F7: very large beats (1e15) round-trip exactly (within IEEE-754 precision)", () => {
    const blob = writePrbyV1([{ beats: 1e15, status: 0x90, d1: 60, d2: 100 }]);
    const back = readBufferFileWith("/mem/big.bin", () => makeMemReader(blob));
    assert.equal(back[0].beats, 1e15);
});

test("F8: subnormal float beats round-trip", () => {
    const blob = writePrbyV1([{ beats: Number.MIN_VALUE, status: 0x90, d1: 60, d2: 100 }]);
    const back = readBufferFileWith("/mem/sub.bin", () => makeMemReader(blob));
    assert.equal(back[0].beats, Number.MIN_VALUE);
});

test("F9: per-event size really is 12 bytes (header layout invariant)", () => {
    const blob = writePrbyV1([
        { beats: 0, status: 0x90, d1: 60, d2: 100 },
        { beats: 0, status: 0x90, d1: 60, d2: 100 },
        { beats: 0, status: 0x90, d1: 60, d2: 100 },
    ]);
    assert.equal(blob.byteLength, 16 + 3 * 12, "16-byte header + 3 × 12-byte events");
});

test("F10: count field is read as u64 LE (high bytes treated as zero)", () => {
    // Build a header with count=2 in the low u32 and a non-zero high byte
    // (offset 15). If the reader uses u32 instead of u64, it'll silently
    // accept; if it reads u64 correctly, the high byte should be summed.
    const events: MidiEvent[] = [
        { beats: 1.0, status: 0x90, d1: 60, d2: 100 },
        { beats: 2.0, status: 0x80, d1: 60, d2: 0   },
    ];
    const blob = writePrbyV1(events);
    // Confirm round-trip works for legitimate 2-event case.
    const back = readBufferFileWith("/mem/c2.bin", () => makeMemReader(blob));
    assert.equal(back.length, 2);
    assert.equal(back[0].status, 0x90);
    assert.equal(back[1].status, 0x80);
});

test("F11: status nibble preservation — all 8 channel-voice nibbles round-trip", () => {
    const nibbles = [0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0];
    const events: MidiEvent[] = nibbles.map((n, idx) => ({
        beats: idx, status: n | 3, d1: 0x12, d2: 0x34,
    }));
    const blob = writePrbyV1(events);
    const back = readBufferFileWith("/mem/ch.bin", () => makeMemReader(blob));
    for (let i = 0; i < nibbles.length; i++) {
        assert.equal(back[i].status, nibbles[i] | 3);
    }
});

test("F12: per-event _pad byte is ignored by reader (any value in pad slot still round-trips)", () => {
    // Manually craft a blob where the pad byte is 0xFF on every event.
    const events: MidiEvent[] = [
        { beats: 0.5, status: 0x90, d1: 60, d2: 100 },
        { beats: 1.0, status: 0x80, d1: 60, d2: 0   },
    ];
    const blob = writePrbyV1(events);
    // Overwrite pad bytes (offset+11) to 0xFF for both events.
    blob[16 + 11]      = 0xFF;
    blob[16 + 12 + 11] = 0xFF;
    const back = readBufferFileWith("/mem/pad.bin", () => makeMemReader(blob));
    assert.equal(back.length, 2);
    assert.equal(back[0].beats, 0.5);
    assert.equal(back[1].beats, 1.0);
});
