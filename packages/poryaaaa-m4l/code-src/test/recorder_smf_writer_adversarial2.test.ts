// Second-pass adversarial tests for buildSmf.
//
// The previous adversarial run focused on absolute-tick correctness. It
// pinned ticks for every event class on channel 0 and concluded the tick
// math holds. This pass goes after the cases that ch-0 single-event tests
// can't see: cross-channel byte encoding, multi-event chains across event
// classes on the SAME channel, VLQ boundaries, out-of-order input,
// precision drift at scale, and same-tick interleaving with the lib's
// `wait: "T0"` path.
//
// Coverage summary at the top of the file so anyone re-running this can
// triage quickly:
//
//   midi-writer-js uses different channel conventions by event class:
//   NoteOn/NoteOff/CC constructors subtract 1 internally, while
//   ProgramChangeEvent and PitchBendEvent OR `fields.channel || 0` directly
//   into the status byte. recorder_smf_writer.ts therefore passes
//   `p.channel + 1` for NoteOn/NoteOff/CC and raw `p.channel` for PC/Bend.
//
//   The tests below in section "PC*/PB*" pin that split so channel 15 cannot
//   overflow into 0xD0 ChannelPressure or 0xF0 system status.

import test from "node:test";
import assert from "node:assert/strict";

import { buildSmf, PPQ, type MidiEvent } from "../recorder_smf_writer";
import {
    parseSmf, voiceEvents, byStatusNibble, metaEvents,
} from "./_smf_parser";

function build(events: MidiEvent[], opts: {
    voicemap?: Map<number, number>;
    loop?:     { on: boolean; start: number; length: number };
    timeSig?:  { num: number; den: number };
    tempo?:    number;
} = {}): Uint8Array {
    return buildSmf({
        events,
        tempo:    opts.tempo   ?? 120,
        loop:     opts.loop    ?? { on: false, start: 0, length: 0 },
        timeSig:  opts.timeSig ?? { num: 4, den: 4 },
        voicemap: opts.voicemap ?? new Map(),
    });
}

function tick(beats: number): number {
    return Math.round(beats * PPQ);
}

function eventTick(beats: number): number {
    const t = tick(beats);
    return t > 0 ? t + 1 : 0;
}

// ---------------------------------------------------------------------------
// PC0 / PC1 / PB0 / PB1 — the channel-encoding off-by-one bug
// These FAIL on current code. They are the load-bearing findings of this
// review. Do not delete until both midi-writer-js constructor calls in
// recorder_smf_writer.ts are corrected.
// ---------------------------------------------------------------------------

test("PC0: captured PC on input ch 0 emits status byte 0xC0 (channel nibble = 0)", () => {
    const bytes = build([{ beats: 0, status: 0xC0, d1: 5, d2: 0 }]);
    const parsed = parseSmf(bytes);
    const pcs = byStatusNibble(voiceEvents(parsed), 0xC0);
    assert.equal(pcs.length, 1);
    assert.equal(pcs[0].status & 0x0F, 0,
        `BUG: input ch 0 PC emits status 0x${pcs[0].status.toString(16).padStart(2,'0')}. ` +
        `ProgramChangeEvent does NOT subtract 1 from fields.channel, but writer passes p.channel+1.`);
    assert.equal(pcs[0].d1, 5);
});

test("PC1: captured PC on input ch 7 emits status byte 0xC7", () => {
    const bytes = build([{ beats: 0, status: 0xC7, d1: 5, d2: 0 }]);
    const parsed = parseSmf(bytes);
    const pcs = byStatusNibble(voiceEvents(parsed), 0xC0);
    assert.equal(pcs.length, 1);
    assert.equal(pcs[0].status & 0x0F, 7,
        `BUG: input ch 7 PC emits status 0x${pcs[0].status.toString(16)}. Expected 0xC7.`);
});

