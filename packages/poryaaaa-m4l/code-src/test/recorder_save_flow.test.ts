// G: end-to-end Save orchestrator tests.
// H: dump-handshake tests.
//
// Before this file, createSmfWriter.save() had ZERO direct test coverage —
// the existing ccomidi_recorder.test.ts stubbed save() with a counter.
//
// These tests wire all of createSmfWriter's dependencies as in-memory mocks
// and assert that:
//   - the orchestrator passes buildSmf's output (not garbage) to writeSmf
//   - unlink ALWAYS runs (success, failure, throw, parse error, LiveAPI throw)
//   - empty filename short-circuits before any dump request
//   - the dump handshake's pendingDump guard works
//   - dumped() with a path mismatch rejects
//   - dumped() with no pending dump warns but does not throw

import test from "node:test";
import assert from "node:assert/strict";

import {
    createSmfWriter,
    buildSmf,
    PPQ,
    validateSmfInitialChannelState,
    type InitialCc,
    type LiveApiAdapter,
    type MidiEvent,
} from "../recorder_smf_writer";
import {
    createDumpHandshake,
} from "../ccomidi_recorder";
import { metaEvents, parseSmf, voiceEvents } from "./_smf_parser";

// ---- helpers ---------------------------------------------------------------

interface SavedWrite {
    path:  string;
    bytes: number[];
}

function makeLiveApi(opts: Partial<LiveApiAdapter> & {
    tempo?:   number;
    loop?:    { on: boolean; start: number; length: number };
    timeSig?: { num: number; den: number };
} = {}): LiveApiAdapter {
    return {
        getTempo:    opts.getTempo    ?? (() => opts.tempo   ?? 120),
        getLoop:     opts.getLoop     ?? (() => opts.loop    ?? { on: false, start: 0, length: 0 }),
        getTimeSig:  opts.getTimeSig  ?? (() => opts.timeSig ?? { num: 4, den: 4 }),
    };
}

function requiredState(channel = 0, program = 5): { voicemap: Map<number, number>; initialCcs: InitialCc[] } {
    return {
        voicemap: new Map<number, number>([[channel, program]]),
        initialCcs: [
            { channel, cc: 0x07, value: 100 },
            { channel, cc: 0x0A, value: 64  },
        ],
    };
}

interface Harness {
    posts:    string[];
    unlinks:  string[];
    writes:   SavedWrite[];
    readArgs: string[];
}

function makeHarness(opts: {
    events?:        MidiEvent[];
    voicemap?:      Map<number, number>;
    initialCcs?:    InitialCc[];
    outputPath?:    string;
    writeSmfOk?:    boolean;
    readThrows?:    string | null;
    requestThrows?: string | null;
    liveApi?:       LiveApiAdapter;
    range?:         () => { start: string; length: string };
    markerRange?:   () => { start: string; end: string };
}): { harness: Harness; writer: ReturnType<typeof createSmfWriter> } {
    const harness: Harness = { posts: [], unlinks: [], writes: [], readArgs: [] };
    const events   = opts.events   ?? [];
    const voicemap = opts.voicemap ?? new Map<number, number>();
    const writer = createSmfWriter({
        post:    (m) => harness.posts.push(m),
        requestBufferDump: () => {
            if (opts.requestThrows) return Promise.reject(new Error(opts.requestThrows));
            return Promise.resolve({ path: "/tmp/probe.bin", count: events.length });
        },
        readBufferFile: (p) => {
            harness.readArgs.push(p);
            if (opts.readThrows) throw new Error(opts.readThrows);
            return events;
        },
        unlink:  (p) => { harness.unlinks.push(p); },
        writeSmf: (p, b) => {
            harness.writes.push({ path: p, bytes: [...b] });
            return opts.writeSmfOk ?? true;
        },
        liveApi:    opts.liveApi ?? makeLiveApi(),
        voicemap:   () => voicemap,
        readInitialCcs: () => opts.initialCcs ?? [],
        outputPath: () => opts.outputPath ?? "/tmp/out.mid",
        range:      opts.range ?? (() => ({ start: "", length: "" })),
        markerRange:opts.markerRange ?? (() => ({ start: "", end: "" })),
    });
    return { harness, writer };
}

// ---------------------------------------------------------------------------
// G: save orchestrator
// ---------------------------------------------------------------------------

