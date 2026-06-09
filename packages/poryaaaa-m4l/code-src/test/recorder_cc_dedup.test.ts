// Regression coverage for CC dedup: ccomidi emits its dial state multiple
// times during a take (transport rising-edge / clip-launch / attribute-change
// triggers), so identical CCs pile up at the same tick AND across ticks. The
// recorder collapses these:
//   1. Same-tick last-wins for (channel, cc) — user typed this preference as
//      "for CC changes that all occur on the same measure, ie 12.1.0, the
//      latest CC change should apply."
//   2. Consecutive-value dedup across ticks for (channel, cc) — drops a
//      captured CC whose value equals the prior emitted value (including the
//      LOM snapshot's tick-0 initial state).
// XCMD controllers 0x1E/0x1D are deduped only as ordered same-tick pairs.

import test from "node:test";
import assert from "node:assert/strict";

import { buildSmf, PPQ, type MidiEvent } from "../recorder/recorder_smf_writer";

interface ParsedCc { tick: number; channel: number; cn: number; cv: number }

function parseCcStream(bytes: Uint8Array): ParsedCc[] {
    const result: ParsedCc[] = [];
    let i = 0;
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

    let trackIndex = 0;
    while (i < bytes.length) {
        if (bytes[i] !== 0x4d) break;
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
            if (status < 0x80) status = runningStatus;
            else { i++; runningStatus = status; }
            if (status === 0xFF) { i++; const ml = readVarInt(); i += ml; continue; }
            if (status === 0xF0 || status === 0xF7) { const sl = readVarInt(); i += sl; continue; }
            const high = status & 0xF0;
            const d1 = bytes[i++];
            const d2 = (high === 0xC0 || high === 0xD0) ? 0 : bytes[i++];
            if (high === 0xB0) {
                result.push({ tick: absTick, channel: status & 0x0F, cn: d1, cv: d2 });
            }
        }
        i = trackEnd;
        trackIndex++;
    }
    return result;
}

// ---------------------------------------------------------------------------
// Same-tick last-wins.

test("CC dedup — same-tick captures collapse to the LAST value", () => {
    // ccomidi-style noise: vol bouncing between 64 (default) and 97 (user)
    // at the same beat, repeating.
    const events: MidiEvent[] = [];
    for (let i = 0; i < 5; i++) {
        events.push({ beats: 4, status: 0xB0, d1: 0x07, d2: 64 });
        events.push({ beats: 4, status: 0xB0, d1: 0x07, d2: 97 });
    }
    const bytes = buildSmf({
        events,
        tempo: 120,
        loop: { on: false, start: 0, length: 0 },
        timeSig: { num: 4, den: 4 },
        voicemap: new Map(),
    });
    const ccs = parseCcStream(bytes).filter((c) => c.cn === 0x07);
    // Only one CC emitted: the LAST (97).
    assert.equal(ccs.length, 1);
    assert.equal(ccs[0].cv, 97);
});

test("CC dedup — same-tick last-wins applies independently per cc number", () => {
    const events: MidiEvent[] = [
        { beats: 4, status: 0xB0, d1: 0x07, d2: 64 },  // Volume 64
        { beats: 4, status: 0xB0, d1: 0x0A, d2: 30 },  // Pan 30
        { beats: 4, status: 0xB0, d1: 0x07, d2: 97 },  // Volume 97 (wins)
        { beats: 4, status: 0xB0, d1: 0x0A, d2: 70 },  // Pan 70 (wins)
    ];
    const bytes = buildSmf({
        events,
        tempo: 120,
        loop: { on: false, start: 0, length: 0 },
        timeSig: { num: 4, den: 4 },
        voicemap: new Map(),
    });
    const ccs = parseCcStream(bytes);
    const vol = ccs.find((c) => c.cn === 0x07);
    const pan = ccs.find((c) => c.cn === 0x0A);
    assert.equal(vol?.cv, 97);
    assert.equal(pan?.cv, 70);
    assert.equal(ccs.length, 2);
});

