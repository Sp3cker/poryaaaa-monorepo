// Adversarial tick-position tests for buildSmf.
//
// The existing recorder_smf_writer.test.ts checks event COUNTS and PROGRAM
// VALUES but never that any event lands at the expected ABSOLUTE TICK. With
// buildSmf computes absolute integer ticks with Math.round before converting
// to deltas for midi-writer-js. These tests pin exact tick positions and catch
// regressions where fractional deltas could introduce cumulative drift.

import test from "node:test";
import assert from "node:assert/strict";

import { buildSmf, PPQ, type MidiEvent } from "../recorder/recorder_smf_writer";
import {
    parseSmf, voiceEvents, byStatusNibble, metaEvents,
} from "./_smf_parser";

function build(events: MidiEvent[], opts: {
    voicemap?: Map<number, number>;
    loop?:     { on: boolean; start: number; length: number };
    loopMarkers?: { startBeats: number; endBeats: number };
    timeSig?:  { num: number; den: number };
    tempo?:    number;
    range?:    { startBeats: number; endBeats: number };
} = {}): Uint8Array {
    const input = {
        events,
        tempo:    opts.tempo   ?? 120,
        loop:     opts.loop    ?? { on: false, start: 0, length: 0 },
        timeSig:  opts.timeSig ?? { num: 4, den: 4 },
        voicemap: opts.voicemap ?? new Map(),
        range:    opts.range,
    };
    return buildSmf(opts.loopMarkers === undefined
        ? input
        : { ...input, loopMarkers: opts.loopMarkers });
}

function tick(beats: number): number {
    return Math.round(beats * PPQ);
}

function eventTick(beats: number): number {
    const t = tick(beats);
    return t > 0 ? t + 1 : 0;
}

function strictNoteBalance(bytes: Uint8Array): { dangling: number; offWithoutOn: number } {
    const open = new Map<string, number>();
    let offWithoutOn = 0;
    for (const e of voiceEvents(parseSmf(bytes))) {
        const type = e.status & 0xF0;
        const key = `${e.status & 0x0F}:${e.d1}`;
        if (type === 0x90 && e.d2 > 0) {
            open.set(key, (open.get(key) ?? 0) + 1);
        } else if (type === 0x80 || (type === 0x90 && e.d2 === 0)) {
            const n = open.get(key) ?? 0;
            if (n <= 0) {
                offWithoutOn++;
            } else if (n === 1) {
                open.delete(key);
            } else {
                open.set(key, n - 1);
            }
        }
    }
    let dangling = 0;
    for (const n of open.values()) dangling += n;
    return { dangling, offWithoutOn };
}

test("SMF header division matches the writer PPQ", () => {
    const parsed = parseSmf(build([]));
    assert.equal(parsed.header.division, PPQ);
});

// ---------------------------------------------------------------------------
// A: tick positions of every event class
// ---------------------------------------------------------------------------

test("A1: note-on at beats=2.0 lands at the expected absolute tick", () => {
    const bytes = build([{ beats: 2.0, status: 0x90, d1: 60, d2: 100 }]);
    const parsed = parseSmf(bytes);
    const ons = byStatusNibble(voiceEvents(parsed), 0x90).filter((e) => e.d2 !== 0);
    assert.equal(ons.length, 1);
    assert.equal(ons[0].tick, eventTick(2.0));
});

test("A2: CC at beats=0.5 lands at the expected absolute tick", () => {
    const bytes = build([{ beats: 0.5, status: 0xB0, d1: 7, d2: 100 }]);
    const parsed = parseSmf(bytes);
    const ccs = byStatusNibble(voiceEvents(parsed), 0xB0);
    assert.equal(ccs.length, 1);
    assert.equal(ccs[0].tick, eventTick(0.5));
    assert.equal(ccs[0].d1, 7);
    assert.equal(ccs[0].d2, 100);
});

test("A3: PC at beats=1.5 lands at the expected absolute tick", () => {
    const bytes = build([{ beats: 1.5, status: 0xC0, d1: 5, d2: 0 }]);
    const parsed = parseSmf(bytes);
    const pcs = byStatusNibble(voiceEvents(parsed), 0xC0);
    assert.equal(pcs.length, 1);
    assert.equal(pcs[0].tick, eventTick(1.5));
    assert.equal(pcs[0].d1, 5);
});