test("V1: initial-state validation accepts tick-0 PC, Volume CC7, and Pan CC10", () => {
    const state = requiredState();
    const bytes = buildSmf({
        events: [{ beats: 0, status: 0x90, d1: 60, d2: 100 }],
        tempo: 120,
        loop: { on: false, start: 0, length: 0 },
        timeSig: { num: 4, den: 4 },
        voicemap: state.voicemap,
        initialCcs: state.initialCcs,
    });

    assert.deepEqual(validateSmfInitialChannelState(bytes), { ok: true, missing: [] });
});

test("V2: initial-state validation reports missing tick-0 PC, Volume, and Pan", () => {
    const bytes = buildSmf({
        events: [{ beats: 0, status: 0x90, d1: 60, d2: 100 }],
        tempo: 120,
        loop: { on: false, start: 0, length: 0 },
        timeSig: { num: 4, den: 4 },
        voicemap: new Map(),
        anchorMode: "firstNote",
    });

    assert.deepEqual(validateSmfInitialChannelState(bytes), {
        ok: false,
        missing: ["ch1 Program Change", "ch1 Volume CC7", "ch1 Pan CC10"],
    });
});

test("V3: initial-state validation does not accept PC, Volume, or Pan after tick 0", () => {
    const bytes = buildSmf({
        events: [
            { beats: 1, status: 0x90, d1: 60,   d2: 100 },
            { beats: 2, status: 0xC0, d1: 5,    d2: 0   },
            { beats: 2, status: 0xB0, d1: 0x07, d2: 100 },
            { beats: 2, status: 0xB0, d1: 0x0A, d2: 64  },
        ],
        tempo: 120,
        loop: { on: false, start: 0, length: 0 },
        timeSig: { num: 4, den: 4 },
        voicemap: new Map(),
        anchorMode: "firstNote",
    });

    assert.deepEqual(validateSmfInitialChannelState(bytes), {
        ok: false,
        missing: ["ch1 Program Change", "ch1 Volume CC7", "ch1 Pan CC10"],
    });
});

test("V4: pre-anchor captured PC, Volume, and Pan are clamped to tick 0", () => {
    const bytes = buildSmf({
        events: [
            { beats: 7.99, status: 0xC0, d1: 11,   d2: 0   },
            { beats: 8.00, status: 0xC0, d1: 12,   d2: 0   },
            { beats: 8.01, status: 0xB0, d1: 0x07, d2: 100 },
            { beats: 8.02, status: 0xB0, d1: 0x0A, d2: 64  },
            { beats: 9.00, status: 0x90, d1: 60,   d2: 100 },
        ],
        tempo: 120,
        loop: { on: false, start: 0, length: 0 },
        timeSig: { num: 4, den: 4 },
        voicemap: new Map(),
        anchorMode: "firstNote",
    });

    assert.deepEqual(validateSmfInitialChannelState(bytes), { ok: true, missing: [] });
    const events = voiceEvents(parseSmf(bytes));
    assert.equal(events.some((e) => e.tick === 0 && e.status === 0xC0 && e.d1 === 11), false);
    assert.ok(events.some((e) => e.tick === 0 && e.status === 0xC0 && e.d1 === 12));
    assert.ok(events.some((e) => e.tick === 0 && e.status === 0xB0 && e.d1 === 0x07 && e.d2 === 100));
    assert.ok(events.some((e) => e.tick === 0 && e.status === 0xB0 && e.d1 === 0x0A && e.d2 === 64));
    assert.ok(events.some((e) => e.tick === 0 && e.status === 0x90 && e.d1 === 60 && e.d2 > 0));
});

test("G1: golden path — save() writes the SMF and unlinks the temp file once", async () => {
    const events: MidiEvent[] = [
        { beats: 0,   status: 0x90, d1: 60, d2: 100 },
        { beats: 1.0, status: 0x80, d1: 60, d2: 0   },
    ];
    const { harness, writer } = makeHarness({ events, ...requiredState() });
    await writer.save();

    assert.equal(harness.writes.length, 1, "writeSmf called once");
    assert.equal(harness.writes[0].path, "/tmp/out.mid");
    assert.equal(harness.unlinks.length, 1, "unlink called once");
    assert.equal(harness.unlinks[0], "/tmp/probe.bin");
});