test("CC dedup — distinct XCMD prefix/value pairs keep their order at same tick", () => {
    // GBA XCMD sequence: 0x1E selector, 0x1D value, 0x1E selector, 0x1D value.
    // Each emission is semantically distinct.
    const events: MidiEvent[] = [
        { beats: 4, status: 0xB0, d1: 0x1E, d2: 0x02 },
        { beats: 4, status: 0xB0, d1: 0x1D, d2: 0x05 },
        { beats: 4, status: 0xB0, d1: 0x1E, d2: 0x04 },
        { beats: 4, status: 0xB0, d1: 0x1D, d2: 0x10 },
    ];
    const bytes = buildSmf({
        events,
        tempo: 120,
        loop: { on: false, start: 0, length: 0 },
        timeSig: { num: 4, den: 4 },
        voicemap: new Map(),
    });
    const ccs = parseCcStream(bytes);
    assert.equal(ccs.length, 4);
    assert.deepEqual(ccs.map((c) => c.cn), [0x1E, 0x1D, 0x1E, 0x1D]);
    assert.deepEqual(ccs.map((c) => c.cv), [0x02, 0x05, 0x04, 0x10]);
});

test("CC dedup — identical same-tick XCMD pairs collapse as ordered pairs", () => {
    const events: MidiEvent[] = [
        { beats: 4, status: 0xB0, d1: 0x1E, d2: 0x02 },
        { beats: 4, status: 0xB0, d1: 0x1D, d2: 0x05 },
        { beats: 4, status: 0xB0, d1: 0x1E, d2: 0x02 },
        { beats: 4, status: 0xB0, d1: 0x1D, d2: 0x05 },
        { beats: 4, status: 0xB0, d1: 0x1E, d2: 0x04 },
        { beats: 4, status: 0xB0, d1: 0x1D, d2: 0x10 },
    ];
    const bytes = buildSmf({
        events,
        tempo: 120,
        loop: { on: false, start: 0, length: 0 },
        timeSig: { num: 4, den: 4 },
        voicemap: new Map(),
    });
    const ccs = parseCcStream(bytes);
    assert.equal(ccs.length, 4);
    assert.deepEqual(ccs.map((c) => c.cn), [0x1E, 0x1D, 0x1E, 0x1D]);
    assert.deepEqual(ccs.map((c) => c.cv), [0x02, 0x05, 0x04, 0x10]);
});

test("CC dedup — malformed XCMD order is not pair-deduped", () => {
    const events: MidiEvent[] = [
        { beats: 4, status: 0xB0, d1: 0x1D, d2: 0x05 },
        { beats: 4, status: 0xB0, d1: 0x1E, d2: 0x02 },
        { beats: 4, status: 0xB0, d1: 0x1D, d2: 0x05 },
    ];
    const bytes = buildSmf({
        events,
        tempo: 120,
        loop: { on: false, start: 0, length: 0 },
        timeSig: { num: 4, den: 4 },
        voicemap: new Map(),
    });
    const ccs = parseCcStream(bytes);
    assert.equal(ccs.length, 3);
    assert.deepEqual(ccs.map((c) => c.cn), [0x1D, 0x1E, 0x1D]);
    assert.deepEqual(ccs.map((c) => c.cv), [0x05, 0x02, 0x05]);
});

// ---------------------------------------------------------------------------
// Consecutive-value dedup across ticks.

test("CC dedup — across-tick consecutive same values collapse", () => {
    // ccomidi re-emits Volume=97 on every clip launch; the user wants only
    // the first to land in the SMF.
    const events: MidiEvent[] = [
        { beats: 1, status: 0xB0, d1: 0x07, d2: 97 },
        { beats: 2, status: 0xB0, d1: 0x07, d2: 97 },
        { beats: 3, status: 0xB0, d1: 0x07, d2: 97 },
    ];
    const bytes = buildSmf({
        events,
        tempo: 120,
        loop: { on: false, start: 0, length: 0 },
        timeSig: { num: 4, den: 4 },
        voicemap: new Map(),
    });
    const ccs = parseCcStream(bytes).filter((c) => c.cn === 0x07);
    assert.equal(ccs.length, 1);
    assert.equal(ccs[0].tick, PPQ + 1);
});