test("PC2: captured PC on input ch 15 emits status byte 0xCF (NOT 0xD0 ChannelPressure)", () => {
    // This is the worst case: 0xD0 has ONE data byte, not two, so the rest
    // of the MTrk gets mis-parsed by any standard SMF reader.
    const bytes = build([{ beats: 0, status: 0xCF, d1: 42, d2: 0 }]);
    const parsed = parseSmf(bytes);
    // We can't use byStatusNibble for 0xC0 here because the buggy byte is
    // 0xD0 — search by track instead.
    const voiceCh15 = voiceEvents(parsed).filter((e) => e.track !== 0);
    assert.equal(voiceCh15.length, 1, "exactly one voice event in the music track");
    assert.equal(voiceCh15[0].status & 0xF0, 0xC0,
        `BUG: PC on ch 15 emits status 0x${voiceCh15[0].status.toString(16)}, which is ` +
        `not even a ProgramChange. The whole MTrk byte stream is corrupted from this point.`);
    assert.equal(voiceCh15[0].status & 0x0F, 15);
    assert.equal(voiceCh15[0].d1, 42);
});

test("PC3: voicemap-injected PC on input ch 0 emits status byte 0xC0", () => {
    // Voicemap path goes through the same `case "pc"` emit arm — same bug,
    // hit at tick 0 via step 1 instead of via step 2.
    const bytes = build([], { voicemap: new Map([[0, 5]]) });
    const parsed = parseSmf(bytes);
    const pcs = byStatusNibble(voiceEvents(parsed), 0xC0);
    assert.equal(pcs.length, 1);
    assert.equal(pcs[0].status & 0x0F, 0,
        `BUG: voicemap PC on ch 0 emits status 0x${pcs[0].status.toString(16)}.`);
});

test("PC4: voicemap-injected PC on input ch 15 must not overflow into 0xD0", () => {
    const bytes = build([], { voicemap: new Map([[15, 5]]) });
    const parsed = parseSmf(bytes);
    const voice = voiceEvents(parsed).filter((e) => e.track !== 0);
    assert.equal(voice.length, 1);
    assert.equal(voice[0].status & 0xF0, 0xC0,
        `BUG: voicemap PC on ch 15 emits status 0x${voice[0].status.toString(16)}.`);
    assert.equal(voice[0].status & 0x0F, 15);
});

test("PC5: loop-replay PC on input ch 7 emits status byte 0xC7", () => {
    // Step 3 in buildSmf goes through the same `case "pc"` emit arm.
    const bytes = build(
        [{ beats: 1, status: 0xC7, d1: 5, d2: 0 }],
        { loop: { on: true, start: 4, length: 4 } },
    );
    const parsed = parseSmf(bytes);
    const pcs = byStatusNibble(voiceEvents(parsed), 0xC0);
    assert.equal(pcs.length, 2);
    for (const pc of pcs) {
        assert.equal(pc.status & 0x0F, 7,
            `BUG: PC channel nibble should be 7 but got ${pc.status & 0x0F}.`);
    }
});

test("PB0: captured pitch bend on input ch 0 emits status byte 0xE0", () => {
    const bytes = build([{ beats: 0, status: 0xE0, d1: 0, d2: 64 }]);
    const parsed = parseSmf(bytes);
    const bends = byStatusNibble(voiceEvents(parsed), 0xE0);
    assert.equal(bends.length, 1);
    assert.equal(bends[0].status & 0x0F, 0,
        `BUG: input ch 0 bend emits status 0x${bends[0].status.toString(16)}. ` +
        `PitchBendEvent does NOT subtract 1 from fields.channel.`);
});

test("PB1: captured pitch bend on input ch 7 emits status byte 0xE7", () => {
    const bytes = build([{ beats: 0, status: 0xE7, d1: 0, d2: 64 }]);
    const parsed = parseSmf(bytes);
    const bends = byStatusNibble(voiceEvents(parsed), 0xE0);
    assert.equal(bends.length, 1);
    assert.equal(bends[0].status & 0x0F, 7);
});