test("G2: bytes passed to writeSmf equal buildSmf(events, livestate) byte-for-byte", async () => {
    const events: MidiEvent[] = [
        { beats: 0,   status: 0x90, d1: 64, d2: 100 },
        { beats: 2.0, status: 0x80, d1: 64, d2: 0   },
        { beats: 0,   status: 0xB0, d1: 7,  d2: 100 },
    ];
    const voicemap = new Map<number, number>([[0, 5]]);
    const initialCcs = requiredState().initialCcs;
    const liveApi  = makeLiveApi({
        tempo: 140, loop: { on: true, start: 4, length: 4 },
        timeSig: { num: 3, den: 8 },
    });
    const { harness, writer } = makeHarness({ events, voicemap, initialCcs, liveApi });
    await writer.save();

    const expected = buildSmf({
        events,
        tempo: 140,
        loop:  { on: true, start: 4, length: 4 },
        timeSig: { num: 3, den: 8 },
        voicemap,
        anchorMode: "firstNote",
        initialCcs,
        loopMarkers: null,
    });
    const got = harness.writes[0].bytes;
    assert.equal(got.length, expected.length);
    for (let i = 0; i < expected.length; i++) {
        assert.equal(got[i], expected[i], `byte ${i}`);
    }
});

test("G2b: blank Start or Beats means save the whole buffer with no range trim", async () => {
    const events: MidiEvent[] = [
        { beats: 2, status: 0x90, d1: 60, d2: 100 },
        { beats: 8, status: 0x80, d1: 60, d2: 0   },
    ];
    const state = requiredState();
    const liveApi = makeLiveApi({ loop: { on: false, start: 0, length: 0 } });

    for (const range of [
        () => ({ start: "", length: "" }),
        () => ({ start: "", length: "4" }),
        () => ({ start: "4", length: "" }),
    ]) {
        const { harness, writer } = makeHarness({ events, liveApi, range, ...state });
        await writer.save();
        const expected = buildSmf({
            events,
            tempo: 120,
            loop: { on: false, start: 0, length: 0 },
            timeSig: { num: 4, den: 4 },
            voicemap: state.voicemap,
            anchorMode: "firstNote",
            initialCcs: state.initialCcs,
        });
        assert.deepEqual(harness.writes[0].bytes, [...expected]);
        assert.ok(
            !harness.posts.some((m) => m.includes("export range start=")),
            "no explicit range was applied",
        );
    }
});

test("G2c: nonblank Start and Beats still trim to the requested range", async () => {
    const events: MidiEvent[] = [
        { beats: 2, status: 0x90, d1: 60, d2: 100 },
        { beats: 6, status: 0x90, d1: 61, d2: 100 },
    ];
    const state = requiredState();
    const liveApi = makeLiveApi({ loop: { on: false, start: 0, length: 0 } });
    const { harness, writer } = makeHarness({
        events,
        liveApi,
        range: () => ({ start: "4", length: "4" }),
        ...state,
    });
    await writer.save();

    const expected = buildSmf({
        events,
        tempo: 120,
        loop: { on: false, start: 0, length: 0 },
        timeSig: { num: 4, den: 4 },
        voicemap: state.voicemap,
        initialCcs: state.initialCcs,
        range: { startBeats: 4, endBeats: 8 },
        loopMarkers: null,
    });
    assert.deepEqual(harness.writes[0].bytes, [...expected]);
    assert.ok(
        harness.posts.some((m) => m.includes("export range start=4 length=4 end=8")),
        "explicit range was applied",
    );
});

test("G2d: explicit BBS Loop Start and Loop End fields insert loop markers", async () => {
    const events: MidiEvent[] = [
        { beats: 16, status: 0x90, d1: 60, d2: 100 },
        { beats: 17, status: 0x80, d1: 60, d2: 0   },
    ];
    const state = requiredState();
    const { harness, writer } = makeHarness({
        events,
        markerRange: () => ({ start: "5.1.1", end: "6.1.1" }),
        ...state,
    });
    await writer.save();

    const markers = metaEvents(parseSmf(harness.writes[0].bytes)).filter((e) => e.metaType === 0x06);
    assert.equal(markers.length, 2);
    const open  = markers.find((m) => String.fromCharCode(m.data[0]) === "[");
    const close = markers.find((m) => String.fromCharCode(m.data[0]) === "]");
    assert.ok(open && close);
    assert.equal(open!.tick,  0);
    assert.equal(close!.tick, 4 * PPQ);
    assert.ok(
        harness.posts.some((m) => m.includes("loop markers start=16 end=20")),
        "BBS marker fields were parsed to absolute beats",
    );
});