test("A4: pitch bend at beats=3.0 lands at the expected absolute tick and round-trips", () => {
    // Value 14 = (d2 << 7) | d1; pick d1=0, d2=64 → 14-bit 0x2000 = 8192 (center).
    // Then pick d1=0, d2=96 → 14-bit 0x3000 = 12288. Normalised = (12288-8192)/8192 = 0.5.
    // midi-writer-js stores bend in the byte pair lsbValue, msbValue.
    const bytes = build([{ beats: 3.0, status: 0xE0, d1: 0, d2: 96 }]);
    const parsed = parseSmf(bytes);
    const bends = byStatusNibble(voiceEvents(parsed), 0xE0);
    assert.equal(bends.length, 1);
    assert.equal(bends[0].tick, eventTick(3.0));
    // Reconstruct value14 from emitted (d1=lsb, d2=msb).
    const value14 = (bends[0].d2 << 7) | bends[0].d1;
    // Allow ±1 LSB tolerance — midi-writer-js scales via Math.round((bend+1)*8192)
    // which can re-quantise.
    const diff = Math.abs(value14 - 12288);
    assert.ok(diff <= 1, `value14 ${value14} within ±1 of 12288`);
});

test("A5: two CCs at same beat (1.0) on the same channel land at the same tick in insertion order", () => {
    const bytes = build([
        { beats: 1.0, status: 0xB0, d1: 0x1E, d2: 0x42 },
        { beats: 1.0, status: 0xB0, d1: 0x1D, d2: 0x55 },
    ]);
    const parsed = parseSmf(bytes);
    const ccs = byStatusNibble(voiceEvents(parsed), 0xB0);
    assert.equal(ccs.length, 2);
    assert.equal(ccs[0].tick, eventTick(1.0));
    assert.equal(ccs[1].tick, eventTick(1.0));
    assert.equal(ccs[0].d1, 0x1E, "first CC = 0x1E (insertion order preserved)");
    assert.equal(ccs[1].d1, 0x1D, "second CC = 0x1D (the GBA invariant)");
});

test("A6: non-grid beats use independently-rounded absolute ticks", () => {
    const beats = [0.1234, 0.5678, 0.9012];
    const expected = beats.map(eventTick);
    const bytes = build([
        { beats: beats[0], status: 0x90, d1: 60, d2: 100 },
        { beats: beats[1], status: 0x90, d1: 61, d2: 100 },
        { beats: beats[2], status: 0x90, d1: 62, d2: 100 },
    ]);
    const parsed = parseSmf(bytes);
    const ons = byStatusNibble(voiceEvents(parsed), 0x90).filter((e) => e.d2 !== 0);
    assert.equal(ons.length, 3);
    // The writer must round absolute ticks first, then compute deltas.
    const ticks = ons.map((e) => e.tick);
    assert.deepEqual(
        ticks,
        expected,
        `EXPECTED ${JSON.stringify(expected)} (independently rounded); GOT ${JSON.stringify(ticks)}`,
    );
});

test("A7: non-grid events do not accumulate tick drift", () => {
    // Each beat is off-grid; every absolute tick must be rounded independently
    // rather than accumulating rounded deltas.
    const beats: number[] = [];
    for (let k = 1; k <= 10; k++) beats.push(k * 0.099);

    const events = beats.map((b, idx): MidiEvent => ({
        beats: b, status: 0x90, d1: 60 + idx, d2: 100,
    }));
    const bytes = build(events);
    const parsed = parseSmf(bytes);
    const ons = byStatusNibble(voiceEvents(parsed), 0x90).filter((e) => e.d2 !== 0);
    const indep = beats.map(eventTick);
    const got   = ons.map((e) => e.tick);
    // We assert against independently-rounded ticks. Any drift causes failure.
    assert.deepEqual(
        got, indep,
        `independently-rounded: ${JSON.stringify(indep)}; got: ${JSON.stringify(got)}`,
    );
});

test("A8: near-beat scheduler values snap to the 96 PPQ beat boundary", () => {
    const almostBar7Beat2 = 24.996; // 4/4: bar 7 beat 2 is absolute beat 25.
    const bytes = build([{ beats: almostBar7Beat2, status: 0x90, d1: 60, d2: 100 }]);
    const parsed = parseSmf(bytes);
    const ons = byStatusNibble(voiceEvents(parsed), 0x90).filter((e) => e.d2 !== 0);
    assert.equal(ons.length, 1);
    assert.equal(ons[0].tick, eventTick(25));
});

// ---------------------------------------------------------------------------
// B: loop marker tick positions (meta events, not channel-voice)
// ---------------------------------------------------------------------------