test("PB2: captured pitch bend on input ch 15 must not overflow into 0xF0 (system)", () => {
    const bytes = build([{ beats: 0, status: 0xEF, d1: 0, d2: 64 }]);
    const parsed = parseSmf(bytes);
    const voice = voiceEvents(parsed).filter((e) => e.track !== 0);
    assert.equal(voice.length, 1);
    assert.equal(voice[0].status & 0xF0, 0xE0,
        `BUG: bend on ch 15 emits status 0x${voice[0].status.toString(16)} ` +
        `(would be a system message, corrupts byte stream).`);
    assert.equal(voice[0].status & 0x0F, 15);
});

// ---------------------------------------------------------------------------
// Q: NoteOn after non-tick-advancing events on the same channel.
//
// midi-writer-js's Track.buildData advances tickPointer ONLY for
// NoteOn/NoteOff/Tempo (build/index.js:959-973). After a CC/PC/Bend the
// tickPointer stays at whatever the last note tick was — STALE.
//
// We compute deltas ourselves before calling addEvent, so the byte-stream
// deltas are absolute-tick-correct regardless of the lib's stale tickPointer.
// These tests pin that invariant.
// ---------------------------------------------------------------------------

test("Q1: NoteOn-CC-NoteOff on the same channel produces correct absolute ticks", () => {
    const bytes = build([
        { beats: 1, status: 0x90, d1: 60, d2: 100 },
        { beats: 2, status: 0xB0, d1: 7,  d2: 100 },
        { beats: 3, status: 0x80, d1: 60, d2: 0   },
    ]);
    const parsed = parseSmf(bytes);
    const ons  = byStatusNibble(voiceEvents(parsed), 0x90).filter((e) => e.d2 !== 0);
    const ccs  = byStatusNibble(voiceEvents(parsed), 0xB0);
    const offs = byStatusNibble(voiceEvents(parsed), 0x80);
    assert.equal(ons.length,  1);  assert.equal(ons[0].tick,  eventTick(1));
    assert.equal(ccs.length,  1);  assert.equal(ccs[0].tick,  eventTick(2));
    assert.equal(offs.length, 1);  assert.equal(offs[0].tick, eventTick(3));
});

test("Q2: NoteOn-CC-NoteOn-CC-NoteOff-NoteOff chain — every absolute tick correct", () => {
    const bytes = build([
        { beats: 1, status: 0x90, d1: 60, d2: 100 },
        { beats: 2, status: 0xB0, d1: 7,  d2: 50  },
        { beats: 3, status: 0x90, d1: 62, d2: 100 },
        { beats: 4, status: 0xB0, d1: 7,  d2: 80  },
        { beats: 5, status: 0x80, d1: 60, d2: 0   },
        { beats: 6, status: 0x80, d1: 62, d2: 0   },
    ]);
    const parsed = parseSmf(bytes);
    const ons  = byStatusNibble(voiceEvents(parsed), 0x90).filter((e) => e.d2 !== 0);
    const ccs  = byStatusNibble(voiceEvents(parsed), 0xB0);
    const offs = byStatusNibble(voiceEvents(parsed), 0x80);
    assert.deepEqual(ons.map((e) => e.tick),  [eventTick(1), eventTick(3)]);
    assert.deepEqual(ccs.map((e) => e.tick),  [eventTick(2), eventTick(4)]);
    assert.deepEqual(offs.map((e) => e.tick), [eventTick(5), eventTick(6)]);
});