test("G2e: blank marker fields ignore Live loop markers", async () => {
    const events: MidiEvent[] = [
        { beats: 4, status: 0x90, d1: 60, d2: 100 },
        { beats: 5, status: 0x80, d1: 60, d2: 0   },
    ];
    const state = requiredState();
    const liveApi = makeLiveApi({ loop: { on: true, start: 4, length: 4 } });
    const { harness, writer } = makeHarness({ events, liveApi, ...state });
    await writer.save();

    const markers = metaEvents(parseSmf(harness.writes[0].bytes)).filter((e) => e.metaType === 0x06);
    assert.equal(markers.length, 0);
});

test("G3: empty filename → save() short-circuits BEFORE requesting a dump", async () => {
    let dumpCalled = false;
    const writer = createSmfWriter({
        post: () => {},
        requestBufferDump: () => { dumpCalled = true; return Promise.resolve({ path: "", count: 0 }); },
        readBufferFile: () => [],
        unlink: () => {},
        writeSmf: () => true,
        liveApi: makeLiveApi(),
        voicemap: () => new Map(),
        outputPath: () => "",
        range: () => ({ start: "", length: "" }),
        markerRange: () => ({ start: "", end: "" }),
    });
    await writer.save();
    assert.equal(dumpCalled, false, "no dump request when filename empty");
});

test("G4: writeSmf returning false → unlink STILL runs, error posted", async () => {
    const events: MidiEvent[] = [{ beats: 0, status: 0x90, d1: 60, d2: 100 }];
    const { harness, writer } = makeHarness({ events, writeSmfOk: false, ...requiredState() });
    await writer.save();
    assert.equal(harness.writes.length,  1);
    assert.equal(harness.unlinks.length, 1, "unlink runs even when writeSmf fails");
    assert.ok(
        harness.posts.some((m) => m.includes("FAILED to write")),
        "FAILED post present",
    );
});

test("G4b: missing initial channel state refuses export before writeSmf", async () => {
    const events: MidiEvent[] = [{ beats: 0, status: 0x90, d1: 60, d2: 100 }];
    const statuses: string[] = [];
    const { harness, writer } = (() => {
        const harness: Harness = { posts: [], unlinks: [], writes: [], readArgs: [] };
        const writer = createSmfWriter({
            post: (m) => harness.posts.push(m),
            status: (m) => statuses.push(m),
            requestBufferDump: () => Promise.resolve({ path: "/tmp/probe.bin", count: events.length }),
            readBufferFile: (p) => { harness.readArgs.push(p); return events; },
            unlink: (p) => { harness.unlinks.push(p); },
            writeSmf: (p, b) => { harness.writes.push({ path: p, bytes: [...b] }); return true; },
            liveApi: makeLiveApi(),
            voicemap: () => new Map(),
            readInitialCcs: () => [],
            outputPath: () => "/tmp/out.mid",
            range: () => ({ start: "", length: "" }),
            markerRange: () => ({ start: "", end: "" }),
        });
        return { harness, writer };
    })();

    assert.equal(await writer.save(), false);

    assert.equal(harness.writes.length, 0, "SMF is not written");
    assert.equal(harness.unlinks.length, 1, "temp file is still cleaned up");
    assert.deepEqual(statuses, [
        "Saving...",
        "FAILED: missing tick-0 MIDI",
    ]);
    assert.ok(
        harness.posts.some((m) => m.includes("refusing export; missing tick-0 channel state")),
        "refusal reason is posted",
    );
});

test("G5: readBufferFile throwing → unlink STILL runs, error caught", async () => {
    const { harness, writer } = makeHarness({ readThrows: "bad magic" });
    await writer.save();
    assert.equal(harness.unlinks.length, 1, "unlink runs after parse error");
    assert.equal(harness.writes.length,  0, "no SMF write on parse error");
    assert.ok(harness.posts.some((m) => m.includes("save threw")));
});