test("CC dedup — value change re-emits, value-back-to-same is deduped", () => {
    const events: MidiEvent[] = [
        { beats: 1, status: 0xB0, d1: 0x07, d2: 97 },   // emit
        { beats: 2, status: 0xB0, d1: 0x07, d2: 97 },   // drop (same)
        { beats: 3, status: 0xB0, d1: 0x07, d2: 80 },   // emit (change)
        { beats: 4, status: 0xB0, d1: 0x07, d2: 80 },   // drop (same)
        { beats: 5, status: 0xB0, d1: 0x07, d2: 97 },   // emit (change back)
    ];
    const bytes = buildSmf({
        events,
        tempo: 120,
        loop: { on: false, start: 0, length: 0 },
        timeSig: { num: 4, den: 4 },
        voicemap: new Map(),
    });
    const ccs = parseCcStream(bytes).filter((c) => c.cn === 0x07);
    assert.equal(ccs.length, 3);
    assert.deepEqual(ccs.map((c) => c.cv), [97, 80, 97]);
});

test("CC dedup — initialCcs seeds the dedup baseline", () => {
    // The save-time initial snapshot reports Volume=97. Captured stream has
    // Volume=97 at beat 4. The captured one should be DROPPED because the
    // tick-0 initialCc already emitted that value.
    const bytes = buildSmf({
        events: [
            { beats: 4, status: 0xB0, d1: 0x07, d2: 97 },
        ],
        tempo: 120,
        loop: { on: false, start: 0, length: 0 },
        timeSig: { num: 4, den: 4 },
        voicemap: new Map(),
        initialCcs: [
            { channel: 0, cc: 0x07, value: 97 },
        ],
    });
    const ccs = parseCcStream(bytes).filter((c) => c.cn === 0x07);
    // Only the tick-0 initial CC; captured one suppressed.
    assert.equal(ccs.length, 1);
    assert.equal(ccs[0].tick, 0);
    assert.equal(ccs[0].cv, 97);
});

test("CC dedup — captured CC with DIFFERENT value than initialCcs still emits", () => {
    const bytes = buildSmf({
        events: [
            { beats: 4, status: 0xB0, d1: 0x07, d2: 80 },
        ],
        tempo: 120,
        loop: { on: false, start: 0, length: 0 },
        timeSig: { num: 4, den: 4 },
        voicemap: new Map(),
        initialCcs: [
            { channel: 0, cc: 0x07, value: 97 },
        ],
    });
    const ccs = parseCcStream(bytes).filter((c) => c.cn === 0x07);
    assert.equal(ccs.length, 2);
    assert.equal(ccs[0].tick, 0);
    assert.equal(ccs[0].cv, 97);
    assert.equal(ccs[1].cv, 80);
});

test("CC dedup — XCMD pair across ticks NOT collapsed by consecutive-value", () => {
    // Two identical XCMD pairs at different ticks should BOTH survive — the
    // exemption applies to consecutive-value too.
    const events: MidiEvent[] = [
        { beats: 1, status: 0xB0, d1: 0x1E, d2: 0x02 },
        { beats: 1, status: 0xB0, d1: 0x1D, d2: 0x05 },
        { beats: 2, status: 0xB0, d1: 0x1E, d2: 0x02 },
        { beats: 2, status: 0xB0, d1: 0x1D, d2: 0x05 },
    ];
    const bytes = buildSmf({
        events,
        tempo: 120,
        loop: { on: false, start: 0, length: 0 },
        timeSig: { num: 4, den: 4 },
        voicemap: new Map(),
    });
    const ccs = parseCcStream(bytes);
    assert.equal(ccs.length, 4);
});