test("Q3: NoteOn-5CC-NoteOn-5CC-NoteOff-NoteOff stress — tick accuracy under CC density", () => {
    const events: MidiEvent[] = [
        { beats: 1.0, status: 0x90, d1: 60, d2: 100 },
        { beats: 1.1, status: 0xB0, d1: 1, d2: 1 },
        { beats: 1.2, status: 0xB0, d1: 2, d2: 2 },
        { beats: 1.3, status: 0xB0, d1: 3, d2: 3 },
        { beats: 1.4, status: 0xB0, d1: 4, d2: 4 },
        { beats: 1.5, status: 0xB0, d1: 5, d2: 5 },
        { beats: 2.0, status: 0x90, d1: 62, d2: 100 },
        { beats: 3.0, status: 0x80, d1: 60, d2: 0 },
        { beats: 4.0, status: 0x80, d1: 62, d2: 0 },
    ];
    const bytes = build(events);
    const parsed = parseSmf(bytes);
    const ons  = byStatusNibble(voiceEvents(parsed), 0x90).filter((e) => e.d2 !== 0);
    const offs = byStatusNibble(voiceEvents(parsed), 0x80);
    assert.deepEqual(ons.map((e) => e.tick),  [eventTick(1), eventTick(2)]);
    assert.deepEqual(offs.map((e) => e.tick), [eventTick(3), eventTick(4)]);
    const ccTicks = byStatusNibble(voiceEvents(parsed), 0xB0).map((e) => e.tick);
    assert.deepEqual(ccTicks, [eventTick(1.1), eventTick(1.2), eventTick(1.3), eventTick(1.4), eventTick(1.5)]);
});

// ---------------------------------------------------------------------------
// R: same-tick interleaving — wait: "T0" path inside NoteOnEvent.
// NoteOnEvent.buildData parses "T0" → delta 0, so the byte stream emits
// delta 0 even after a CC at the same tick. Confirm.
// ---------------------------------------------------------------------------

test("R1: CC then NoteOn at same tick on same channel — both at the same tick", () => {
    const bytes = build([
        { beats: 1, status: 0xB0, d1: 7,  d2: 100 },
        { beats: 1, status: 0x90, d1: 60, d2: 100 },
    ]);
    const parsed = parseSmf(bytes);
    const ccs = byStatusNibble(voiceEvents(parsed), 0xB0);
    const ons = byStatusNibble(voiceEvents(parsed), 0x90).filter((e) => e.d2 !== 0);
    assert.equal(ccs[0].tick, eventTick(1));
    assert.equal(ons[0].tick, eventTick(1));
});

test("R2: three CCs then NoteOn at same tick — NoteOn still at correct tick", () => {
    const bytes = build([
        { beats: 1, status: 0xB0, d1: 1, d2: 1 },
        { beats: 1, status: 0xB0, d1: 2, d2: 2 },
        { beats: 1, status: 0xB0, d1: 3, d2: 3 },
        { beats: 1, status: 0x90, d1: 60, d2: 100 },
    ]);
    const parsed = parseSmf(bytes);
    const ons = byStatusNibble(voiceEvents(parsed), 0x90).filter((e) => e.d2 !== 0);
    assert.equal(ons.length, 1);
    assert.equal(ons[0].tick, eventTick(1));
});

test("R3: NoteOn-CC-NoteOn all at SAME tick same channel — both NoteOns share the same tick", () => {
    const bytes = build([
        { beats: 1, status: 0x90, d1: 60, d2: 100 },
        { beats: 1, status: 0xB0, d1: 7,  d2: 100 },
        { beats: 1, status: 0x90, d1: 64, d2: 100 },
        { beats: 2, status: 0x80, d1: 60, d2: 0 },
        { beats: 2, status: 0x80, d1: 64, d2: 0 },
    ]);
    const parsed = parseSmf(bytes);
    const ons  = byStatusNibble(voiceEvents(parsed), 0x90).filter((e) => e.d2 !== 0);
    const offs = byStatusNibble(voiceEvents(parsed), 0x80);
    assert.deepEqual(ons.map((e) => e.tick),  [eventTick(1), eventTick(1)]);
    assert.deepEqual(offs.map((e) => e.tick), [eventTick(2), eventTick(2)]);
    // pitches must come out in insertion order
    assert.deepEqual(ons.map((e) => e.d1), [60, 64]);
});

// ---------------------------------------------------------------------------
// V: VLQ boundary correctness.  numberToVariableLength encodes deltas as
// 1..4 byte varints; the boundaries are 127→128, 16383→16384, 2097151→2097152.
// ---------------------------------------------------------------------------