test("G6: requestBufferDump rejecting → no unlink (no path was ever assigned)", async () => {
    const { harness, writer } = makeHarness({ requestThrows: "dump already in flight" });
    await writer.save();
    // requestBufferDump failed before tempPath was learned → unlink would
    // operate on empty path; the writer guards this with `if (tempPath)`.
    assert.equal(harness.unlinks.length, 0);
    assert.ok(harness.posts.some((m) => m.includes("save threw")));
});

test("G7: liveApi.getTempo throwing → adapter-side fallback or save-time catch (no crash)", async () => {
    const events: MidiEvent[] = [{ beats: 0, status: 0x90, d1: 60, d2: 100 }];
    const liveApi: LiveApiAdapter = {
        getTempo:   () => { throw new Error("LOM panic"); },
        getLoop:    () => ({ on: false, start: 0, length: 0 }),
        getTimeSig: () => ({ num: 4, den: 4 }),
    };
    const { harness, writer } = makeHarness({ events, liveApi });
    await writer.save();
    // The save's outer try/catch should swallow this. Unlink must still fire.
    assert.equal(harness.unlinks.length, 1);
    // No SMF written because buildSmf was never called.
    assert.equal(harness.writes.length, 0);
});

test("G8: buildSmf throw (unrouted internal failure simulated via writeSmf throw) → unlink STILL runs", async () => {
    const events: MidiEvent[] = [{ beats: 0, status: 0x90, d1: 60, d2: 100 }];
    const harness: Harness = { posts: [], unlinks: [], writes: [], readArgs: [] };
    const state = requiredState();
    const writer = createSmfWriter({
        post:    (m) => harness.posts.push(m),
        requestBufferDump: () => Promise.resolve({ path: "/tmp/probe.bin", count: 1 }),
        readBufferFile:    () => events,
        unlink:  (p) => { harness.unlinks.push(p); },
        writeSmf: () => { throw new Error("disk full"); },
        liveApi:    makeLiveApi(),
        voicemap:   () => state.voicemap,
        readInitialCcs: () => state.initialCcs,
        outputPath: () => "/tmp/out.mid",
        range:      () => ({ start: "", length: "" }),
        markerRange:() => ({ start: "", end: "" }),
    });
    await writer.save();
    assert.equal(harness.unlinks.length, 1, "unlink runs even when writeSmf throws");
});

test("G9: unlink itself throwing — finally swallows it (no unhandled rejection)", async () => {
    const events: MidiEvent[] = [{ beats: 0, status: 0x90, d1: 60, d2: 100 }];
    const state = requiredState();
    const writer = createSmfWriter({
        post: () => {},
        requestBufferDump: () => Promise.resolve({ path: "/tmp/probe.bin", count: 1 }),
        readBufferFile: () => events,
        unlink: () => { throw new Error("unlink boom"); },
        writeSmf: () => true,
        liveApi: makeLiveApi(),
        voicemap: () => state.voicemap,
        readInitialCcs: () => state.initialCcs,
        outputPath: () => "/tmp/out.mid",
        range: () => ({ start: "", length: "" }),
        markerRange: () => ({ start: "", end: "" }),
    });
    // Should resolve cleanly — unlink failures must not propagate.
    await writer.save();
});

// ---------------------------------------------------------------------------
// H: dump handshake (createDumpHandshake)
// ---------------------------------------------------------------------------

function makeHandshakeHarness(opts: { timeoutMs?: number } = {}) {
    const outletCalls: { idx: number; args: unknown[] }[] = [];
    const posts: string[] = [];
    let pathCounter = 0;
    const handshake = createDumpHandshake({
        outlet: (idx, ...args) => outletCalls.push({ idx, args }),
        tempPath: () => `/tmp/dump-${++pathCounter}.bin`,
        post: (m) => posts.push(m),
        timeoutMs: opts.timeoutMs,
    });
    return { handshake, outletCalls, posts };
}

test("H1: dumped() with matching path resolves the pending dump promise", async () => {
    const { handshake, outletCalls } = makeHandshakeHarness();
    const promise = handshake.requestBufferDump();
    // outlet(0, 'dump', '/tmp/dump-1.bin') should have been called.
    assert.equal(outletCalls.length, 1);
    assert.equal(outletCalls[0].idx, 0);
    assert.equal(outletCalls[0].args[0], "dump");
    const sentPath = outletCalls[0].args[1] as string;

    handshake.dumped(sentPath, 42);
    const result = await promise;
    assert.equal(result.path, sentPath);
    assert.equal(result.count, 42);
});