test("B1: loop {start: 4, length: 4} places markers at expected ticks", () => {
    const bytes = build([], { loop: { on: true, start: 4, length: 4 } });
    const parsed = parseSmf(bytes);
    const markers = metaEvents(parsed).filter((e) => e.metaType === 0x06);
    assert.equal(markers.length, 2, "two markers");
    const open  = markers.find((m) => String.fromCharCode(m.data[0]) === "[");
    const close = markers.find((m) => String.fromCharCode(m.data[0]) === "]");
    assert.ok(open && close, "both markers present");
    assert.equal(open!.tick,  tick(4));
    assert.equal(close!.tick, tick(8));
});

test("B2: loop {start: 0.5, length: 1.5} places markers at expected ticks", () => {
    const bytes = build([], { loop: { on: true, start: 0.5, length: 1.5 } });
    const parsed = parseSmf(bytes);
    const markers = metaEvents(parsed).filter((e) => e.metaType === 0x06);
    const open  = markers.find((m) => String.fromCharCode(m.data[0]) === "[");
    const close = markers.find((m) => String.fromCharCode(m.data[0]) === "]");
    assert.ok(open && close);
    assert.equal(open!.tick,  tick(0.5));
    assert.equal(close!.tick, tick(2.0));
});

test("B3: loop off → no marker events", () => {
    const bytes = build([], { loop: { on: false, start: 4, length: 4 } });
    const parsed = parseSmf(bytes);
    const markers = metaEvents(parsed).filter((e) => e.metaType === 0x06);
    assert.equal(markers.length, 0);
});

test("B4: finite export range places markers at range bounds, not Live loop bounds", () => {
    const bytes = build([], {
        loop:  { on: true, start: 10, length: 4 },
        range: { startBeats: 56, endBeats: 72 },
    });
    const parsed = parseSmf(bytes);
    const markers = metaEvents(parsed).filter((e) => e.metaType === 0x06);
    assert.equal(markers.length, 2, "two markers");
    const open  = markers.find((m) => String.fromCharCode(m.data[0]) === "[");
    const close = markers.find((m) => String.fromCharCode(m.data[0]) === "]");
    assert.ok(open && close, "both markers present");
    assert.equal(open!.tick,  0);
    assert.equal(close!.tick, tick(16));
});

test("B5: no explicit range plus captured events places loop markers around the exported buffer", () => {
    const bytes = build(
        [{ beats: 2, status: 0x90, d1: 60, d2: 100 }],
        { loop: { on: true, start: 16, length: 16 } },
    );
    const parsed = parseSmf(bytes);
    const markers = metaEvents(parsed).filter((e) => e.metaType === 0x06);
    const open  = markers.find((m) => String.fromCharCode(m.data[0]) === "[");
    const close = markers.find((m) => String.fromCharCode(m.data[0]) === "]");
    assert.ok(open && close, "both markers present");
    assert.equal(open!.tick, 0);
    assert.equal(close!.tick, tick(16));
});

// ---------------------------------------------------------------------------
// C: PC-replay-at-loop-start tick position
// ---------------------------------------------------------------------------

test("C1: no-range PC replay lands at exported-buffer start", () => {
    const bytes = build(
        [{ beats: 2, status: 0xC0, d1: 7, d2: 0 }],
        { loop: { on: true, start: 8, length: 4 } },
    );
    const parsed = parseSmf(bytes);
    const pcs = byStatusNibble(voiceEvents(parsed), 0xC0).sort((a, b) => a.tick - b.tick);
    assert.equal(pcs.length, 2);
    assert.equal(pcs[0].tick, 0, "replayed PC at exported-buffer start");
    assert.equal(pcs[1].tick, eventTick(2), "captured PC is preserved at its captured tick");
    for (const p of pcs) assert.equal(p.d1, 7, "program preserved");
});

test("C2: loop-start PC replay uses the latest PC before the loop marker", () => {
    const bytes = build(
        [
            { beats: 0,  status: 0xC4, d1: 11, d2: 0 },
            { beats: 28, status: 0xC4, d1: 12, d2: 0 },
            { beats: 93, status: 0xC4, d1: 10, d2: 0 },
        ],
        { loopMarkers: { startBeats: 32, endBeats: 144 } },
    );
    const parsed = parseSmf(bytes);
    const pcs = byStatusNibble(voiceEvents(parsed), 0xC0)
        .filter((e) => (e.status & 0x0F) === 4)
        .sort((a, b) => a.tick - b.tick);

    const replay = pcs.find((e) => e.tick === tick(32));
    assert.ok(replay, "PC replay at loop start");
    assert.equal(replay!.d1, 12, "replay uses the PC active at loop start");
});