const VLQ_TICKS = [1, 127, 128, 16383, 16384, 2097151, 2097152];
for (const t of VLQ_TICKS) {
    test(`V: NoteOn at exact tick ${t} (VLQ boundary) round-trips`, () => {
        const beats = t / PPQ;
        const bytes = build([{ beats, status: 0x90, d1: 60, d2: 100 }]);
        const parsed = parseSmf(bytes);
        const ons = byStatusNibble(voiceEvents(parsed), 0x90).filter((e) => e.d2 !== 0);
        assert.equal(ons.length, 1);
        assert.equal(ons[0].tick, eventTick(beats), `expected compensated tick for raw ${t}, got ${ons[0].tick}`);
    });
}

// ---------------------------------------------------------------------------
// S: input events arrive in non-sorted tick order — the per-channel sort
// (recorder_smf_writer.ts:308) must reorder them.
// ---------------------------------------------------------------------------

test("S1: out-of-order NoteOn/NoteOff input is sorted by tick on emit", () => {
    const bytes = build([
        { beats: 3, status: 0x90, d1: 64, d2: 100 },
        { beats: 1, status: 0x90, d1: 60, d2: 100 },
        { beats: 4, status: 0x80, d1: 64, d2: 0 },
        { beats: 2, status: 0x80, d1: 60, d2: 0 },
    ]);
    const parsed = parseSmf(bytes);
    const ons  = byStatusNibble(voiceEvents(parsed), 0x90).filter((e) => e.d2 !== 0);
    const offs = byStatusNibble(voiceEvents(parsed), 0x80);
    // After sort, ticks must be monotonic on the stream:
    assert.deepEqual(ons.map((e) => e.d1),   [60, 64]);
    assert.deepEqual(ons.map((e) => e.tick), [eventTick(1), eventTick(3)]);
    assert.deepEqual(offs.map((e) => e.d1),   [60, 64]);
    assert.deepEqual(offs.map((e) => e.tick), [eventTick(2), eventTick(4)]);
});

// ---------------------------------------------------------------------------
// X: held-note flush vs loop-replay PC — sort interleaves correctly when
// the replay PC tick is < the flush tick (note far after loop).
// ---------------------------------------------------------------------------

test("X1: NoteOn far after loop end — replay PC at loop start, NoteOn later, flush NoteOff after", () => {
    const bytes = build(
        [
            { beats: 0.5, status: 0xC0, d1: 5, d2: 0 },
            { beats: 10,  status: 0x90, d1: 60, d2: 100 },
        ],
        { loop: { on: true, start: 1, length: 1 } },
    );
    const parsed = parseSmf(bytes);
    const pcs  = byStatusNibble(voiceEvents(parsed), 0xC0);
    const ons  = byStatusNibble(voiceEvents(parsed), 0x90).filter((e) => e.d2 !== 0);
    const offs = byStatusNibble(voiceEvents(parsed), 0x80);
    assert.deepEqual(pcs.map((e) => e.tick),  [0, eventTick(0.5)]);
    assert.deepEqual(ons.map((e) => e.tick),  [eventTick(10)]);
    assert.deepEqual(offs.map((e) => e.tick), [eventTick(10) + 1]);
});

test("X2: held-note flush + loop-replay PC same channel — overall tick order monotonic", () => {
    const bytes = build(
        [
            { beats: 0.5, status: 0xC0, d1: 5, d2: 0 },
            { beats: 10,  status: 0x90, d1: 60, d2: 100 },
        ],
        { loop: { on: true, start: 1, length: 1 } },
    );
    const parsed = parseSmf(bytes);
    // Pull every voice event on track 1 in stream order and check ticks ascend.
    const ch = voiceEvents(parsed).filter((e) => e.track === 1);
    for (let i = 1; i < ch.length; i++) {
        assert.ok(ch[i].tick >= ch[i - 1].tick,
            `track-1 tick monotonicity broken at index ${i}: ${ch[i - 1].tick} → ${ch[i].tick}`);
    }
});