test("H2: dumped() with mismatched path REJECTS the pending dump", async () => {
    const { handshake } = makeHandshakeHarness();
    const promise = handshake.requestBufferDump();
    handshake.dumped("/tmp/wrong.bin", 0);
    await assert.rejects(promise, /path mismatch/);
});

test("H3: dumped() with no pending dump posts a warning and does NOT throw", () => {
    const { handshake, posts } = makeHandshakeHarness();
    assert.doesNotThrow(() => handshake.dumped("/tmp/whatever.bin", 0));
    assert.ok(posts.some((m) => m.includes("ignoring unexpected dumped reply")));
});

test("H4: second requestBufferDump while one is pending REJECTS immediately", async () => {
    const { handshake } = makeHandshakeHarness();
    const _first = handshake.requestBufferDump();
    await assert.rejects(
        handshake.requestBufferDump(),
        /dump already in flight/,
    );
    // Drain the first to keep node happy.
    handshake.dumped("/tmp/dump-1.bin", 1);
    await _first;
});

test("H5: after dumped resolves, a fresh requestBufferDump works (pending slot is cleared)", async () => {
    const { handshake, outletCalls } = makeHandshakeHarness();
    const p1 = handshake.requestBufferDump();
    handshake.dumped(outletCalls[0].args[1] as string, 1);
    await p1;
    const p2 = handshake.requestBufferDump();
    assert.equal(outletCalls.length, 2, "second dump request emitted to outlet");
    handshake.dumped(outletCalls[1].args[1] as string, 2);
    const r = await p2;
    assert.equal(r.count, 2);
});

test("H6: after a path-mismatch rejection, the pending slot is CLEARED so a retry can proceed", async () => {
    const { handshake, outletCalls } = makeHandshakeHarness();
    const p1 = handshake.requestBufferDump();
    handshake.dumped("/tmp/wrong.bin", 0);
    await assert.rejects(p1, /path mismatch/);
    // A new dump must now be acceptable.
    assert.equal(handshake.isPending(), false, "pending cleared after rejection");
    const p2 = handshake.requestBufferDump();
    handshake.dumped(outletCalls[1].args[1] as string, 5);
    const r = await p2;
    assert.equal(r.count, 5);
});

test("H6b: dump timeout rejects and clears pending so save cannot hang forever", async () => {
    const { handshake, posts } = makeHandshakeHarness({ timeoutMs: 1 });
    const p1 = handshake.requestBufferDump();
    await assert.rejects(p1, /dump timed out waiting for poryaaaa~ reply/);
    assert.equal(handshake.isPending(), false);
    assert.ok(posts.some((m) => m.includes("dump timed out waiting for poryaaaa~ reply")));

    const p2 = handshake.requestBufferDump();
    handshake.dumped("/tmp/dump-2.bin", 7);
    const result = await p2;
    assert.equal(result.count, 7);
});

test("H6c: dumped() with count 0 rejects as nothing recorded", async () => {
    const { handshake, outletCalls } = makeHandshakeHarness();
    const p1 = handshake.requestBufferDump();
    handshake.dumped(outletCalls[0].args[1] as string, 0);
    await assert.rejects(p1, /nothing recorded/);
    assert.equal(handshake.isPending(), false);
});

test("H6d: dumpfailed() rejects with a user-facing reason and clears pending", async () => {
    const { handshake, outletCalls } = makeHandshakeHarness();
    const p1 = handshake.requestBufferDump();
    handshake.dumpfailed(outletCalls[0].args[1] as string, "nothing_recorded");
    await assert.rejects(p1, /nothing recorded/);
    assert.equal(handshake.isPending(), false);

    const p2 = handshake.requestBufferDump();
    handshake.dumped(outletCalls[1].args[1] as string, 3);
    const result = await p2;
    assert.equal(result.count, 3);
});