test("C3: loop-start CC replay uses latest Volume and Pan before the loop marker", () => {
    const bytes = build(
        [
            { beats: 0,  status: 0xB4, d1: 7,  d2: 40 },
            { beats: 0,  status: 0xB4, d1: 10, d2: 50 },
            { beats: 28, status: 0xB4, d1: 7,  d2: 41 },
            { beats: 29, status: 0xB4, d1: 10, d2: 51 },
            { beats: 93, status: 0xB4, d1: 7,  d2: 42 },
            { beats: 94, status: 0xB4, d1: 10, d2: 52 },
        ],
        { loopMarkers: { startBeats: 32, endBeats: 144 } },
    );
    const parsed = parseSmf(bytes);
    const replayCcs = byStatusNibble(voiceEvents(parsed), 0xB0)
        .filter((e) => (e.status & 0x0F) === 4 && e.tick === tick(32));

    assert.deepEqual(
        replayCcs.map((e) => [e.d1, e.d2]).sort((a, b) => a[0] - b[0]),
        [[7, 41], [10, 51]],
    );
});

// ---------------------------------------------------------------------------
// D: held-note flush tick position
// ---------------------------------------------------------------------------

test("D1: single note-on at beats=0 → auto note-off at tick 1 (lastEventTick+1)", () => {
    const bytes = build([{ beats: 0, status: 0x90, d1: 64, d2: 100 }]);
    const parsed = parseSmf(bytes);
    const offs = byStatusNibble(voiceEvents(parsed), 0x80);
    assert.equal(offs.length, 1);
    assert.equal(offs[0].tick, 1, "flush at lastEventTick (0) + 1 = 1");
    assert.equal(offs[0].d1, 64);
});

test("D2: note-on at beats=3 → auto note-off one tick after the note-on", () => {
    const bytes = build([{ beats: 3, status: 0x90, d1: 60, d2: 100 }]);
    const parsed = parseSmf(bytes);
    const offs = byStatusNibble(voiceEvents(parsed), 0x80);
    assert.equal(offs.length, 1);
    assert.equal(offs[0].tick, eventTick(3) + 1);
});

// ---------------------------------------------------------------------------
// E: time-signature denominator byte
// ---------------------------------------------------------------------------

function findTimeSigMeta(bytes: ArrayLike<number>): { num: number; denLog2: number; clk: number; n32: number } | null {
    const parsed = parseSmf(bytes);
    const ts = metaEvents(parsed).find((e) => e.metaType === 0x58);
    if (!ts) return null;
    return {
        num:     ts.data[0],
        denLog2: ts.data[1],
        clk:     ts.data[2],
        n32:     ts.data[3],
    };
}

test("E1: 4/4 → denominator byte == 2 (log2(4))", () => {
    const ts = findTimeSigMeta(build([], { timeSig: { num: 4, den: 4 } }));
    assert.ok(ts);
    assert.equal(ts!.num, 4);
    assert.equal(ts!.denLog2, 2);
});

test("E2: 6/8 → numerator 6, denominator byte == 3 (log2(8))", () => {
    const ts = findTimeSigMeta(build([], { timeSig: { num: 6, den: 8 } }));
    assert.ok(ts);
    assert.equal(ts!.num, 6);
    assert.equal(ts!.denLog2, 3);
});

test("E3: 3/4 → numerator 3, denominator byte == 2", () => {
    const ts = findTimeSigMeta(build([], { timeSig: { num: 3, den: 4 } }));
    assert.ok(ts);
    assert.equal(ts!.num, 3);
    assert.equal(ts!.denLog2, 2);
});

test("E4: 7/16 → numerator 7, denominator byte == 4 (log2(16))", () => {
    const ts = findTimeSigMeta(build([], { timeSig: { num: 7, den: 16 } }));
    assert.ok(ts);
    assert.equal(ts!.num, 7);
    assert.equal(ts!.denLog2, 4);
});

test("E5: time-sig MIDI-clocks-per-tick = 24 and 32nds-per-quarter = 8 (from writer call)", () => {
    const ts = findTimeSigMeta(build([], { timeSig: { num: 4, den: 4 } }));
    assert.ok(ts);
    assert.equal(ts!.clk, 24);
    assert.equal(ts!.n32, 8);
});