// ---------------------------------------------------------------------------
// Y: captured CC at loop-start tick + replay PC at same tick (insertion-order
// tiebreak inside the per-channel sort).
// ---------------------------------------------------------------------------

test("Y1: captured CC at loop-start tick + replay PC at same tick — both present, both at right tick", () => {
    const bytes = build(
        [
            { beats: 1, status: 0xC0, d1: 7, d2: 0 },
            { beats: 4, status: 0xB0, d1: 7, d2: 50 },
        ],
        { loop: { on: true, start: 4, length: 4 } },
    );
    const parsed = parseSmf(bytes);
    const ccs = byStatusNibble(voiceEvents(parsed), 0xB0);
    const pcs = byStatusNibble(voiceEvents(parsed), 0xC0);
    assert.equal(ccs.length, 1); assert.equal(ccs[0].tick, eventTick(4));
    assert.equal(pcs.length, 2);
    const pcTicks = pcs.map((e) => e.tick).sort((a, b) => a - b);
    assert.deepEqual(pcTicks, [0, eventTick(1)], "replay PC at exported-buffer start; captured PC preserved");
});

// ---------------------------------------------------------------------------
// Z: precision-drift stress — 1000 non-grid CCs.  Each tick should match
// Math.round(beats * PPQ) independently.
// ---------------------------------------------------------------------------

test("Z1: 1000 non-grid CCs land at independently-rounded ticks (no cumulative drift)", () => {
    const events: MidiEvent[] = [];
    const expected: number[] = [];
    for (let k = 1; k <= 1000; k++) {
        const beats = k * 0.137;     // arbitrary irrational-ish stride
        events.push({ beats, status: 0xB0, d1: 1, d2: k % 128 });
        expected.push(eventTick(beats));
    }
    const bytes = build(events);
    const parsed = parseSmf(bytes);
    const got = byStatusNibble(voiceEvents(parsed), 0xB0).map((e) => e.tick);
    assert.deepEqual(got, expected,
        "any mismatch indicates tick drift — should be zero with " +
        "round-then-delta semantics");
});

test("Z2: 200 non-grid NoteOn/NoteOff pairs land at independently-rounded ticks", () => {
    const events: MidiEvent[] = [];
    const expectedOn: number[] = [];
    const expectedOff: number[] = [];
    for (let k = 1; k <= 200; k++) {
        const onBeats  = k * 1.137;
        const offBeats = onBeats + 0.5;
        events.push({ beats: onBeats,  status: 0x90, d1: 60, d2: 100 });
        events.push({ beats: offBeats, status: 0x80, d1: 60, d2: 0 });
        expectedOn.push(eventTick(onBeats));
        expectedOff.push(eventTick(offBeats));
    }
    const bytes = build(events);
    const parsed = parseSmf(bytes);
    const gotOn  = byStatusNibble(voiceEvents(parsed), 0x90).filter((e) => e.d2 !== 0).map((e) => e.tick);
    const gotOff = byStatusNibble(voiceEvents(parsed), 0x80).map((e) => e.tick);
    assert.deepEqual(gotOn,  expectedOn);
    assert.deepEqual(gotOff, expectedOff);
});

test("Z3: large captured CC stream builds without writer stack overflow", () => {
    const events: MidiEvent[] = [
        { beats: 0, status: 0xC0, d1: 5,    d2: 0   },
        { beats: 0, status: 0xB0, d1: 0x07, d2: 100 },
        { beats: 0, status: 0xB0, d1: 0x0A, d2: 64  },
        { beats: 1, status: 0x90, d1: 60,   d2: 100 },
    ];
    for (let i = 0; i < 70_000; i++) {
        events.push({ beats: 2 + i, status: 0xB0, d1: 1, d2: i & 0x7F });
    }
    const parsed = parseSmf(build(events, { loop: { on: false, start: 0, length: 0 } }));
    const ccs = byStatusNibble(voiceEvents(parsed), 0xB0).filter((e) => e.d1 === 1);
    assert.equal(ccs.length, 70_000);
});