test("H7: integration — save() while a previous dump is mid-flight rejects without losing the first", async () => {
    // Drive two saves through the same handshake. The first holds the
    // pendingDump slot. The second must reject with 'dump already in flight'.
    const { handshake, outletCalls } = makeHandshakeHarness();
    const events: MidiEvent[] = [{ beats: 0, status: 0x90, d1: 60, d2: 100 }];
    const state = requiredState();

    const harness1: Harness = { posts: [], unlinks: [], writes: [], readArgs: [] };
    const harness2: Harness = { posts: [], unlinks: [], writes: [], readArgs: [] };
    const writerCommon = {
        readBufferFile: () => events,
        liveApi: makeLiveApi(),
        voicemap: () => state.voicemap,
        readInitialCcs: () => state.initialCcs,
        writeSmf: (_p: string, _b: number[]) => true,
        outputPath: () => "/tmp/out.mid",
        range: () => ({ start: "", length: "" }),
        markerRange: () => ({ start: "", end: "" }),
    };
    const writer1 = createSmfWriter({
        ...writerCommon,
        post: (m) => harness1.posts.push(m),
        requestBufferDump: handshake.requestBufferDump,
        unlink: (p) => harness1.unlinks.push(p),
        writeSmf: (p, b) => { harness1.writes.push({ path: p, bytes: [...b] }); return true; },
    });
    const writer2 = createSmfWriter({
        ...writerCommon,
        post: (m) => harness2.posts.push(m),
        requestBufferDump: handshake.requestBufferDump,
        unlink: (p) => harness2.unlinks.push(p),
        writeSmf: (p, b) => { harness2.writes.push({ path: p, bytes: [...b] }); return true; },
    });

    const save1 = writer1.save();
    const save2 = writer2.save();         // contends — should not steal slot
    // Resolve only the first dump.
    handshake.dumped(outletCalls[0].args[1] as string, events.length);
    await save1;
    await save2;

    assert.equal(harness1.writes.length, 1, "first save completed");
    assert.equal(harness2.writes.length, 0, "second save did not write");
    assert.ok(
        harness2.posts.some((m) => m.includes("dump already in flight"))
        || harness2.posts.some((m) => m.includes("save threw")),
        "second save reported the rejection",
    );
});

// ---------------------------------------------------------------------------
// I: unlinkTemp delegates to the Node-for-Max cleanup.js sidecar via outlet 0.
//
// The previous v8-side implementation opened the file in write mode (which
// truncates rather than unlinks) and left zero-byte temp files on disk —
// the v8 File API has no delete(). The new design routes `unlink <path>`
// through the patcher's [route unlink] into [node.script cleanup.js], which
// calls fs.unlinkSync. End-to-end deletion can only be tested by spawning
// the sidecar (out of scope here); this test pins the contract at the v8
// boundary — the message v8 emits to outlet 0.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// S: status callback — UI-bound brief messages for the save indicator.
//
// The status callback feeds the patcher's status comment via outlet 2. Tests
// pin the contract that every save attempt emits at least one status call,
// and that the ordering is Saving... → (Saved | FAILED) so the user always
// sees a terminal outcome message (no stuck-on-Saving... indicator).
// ---------------------------------------------------------------------------

function harnessWithStatus(extra: Parameters<typeof makeHarness>[0] & { status?: (m: string) => void } = {}) {
    const statuses: string[] = [];
    const events = extra.events ?? [];
    const writer = createSmfWriter({
        post: () => {},
        status: (m) => statuses.push(m),
        requestBufferDump: () => {
            if (extra.requestThrows) return Promise.reject(new Error(extra.requestThrows));
            return Promise.resolve({ path: "/tmp/probe.bin", count: events.length });
        },
        readBufferFile: () => {
            if (extra.readThrows) throw new Error(extra.readThrows);
            return events;
        },
        unlink: () => {},
        writeSmf: () => extra.writeSmfOk ?? true,
        liveApi: extra.liveApi ?? makeLiveApi(),
        voicemap: () => extra.voicemap ?? new Map(),
        readInitialCcs: () => extra.initialCcs ?? [],
        outputPath: () => extra.outputPath ?? "/tmp/out.mid",
        range: () => ({ start: "", length: "" }),
        markerRange: () => ({ start: "", end: "" }),
    });
    return { statuses, writer };
}

test("S1: success path emits Saving... then Saved: <name> (N events)", async () => {
    const events: MidiEvent[] = [
        { beats: 0,   status: 0x90, d1: 60, d2: 100 },
        { beats: 1.0, status: 0x80, d1: 60, d2: 0   },
    ];
    const { statuses, writer } = harnessWithStatus({
        events, outputPath: "/Users/spencer/Music/poryaaaa-recordings/song.mid", ...requiredState(),
    });
    assert.equal(await writer.save(), true);
    assert.equal(statuses.length, 2, "exactly two status emissions");
    assert.equal(statuses[0], "Saving...");
    assert.equal(statuses[1], "Saved: song.mid (2 events)");
});

