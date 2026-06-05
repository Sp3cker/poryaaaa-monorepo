import test from "node:test";
import assert from "node:assert/strict";

import { parseBarBeatSixteenth, parseBeatNumber, buildSmf, PPQ } from "../recorder_smf_writer";

// ---------------------------------------------------------------------------
// Ableton bar.beat.sixteenth → quarter-beats parser. Honors time signature.

test("parseBarBeatSixteenth — 4/4 basic positions", () => {
    // 1.1.1 = song start
    assert.equal(parseBarBeatSixteenth("1.1.1", 4, 4), 0);
    // 2.1.1 = one bar in = 4 quarter-beats
    assert.equal(parseBarBeatSixteenth("2.1.1", 4, 4), 4);
    // 1.2.1 = beat 2 of bar 1 = 1 quarter-beat
    assert.equal(parseBarBeatSixteenth("1.2.1", 4, 4), 1);
    // 1.1.5 = 4 sixteenths in = 1 quarter-beat (sixteenth indexing is /4)
    assert.equal(parseBarBeatSixteenth("1.1.5", 4, 4), 1);
});

test("parseBarBeatSixteenth — shorthand forms", () => {
    assert.equal(parseBarBeatSixteenth("37",    4, 4), 36 * 4);       // 144
    assert.equal(parseBarBeatSixteenth("37.2",  4, 4), 36 * 4 + 1);   // 145
    assert.equal(parseBarBeatSixteenth("37.2.3", 4, 4), 36 * 4 + 1 + 0.5); // 145.5
});

test("parseBarBeatSixteenth — 6/8 bar length is 3 quarter-beats", () => {
    // In 6/8, one bar = 6 eighths = 3 quarter-notes
    assert.equal(parseBarBeatSixteenth("2.1.1", 6, 8), 3);
    // beat 4 in 6/8 = 3 eighths in = 1.5 quarter-beats
    assert.equal(parseBarBeatSixteenth("1.4.1", 6, 8), 1.5);
});

test("parseBarBeatSixteenth — empty / malformed → null", () => {
    assert.equal(parseBarBeatSixteenth("",        4, 4), null);
    assert.equal(parseBarBeatSixteenth("   ",     4, 4), null);
    assert.equal(parseBarBeatSixteenth("garbage", 4, 4), null);
    assert.equal(parseBarBeatSixteenth("0.1.1",   4, 4), null); // bar 0 invalid
    assert.equal(parseBarBeatSixteenth("1.0.1",   4, 4), null); // beat 0 invalid
    assert.equal(parseBarBeatSixteenth("-1.1.1",  4, 4), null); // negative
});

test("parseBarBeatSixteenth — tolerates whitespace inside", () => {
    assert.equal(parseBarBeatSixteenth(" 37 . 2 . 3 ", 4, 4), 145.5);
});

test("parseBeatNumber — plain beat counts, not BBS positions", () => {
    assert.equal(parseBeatNumber("16"), 16);
    assert.equal(parseBeatNumber("56.5"), 56.5);
    assert.equal(parseBeatNumber(" 0 "), 0);
    assert.equal(parseBeatNumber("15.1.1"), null);
    assert.equal(parseBeatNumber("-1"), null);
    assert.equal(parseBeatNumber("garbage"), null);
});

// ---------------------------------------------------------------------------
// buildSmf with range filtering — events outside [start, end] are dropped
// and tick 0 of the SMF corresponds to range.startBeats.

function parseAnchoredFirstNoteTick(bytes: Uint8Array): number | null {
    // Walk the first music MTrk after the conductor; reconstruct absolute
    // tick of the first 0x90 event. Returns null if no note-on found.
    let i = 0;
    // Skip MThd (4 + 4 + headerLen)
    if (bytes[i] !== 0x4d) throw new Error("not an SMF");
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

    // Skip conductor track (first MTrk).
    let trackIndex = 0;
    while (i < bytes.length) {
        if (bytes[i] !== 0x4d) return null;
        i += 4;
        const trackLen = (bytes[i] << 24) | (bytes[i + 1] << 16) | (bytes[i + 2] << 8) | bytes[i + 3];
        i += 4;
        const trackEnd = i + trackLen;
        if (trackIndex === 0) { i = trackEnd; trackIndex++; continue; }

        let absTick = 0;
        let runningStatus = 0;
        while (i < trackEnd) {
            absTick += readVarInt();
            let status = bytes[i];
            if (status < 0x80) status = runningStatus; else { i++; runningStatus = status; }
            if (status === 0xFF) { i++; const ml = readVarInt(); i += ml; continue; }
            if (status === 0xF0 || status === 0xF7) { const sl = readVarInt(); i += sl; continue; }
            const high = status & 0xF0;
            const d1 = bytes[i++];
            const d2 = (high === 0xC0 || high === 0xD0) ? 0 : bytes[i++];
            if (high === 0x90 && d2 > 0) return absTick;
        }
        i = trackEnd;
        trackIndex++;
    }
    return null;
}

test("buildSmf — range trims leading silence", () => {
    // Event at beat 144 (song bar 37 in 4/4). Without range it lands at tick 69120.
    // With range starting at beat 144 it should land at tick 0.
    const bytes = buildSmf({
        events:   [{ beats: 144, status: 0x90, d1: 60, d2: 100 }],
        tempo:    120,
        loop:     { on: false, start: 0, length: 0 },
        timeSig:  { num: 4, den: 4 },
        voicemap: new Map(),
        range:    { startBeats: 144, endBeats: 200 },
    });
    const tick = parseAnchoredFirstNoteTick(bytes);
    assert.equal(tick, 0);
});

test("buildSmf — range drops events outside window", () => {
    const bytes = buildSmf({
        events: [
            { beats: 50,  status: 0x90, d1: 60, d2: 100 },   // before window — drop
            { beats: 144, status: 0x90, d1: 61, d2: 100 },   // inside — keep at tick 0
            { beats: 250, status: 0x90, d1: 62, d2: 100 },   // after window — drop
        ],
        tempo:    120,
        loop:     { on: false, start: 0, length: 0 },
        timeSig:  { num: 4, den: 4 },
        voicemap: new Map(),
        range:    { startBeats: 144, endBeats: 200 },
    });

    // Only one note-on should be in the file: pitch 61 at tick 0.
    let noteOnCount = 0;
    for (let i = 0; i < bytes.length - 2; i++) {
        if ((bytes[i] & 0xF0) === 0x90 && bytes[i + 2] > 0) noteOnCount++;
    }
    assert.equal(noteOnCount, 1);
});

test("buildSmf — no range preserves song-absolute behavior", () => {
    const bytes = buildSmf({
        events:   [{ beats: 144, status: 0x90, d1: 60, d2: 100 }],
        tempo:    120,
        loop:     { on: false, start: 0, length: 0 },
        timeSig:  { num: 4, den: 4 },
        voicemap: new Map(),
        // no range
    });
    const tick = parseAnchoredFirstNoteTick(bytes);
    assert.equal(tick, 144 * PPQ + 1);
});