// ---------------------------------------------------------------------------
// CH: 16-channel stress — every channel gets its own track and no event leaks.
// ---------------------------------------------------------------------------

test("CH1: 16-channel mixed events — each channel has exactly 1 NoteOn, 1 CC, 1 NoteOff at expected ticks", () => {
    const events: MidiEvent[] = [];
    for (let ch = 0; ch < 16; ch++) {
        events.push({ beats: ch * 0.1 + 0.5, status: 0x90 | ch, d1: 60 + ch, d2: 100 });
        events.push({ beats: ch * 0.1 + 1.0, status: 0xB0 | ch, d1: 7, d2: ch });
        events.push({ beats: ch * 0.1 + 1.5, status: 0x80 | ch, d1: 60 + ch, d2: 0 });
    }
    const bytes = build(events);
    const parsed = parseSmf(bytes);
    assert.equal(parsed.header.tracks, 17, "conductor + 16 music tracks");

    // Per channel, walk the events and verify each one.
    for (let ch = 0; ch < 16; ch++) {
        const own = voiceEvents(parsed).filter((e) => (e.status & 0x0F) === ch);
        // NoteOn/NoteOff have correct channel encoding (lib subtracts 1). CC also.
        // PC/Bend would not, but we don't use those here.
        const ons  = own.filter((e) => (e.status & 0xF0) === 0x90 && e.d2 !== 0);
        const ccs  = own.filter((e) => (e.status & 0xF0) === 0xB0);
        const offs = own.filter((e) => (e.status & 0xF0) === 0x80);
        assert.equal(ons.length,  1, `ch${ch} ons`);
        assert.equal(ccs.length,  1, `ch${ch} ccs`);
        assert.equal(offs.length, 1, `ch${ch} offs`);
        const onTick  = eventTick(ch * 0.1 + 0.5);
        const ccTick  = eventTick(ch * 0.1 + 1.0);
        const offTick = eventTick(ch * 0.1 + 1.5);
        assert.equal(ons[0].tick,  onTick,  `ch${ch} on tick`);
        assert.equal(ccs[0].tick,  ccTick,  `ch${ch} cc tick`);
        assert.equal(offs[0].tick, offTick, `ch${ch} off tick`);
    }
});

// ---------------------------------------------------------------------------
// CT: conductor track invariants.
// ---------------------------------------------------------------------------

test("CT1: conductor track (track 0) has zero channel-voice events", () => {
    const bytes = build(
        [
            { beats: 0, status: 0x90, d1: 60, d2: 100 },
            { beats: 1, status: 0x80, d1: 60, d2: 0 },
        ],
        { loop: { on: true, start: 0, length: 4 }, voicemap: new Map([[0, 5]]) },
    );
    const parsed = parseSmf(bytes);
    const t0 = voiceEvents(parsed).filter((e) => e.track === 0);
    assert.equal(t0.length, 0, "conductor must hold only meta events");
});

test("CT2: tempo + time-sig + both loop markers all appear in conductor (track 0)", () => {
    const bytes = build([], { loop: { on: true, start: 4, length: 4 } });
    const parsed = parseSmf(bytes);
    const t0meta = metaEvents(parsed).filter((e) => e.track === 0);
    assert.ok(t0meta.find((e) => e.metaType === 0x51), "tempo");
    assert.ok(t0meta.find((e) => e.metaType === 0x58), "time-sig");
    const markers = t0meta.filter((e) => e.metaType === 0x06);
    assert.equal(markers.length, 2);
});

// ---------------------------------------------------------------------------
// L: loop edge cases.
// ---------------------------------------------------------------------------

test("L1: loop length 0 — two markers at same tick", () => {
    const bytes = build([], { loop: { on: true, start: 4, length: 0 } });
    const parsed = parseSmf(bytes);
    const markers = metaEvents(parsed).filter((e) => e.metaType === 0x06);
    assert.equal(markers.length, 2);
    assert.equal(markers[0].tick, tick(4));
    assert.equal(markers[1].tick, tick(4));
});