test("S2: writeSmf false → Saving... then FAILED: write error", async () => {
    const events: MidiEvent[] = [{ beats: 0, status: 0x90, d1: 60, d2: 100 }];
    const { statuses, writer } = harnessWithStatus({ events, writeSmfOk: false, ...requiredState() });
    assert.equal(await writer.save(), false);
    assert.deepEqual(statuses, ["Saving...", "FAILED: write error"]);
});

test("S3: empty filename → only FAILED: no filename (no Saving...)", async () => {
    const { statuses, writer } = harnessWithStatus({ outputPath: "" });
    assert.equal(await writer.save(), false);
    assert.deepEqual(statuses, ["FAILED: no filename"]);
});

test("S4: readBufferFile throwing → Saving... then FAILED: <ErrorClass>", async () => {
    const { statuses, writer } = harnessWithStatus({ readThrows: "bad magic" });
    await writer.save();
    assert.equal(statuses[0], "Saving...");
    assert.ok(statuses[1].startsWith("FAILED:"), "second status is a FAILED message");
    assert.equal(statuses.length, 2);
});

test("S5: requestBufferDump rejecting → Saving... then FAILED: <ErrorClass>", async () => {
    const { statuses, writer } = harnessWithStatus({ requestThrows: "dump already in flight" });
    await writer.save();
    assert.equal(statuses[0], "Saving...");
    assert.ok(statuses[1].startsWith("FAILED:"));
});

test("S6: status callback is optional — save() works without one", async () => {
    const events: MidiEvent[] = [{ beats: 0, status: 0x90, d1: 60, d2: 100 }];
    const state = requiredState();
    const writer = createSmfWriter({
        post: () => {},
        // no status field
        requestBufferDump: () => Promise.resolve({ path: "/tmp/probe.bin", count: 1 }),
        readBufferFile: () => events,
        unlink: () => {},
        writeSmf: () => true,
        liveApi: makeLiveApi(),
        voicemap: () => state.voicemap,
        readInitialCcs: () => state.initialCcs,
        outputPath: () => "/tmp/out.mid",
        range: () => ({ start: "", length: "" }),
        markerRange: () => ({ start: "", end: "" }),
    });
    await writer.save();   // must not throw
});

test("I1: writer.unlink hook emits `unlink <path>` on outlet 0", async () => {
    type OutletCall = [number, ...unknown[]];
    const outletCalls: OutletCall[] = [];
    const outletSpy = (idx: number, ...args: unknown[]) => {
        outletCalls.push([idx, ...args]);
    };

    // Mirror the Max-runtime wiring in ccomidi_recorder.ts: the writer's
    // `unlink` hook is `(path) => outlet(0, "unlink", path)`.
    const unlinkHook = (path: string) => outletSpy(0, "unlink", path);

    const events: MidiEvent[] = [
        { beats: 0,   status: 0x90, d1: 60, d2: 100 },
        { beats: 0.5, status: 0x80, d1: 60, d2: 0   },
    ];
    const state = requiredState();
    const writer = createSmfWriter({
        post:              () => {},
        requestBufferDump: () => Promise.resolve({ path: "/tmp/poryaaaa-probe.bin", count: events.length }),
        readBufferFile:    () => events,
        unlink:            unlinkHook,
        writeSmf:          () => true,
        liveApi:           makeLiveApi(),
        voicemap:          () => state.voicemap,
        readInitialCcs:    () => state.initialCcs,
        outputPath:        () => "/tmp/out.mid",
        range:             () => ({ start: "", length: "" }),
        markerRange:       () => ({ start: "", end: "" }),
    });

    await writer.save();

    const unlinkOutlets = outletCalls.filter((c) => c[1] === "unlink");
    assert.equal(unlinkOutlets.length, 1, "unlink emitted exactly once");
    assert.deepEqual(
        unlinkOutlets[0],
        [0, "unlink", "/tmp/poryaaaa-probe.bin"],
        "outlet index 0, selector `unlink`, then the temp path",
    );
});