// ---------------------------------------------------------------------------
// K: edge cases on the buildSmf surface
// ---------------------------------------------------------------------------

test("K1: empty events + empty voicemap + loop off → valid SMF with conductor only, no music tracks", () => {
    const bytes = build([]);
    const parsed = parseSmf(bytes);
    // Should have header + conductor track only. No voice events.
    assert.equal(voiceEvents(parsed).length, 0);
    // Tempo and time-sig meta should still appear in track 0.
    const tempo = metaEvents(parsed).find((e) => e.metaType === 0x51);
    const ts    = metaEvents(parsed).find((e) => e.metaType === 0x58);
    assert.ok(tempo, "tempo meta present");
    assert.ok(ts,    "time-sig meta present");
});

test("K2: running-status note-off (0x9X with d2=0) emits SMF status nibble 0x80 (a real NoteOffEvent)", () => {
    const bytes = build([
        { beats: 0,   status: 0x90, d1: 60, d2: 100 },
        { beats: 0.5, status: 0x90, d1: 60, d2: 0   },  // running-status off
    ]);
    const parsed = parseSmf(bytes);
    const offs = byStatusNibble(voiceEvents(parsed), 0x80);
    assert.equal(offs.length, 1, "exactly one note-off (no held-note flush)");
    assert.equal(offs[0].d1, 60);
});

test("K2b: unmatched note-offs from mid-phrase capture are dropped", () => {
    const bytes = build([
        { beats: 0, status: 0x80, d1: 60, d2: 0 },
        { beats: 1, status: 0x90, d1: 61, d2: 100 },
        { beats: 2, status: 0x80, d1: 61, d2: 0 },
    ]);
    assert.deepEqual(strictNoteBalance(bytes), { dangling: 0, offWithoutOn: 0 });
});

test("K3: two note-ons same channel/pitch without off → first is closed before re-attack", () => {
    const bytes = build([
        { beats: 0,   status: 0x90, d1: 60, d2: 100 },
        { beats: 1.0, status: 0x90, d1: 60, d2: 100 },
    ]);
    const parsed = parseSmf(bytes);
    const ons  = byStatusNibble(voiceEvents(parsed), 0x90).filter((e) => e.d2 !== 0);
    const offs = byStatusNibble(voiceEvents(parsed), 0x80);
    assert.equal(ons.length,  2, "both note-ons captured");
    assert.equal(offs.length, 2, "one re-attack off plus one final auto-off");
    assert.equal(offs[0].tick, eventTick(1.0), "first note closes at re-attack tick");
    assert.deepEqual(strictNoteBalance(bytes), { dangling: 0, offWithoutOn: 0 });
});

test("K4: channel 15 events appear in the SMF with channel nibble == 0x0F", () => {
    const bytes = build([
        { beats: 0,   status: 0x9F, d1: 60, d2: 100 },   // ch 15 on
        { beats: 1.0, status: 0x8F, d1: 60, d2: 0   },
    ]);
    const parsed = parseSmf(bytes);
    const ons = byStatusNibble(voiceEvents(parsed), 0x90).filter((e) => e.d2 !== 0);
    assert.equal(ons.length, 1);
    assert.equal(ons[0].status & 0x0F, 0x0F, "channel nibble = 15");
});

test("K5: each used channel gets its own MTrk (header.tracks count matches)", () => {
    const bytes = build([
        { beats: 0,   status: 0x90, d1: 60, d2: 100 },   // ch 0
        { beats: 0,   status: 0x91, d1: 60, d2: 100 },   // ch 1
        { beats: 0,   status: 0x95, d1: 60, d2: 100 },   // ch 5
    ]);
    const parsed = parseSmf(bytes);
    assert.equal(parsed.header.tracks, 4, "1 conductor + 3 channels");
});

// ---------------------------------------------------------------------------
// J: buildSmf return type
// ---------------------------------------------------------------------------

test("J1: buildSmf return value is not a real Array", () => {
    const bytes = build([{ beats: 0, status: 0x90, d1: 60, d2: 100 }]);
    assert.equal(
        Array.isArray(bytes),
        false,
        "buildSmf returns a Uint8Array, not a plain number[]",
    );
});

test("J1b: buildSmf returns Uint8Array", () => {
    const bytes = build([{ beats: 0, status: 0x90, d1: 60, d2: 100 }]);
    assert.equal(
        bytes instanceof Uint8Array,
        true,
        "buildSmf returns a Uint8Array",
    );
});