test("L2: loop start beyond last event — replay PC lands at exported-buffer start, captured PC unaffected", () => {
    const bytes = build(
        [{ beats: 2, status: 0xC0, d1: 5, d2: 0 }],
        { loop: { on: true, start: 1000, length: 4 } },
    );
    const parsed = parseSmf(bytes);
    const pcs = byStatusNibble(voiceEvents(parsed), 0xC0);
    assert.equal(pcs.length, 2);
    const ticks = pcs.map((e) => e.tick).sort((a, b) => a - b);
    assert.deepEqual(ticks, [0, eventTick(2)], "replay PC at exported-buffer start; captured PC preserved");
});

test("L3: voicemap-only + loop on — each voicemap channel gets tick-0 PC AND loop-start PC", () => {
    const bytes = build([], {
        loop: { on: true, start: 4, length: 4 },
        voicemap: new Map([[0, 5], [3, 42]]),
    });
    const parsed = parseSmf(bytes);
    // Group PCs by channel-nibble. Note: due to the PC channel-encoding bug,
    // these will currently come out one nibble higher than they should — but
    // we still expect TWO PCs per voicemap channel (one at tick 0, one at
    // loop-start tick).
    const pcs = byStatusNibble(voiceEvents(parsed), 0xC0);
    assert.equal(pcs.length, 4, "two voicemap channels × two PCs each");
    const ticks = pcs.map((e) => e.tick).sort((a, b) => a - b);
    assert.deepEqual(ticks, [0, 0, tick(4), tick(4)]);
});

// ---------------------------------------------------------------------------
// HF: held-note flush specifics.
// ---------------------------------------------------------------------------

test("HF1: NoteOn at beats=2 with loop start=4, length=4 — flush follows the note-on", () => {
    const bytes = build(
        [{ beats: 2, status: 0x90, d1: 60, d2: 100 }],
        { loop: { on: true, start: 4, length: 4 } },
    );
    const parsed = parseSmf(bytes);
    const ons  = byStatusNibble(voiceEvents(parsed), 0x90).filter((e) => e.d2 !== 0);
    const offs = byStatusNibble(voiceEvents(parsed), 0x80);
    assert.equal(ons.length, 1);  assert.equal(ons[0].tick, eventTick(2));
    assert.equal(offs.length, 1); assert.equal(offs[0].tick, eventTick(2) + 1);
});

test("HF2: two NoteOns on same pitch same channel → first closes before re-attack", () => {
    const bytes = build([
        { beats: 0, status: 0x90, d1: 60, d2: 100 },
        { beats: 1, status: 0x90, d1: 60, d2: 100 },
    ]);
    const parsed = parseSmf(bytes);
    const offs = byStatusNibble(voiceEvents(parsed), 0x80);
    assert.equal(offs.length, 2, "re-attack close plus final auto note-off");
    assert.equal(offs[0].tick, eventTick(1));
});

// ---------------------------------------------------------------------------
// Categories with no concern — documented for the next reviewer.
//
// - Suspicion #14 (PendingDescriptor.kind exhaustiveness):
//     The switch is a TS-discriminated union with no default; adding a kind
//     fails at compile time. Runtime check unnecessary.
//
// - Suspicion #18 (precisionLoss accumulation):
//     Z1/Z2 above verify zero drift across 1000-event and 200-pair runs.
//     Bounded because every delta we feed the library is a non-negative
//     integer, so getPrecisionLoss returns 0 every step.
//
// - Suspicion #20 (PRBY round-trip):
//     Covered by recorder_prby_format.test.ts. Independent of the SMF
//     writer fix path.
//
// - Suspicion #16 (negative beats):
//     The probe confirmed Math.round of a small negative fraction lands at 0 and the
//     max(0, currTick - prevTick) clamp at recorder_smf_writer.ts:313
//     guards against negative deltas. Real captures never produce negative
//     beats (poryaaaa~ samples on transport start), so no dedicated test.
// ---------------------------------------------------------------------------
