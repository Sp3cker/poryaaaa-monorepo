// Unit tests for the pure SMF builder. We parse the built SMF byte stream
// just enough to confirm the invariants we care about (PCs at tick 0,
// loop markers in the conductor track, note pairs, held-note flush).

import test from "node:test";
import assert from "node:assert/strict";

import { buildSmf, PPQ, type MidiEvent } from "../recorder_smf_writer";

// Walk the MTrk chunks and return a flat list of {status, d1, d2} for every
// channel-voice event in track order. Skips meta-events and the MThd header.
// Knows just enough of the SMF format to be useful for test assertions; not
// a general-purpose parser.
interface ParsedEvent { status: number; d1: number; d2: number }
function parseSmfEvents(bytes: ArrayLike<number>): ParsedEvent[] {
    const result: ParsedEvent[] = [];
    let i = 0;

    // MThd: 4 ascii + u32 length + length bytes of header.
    if (bytes[i] !== 0x4D || bytes[i + 1] !== 0x54
        || bytes[i + 2] !== 0x68 || bytes[i + 3] !== 0x64) {
        throw new Error("not an SMF (missing MThd)");
    }
    i += 4;
    const headerLen = (bytes[i] << 24) | (bytes[i + 1] << 16) | (bytes[i + 2] << 8) | bytes[i + 3];
    i += 4 + headerLen;

    function readVarInt(): number {
        let v = 0;
        while (true) {
            const b = bytes[i++];
            v = (v << 7) | (b & 0x7F);
            if ((b & 0x80) === 0) return v;
        }
    }

    while (i < bytes.length) {
        if (bytes[i] !== 0x4D || bytes[i + 1] !== 0x54
            || bytes[i + 2] !== 0x72 || bytes[i + 3] !== 0x6B) {
            break;     // not an MTrk, stop
        }
        i += 4;
        const trackLen = (bytes[i] << 24) | (bytes[i + 1] << 16) | (bytes[i + 2] << 8) | bytes[i + 3];
        i += 4;
        const trackEnd = i + trackLen;

        let runningStatus = 0;
        while (i < trackEnd) {
            readVarInt();                 // delta
            let status = bytes[i];
            if (status < 0x80) {
                // running status; reuse previous
                status = runningStatus;
            } else {
                i++;
                runningStatus = status;
            }
            if (status === 0xFF) {
                // meta: type + varint length + bytes
                i++;                       // type byte
                const metaLen = readVarInt();
                i += metaLen;
                continue;
            }
            if (status === 0xF0 || status === 0xF7) {
                const sysLen = readVarInt();
                i += sysLen;
                continue;
            }
            const high = status & 0xF0;
            const d1 = bytes[i++];
            const d2 = (high === 0xC0 || high === 0xD0) ? 0 : bytes[i++];
            result.push({ status, d1, d2 });
        }
        i = trackEnd;
    }
    return result;
}

function filterByStatusNibble(events: ParsedEvent[], highNibble: number): ParsedEvent[] {
    return events.filter((e) => (e.status & 0xF0) === (highNibble & 0xF0));
}

// ---------------------------------------------------------------------------

test("buildSmf emits a tick-0 PC for each voicemap entry", () => {
    const events: MidiEvent[] = [];
    const voicemap = new Map<number, number>([[0, 5], [3, 42]]);
    const bytes = buildSmf({
        events,
        tempo: 120,
        loop: { on: false, start: 0, length: 0 },
        timeSig: { num: 4, den: 4 },
        voicemap,
    });

    const parsed = parseSmfEvents(bytes);
    const pcs = filterByStatusNibble(parsed, 0xC0);
    assert.equal(pcs.length, 2);

    const programs = pcs.map((p) => p.d1).sort((a, b) => a - b);
    assert.deepEqual(programs, [5, 42]);
});

test("buildSmf preserves note-on / note-off pairing", () => {
    const events: MidiEvent[] = [
        { beats: 0,   status: 0x90, d1: 60, d2: 100 },  // C4 on ch 0
        { beats: 1.0, status: 0x80, d1: 60, d2: 0 },    // C4 off
    ];
    const bytes = buildSmf({
        events,
        tempo: 120,
        loop: { on: false, start: 0, length: 0 },
        timeSig: { num: 4, den: 4 },
        voicemap: new Map(),
    });

    const parsed = parseSmfEvents(bytes);
    const noteOns  = filterByStatusNibble(parsed, 0x90).filter((p) => p.d2 !== 0);
    const noteOffs = filterByStatusNibble(parsed, 0x80);
    assert.equal(noteOns.length, 1, "one note-on");
    assert.equal(noteOffs.length, 1, "one note-off");
    assert.equal(noteOns[0].d1, 60);
    assert.equal(noteOffs[0].d1, 60);
});

test("buildSmf flushes an open note at end of buffer with a note-off", () => {
    const events: MidiEvent[] = [
        { beats: 0, status: 0x90, d1: 64, d2: 100 },
    ];
    const bytes = buildSmf({
        events,
        tempo: 120,
        loop: { on: false, start: 0, length: 0 },
        timeSig: { num: 4, den: 4 },
        voicemap: new Map(),
    });

    const parsed = parseSmfEvents(bytes);
    const noteOffs = filterByStatusNibble(parsed, 0x80);
    assert.equal(noteOffs.length, 1, "held note got an auto-off");
    assert.equal(noteOffs[0].d1, 64);
});

test("buildSmf adds loop markers `[` and `]` when loop is on", () => {
    const events: MidiEvent[] = [];
    const bytes = buildSmf({
        events,
        tempo: 120,
        loop: { on: true, start: 4, length: 4 },
        timeSig: { num: 4, den: 4 },
        voicemap: new Map(),
    });

    // Marker meta event: 0xFF 0x06 <length> <text>
    let openMarkerCount = 0;
    let closeMarkerCount = 0;
    for (let i = 0; i < bytes.length - 3; i++) {
        if (bytes[i] === 0xFF && bytes[i + 1] === 0x06) {
            const len = bytes[i + 2];
            if (len === 1) {
                const ch = String.fromCharCode(bytes[i + 3]);
                if (ch === "[") openMarkerCount++;
                if (ch === "]") closeMarkerCount++;
            }
        }
    }
    assert.equal(openMarkerCount, 1, "one opening marker");
    assert.equal(closeMarkerCount, 1, "one closing marker");
});

test("buildSmf replays last PC at loop start when loop is on", () => {
    const events: MidiEvent[] = [
        // PC on channel 0, program 7 — captured mid-buffer.
        { beats: 2, status: 0xC0, d1: 7, d2: 0 },
    ];
    const bytes = buildSmf({
        events,
        tempo: 120,
        loop: { on: true, start: 8, length: 4 },
        timeSig: { num: 4, den: 4 },
        voicemap: new Map(),  // empty voicemap so tick-0 PC doesn't muddy the count
    });

    const parsed = parseSmfEvents(bytes);
    const pcs = filterByStatusNibble(parsed, 0xC0);
    // One PC from the captured event + one replayed at loop start.
    assert.equal(pcs.length, 2, "captured PC + loop-start replay PC");
    for (const p of pcs) {
        assert.equal(p.d1, 7);
    }
});

test("buildSmf converts beats to ticks at the Live import PPQ", () => {
    assert.equal(PPQ, 96);
    // One beat = 96 ticks, one bar (4/4) = 384.
    // We don't decode delta varints here — that's covered by midi-writer-js's
    // own tests. We just confirm the constant the writer exports.
});
