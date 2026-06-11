import test from "node:test";
import assert from "node:assert/strict";

import {
    CcomidiVoicesService,
    routingChannelForTrackIndex,
    routingChoices,
    trackIndexFromPath,
} from "../ccomidi_voices";
import { makeMockVoicesDeps, routeWithBroadcast } from "./_mocks";

function outletArgs(deps: ReturnType<typeof makeMockVoicesDeps>): unknown[][] {
    return deps.outletCalls.map((c) => c.args);
}

interface SlotEntry {
    program: number;
    name: string;
    typeCode?: number;
    envelope?: unknown;
}

const DEFAULT_TYPE_CODE = 0;

// Tagged label: voice name with the family-tag prefix, what the umenu items
// (and the setsymbol-driven selection) carry.
function ds(name: string): string {
    return `[DS] ${name}`;
}

function statePayload(slots: SlotEntry[]) {
    return {
        slots: slots.map((s) => ({
            ...s,
            typeCode: s.typeCode === undefined ? DEFAULT_TYPE_CODE : s.typeCode,
        })),
    };
}

function encodedState(slots: SlotEntry[]): string {
    return encodeURIComponent(JSON.stringify(statePayload(slots)));
}

// Helper: drive the service through `start` + state + a synthetic load
// pick (mirrors what [r #0-sync] does in the device on Live load).
function startWithSlots(
    slots: SlotEntry[],
): ReturnType<typeof makeMockVoicesDeps> & {
    svc: CcomidiVoicesService;
} {
    const deps = makeMockVoicesDeps();
    const svc = new CcomidiVoicesService(deps);
    svc.start();
    svc.state(encodedState(slots));
    svc.pick(0); // synthetic load pick
    deps.reset();
    return Object.assign(deps, { svc });
}

// Build the sequence emitted whenever the slot list is rendered into the
// umenu (slots clear + slots append × N).
function menuPopulate(slots: SlotEntry[]): unknown[][] {
    const events: unknown[][] = [["slots", "clear"]];
    for (const s of slots) {
        const tag = `[${familyTagFor(s.typeCode ?? DEFAULT_TYPE_CODE)}]`;
        events.push(["slots", "append", `${tag} ${s.name}`]);
    }
    return events;
}

function familyTagFor(typeCode: number): string {
    switch (typeCode) {
        case 0x00: case 0x08: case 0x10: return "DS";
        case 0x01: case 0x09: return "Sq1";
        case 0x02: case 0x0A: return "Sq2";
        case 0x03: case 0x0B: return "Wav";
        case 0x04: case 0x0C: return "Nse";
        case 0x20: return "Cry";
        case 0x30: return "Cr-";
        case 0x40: return "Spl";
        case 0x80: return "Spl*";
        default: return "?";
    }
}

// ---- start / load --------------------------------------------------------

test("`start` populates the umenu with a waiting placeholder when no snapshot is available", () => {
    const deps = makeMockVoicesDeps();
    const svc = new CcomidiVoicesService(deps);
    svc.start();

    assert.deepEqual(outletArgs(deps), [
        ["slots", "clear"],
        ["slots", "append", "(waiting for poryaaaa)"],
    ]);
});

test("`ready` is accepted without changing picker state", () => {
    const deps = makeMockVoicesDeps();
    const svc = new CcomidiVoicesService(deps);
    svc.ready();

    assert.deepEqual(outletArgs(deps), []);
});

test("`reload` does not request voice state; WebSocket snapshots drive updates", () => {
    const deps = makeMockVoicesDeps();
    const svc = new CcomidiVoicesService(deps);
    svc.start();
    deps.reset();
    svc.reload();

    assert.deepEqual(outletArgs(deps), [
        ["slots", "clear"],
        ["slots", "append", "(waiting for poryaaaa)"],
    ]);
});

test("inbound WebSocket state after `start` ungates and updates the visible menu label", () => {
    const deps = makeMockVoicesDeps();
    const svc = new CcomidiVoicesService(deps);
    svc.start();
    deps.reset();

    const slots: SlotEntry[] = [
        { program: 0, name: "Alpha" },
        { program: 1, name: "Beta" },
    ];
    svc.state(encodedState(slots));

    assert.deepEqual(outletArgs(deps), [
        ...menuPopulate(slots),
        ["slots", "setsymbol", ds("Alpha")],
    ]);
});

// ---- pick basics ---------------------------------------------------------

test("`pick` is gated until the first state arrives", () => {
    const deps = makeMockVoicesDeps();
    const svc = new CcomidiVoicesService(deps);
    svc.start();
    deps.reset();

    svc.pick(0);
    assert.equal(outletArgs(deps).length, 0);
});

test("`pick` after start+state updates the selected voice label", () => {
    const deps = makeMockVoicesDeps();
    const svc = new CcomidiVoicesService(deps);
    svc.start();
    svc.state(encodedState([
        { program: 0, name: "Alpha" },
        { program: 1, name: "Beta" },
    ]));
    deps.reset();

    svc.pick(1);
    assert.deepEqual(outletArgs(deps), [
        ["slots", "setsymbol", ds("Beta")],
    ]);
});

test("`pick` indexes into the slots array; VoiceIdx owns Program Change", () => {
    const deps = startWithSlots([
        { program: 0, name: "a" },
        { program: 1, name: "b" },
    ]);
    const svc = deps.svc;

    svc.pick(1);
    assert.deepEqual(outletArgs(deps), [
        ["slots", "setsymbol", ds("b")],
    ]);
});

test("`pick` with an out-of-range index shows no voice", () => {
    const deps = startWithSlots([
        { program: 0, name: "x" },
    ]);
    const svc = deps.svc;

    svc.pick(5);
    svc.pick(-1);
    assert.deepEqual(outletArgs(deps), [
        ["slots", "setsymbol", "(no voice)"],
        ["slots", "setsymbol", "(no voice)"],
    ]);
});

test("`pick` ignores non-integer input", () => {
    const deps = startWithSlots([
        { program: 0, name: "x" },
    ]);
    const svc = deps.svc;

    svc.pick(1.5);
    svc.pick(NaN);
    assert.deepEqual(outletArgs(deps), []);
});

test("`route` asks Live routing helper to update Ableton track routing", () => {
    const deps = makeMockVoicesDeps();
    let routed = 0;
    const svc = new CcomidiVoicesService({
        ...deps,
        routeTrack: () => { routed++; },
    });

    routeWithBroadcast(svc);

    assert.equal(routed, 1);
    assert.deepEqual(outletArgs(deps), [["autorouted", 1]]);
});

test("`route` broadcasts a reroute request that other ccomidi instances apply", () => {
    const shared = makeMockVoicesDeps();
    let routedA = 0;
    let routedB = 0;
    const b = new CcomidiVoicesService({
        ...shared,
        routeTrack: () => { routedB++; },
    });
    const a = new CcomidiVoicesService({
        ...shared,
        routeTrack: () => { routedA++; },
    });
    b.start();
    a.start();
    shared.reset();

    const encoded = routeWithBroadcast(a);

    assert.equal(routedA, 1);
    assert.equal(routedB, 0);

    b.peerReroute(encoded);

    assert.equal(routedA, 1);
    assert.equal(routedB, 1);
});

test("`autorouteifnew(0)` routes once and marks the saved autoroute flag", () => {
    const deps = makeMockVoicesDeps();
    let routed = 0;
    const svc = new CcomidiVoicesService({
        ...deps,
        routeTrack: () => { routed++; },
    });

    svc.autorouteifnew(0);

    assert.equal(routed, 1);
    assert.deepEqual(outletArgs(deps), [["autorouted", 1]]);
});

test("`autorouteifnew(1)` preserves existing routing on Live Set load", () => {
    const deps = makeMockVoicesDeps();
    let routed = 0;
    const svc = new CcomidiVoicesService({
        ...deps,
        routeTrack: () => { routed++; },
    });

    svc.autorouteifnew(1);

    assert.equal(routed, 0);
    assert.deepEqual(outletArgs(deps), []);
});

test("routingChoices parses documented dictionary wrapper", () => {
    assert.deepEqual(
        routingChoices({
            available_output_routing_types: [
                { display_name: "poryaaaa", identifier: 1 },
            ],
        }, "available_output_routing_types"),
        [{ display_name: "poryaaaa", identifier: 1 }],
    );
});

test("routingChoices parses LiveAPI track.get JSON atom arrays", () => {
    assert.deepEqual(
        routingChoices([
            JSON.stringify({
                available_output_routing_types: [
                    { display_name: "1-poryaaaa", identifier: 1 },
                    { display_name: "2-MIDI", identifier: 2 },
                ],
            }),
        ], "available_output_routing_types"),
        [
            { display_name: "1-poryaaaa", identifier: 1 },
            { display_name: "2-MIDI", identifier: 2 },
        ],
    );
});

test("routingChoices rejects bare routing-choice lists instead of hiding track.get contract drift", () => {
    assert.throws(
        () => routingChoices([
            { display_name: "poryaaaa", identifier: 1 },
            { display_name: "No Output", identifier: "none" },
        ], "available_output_routing_types"),
        /available_output_routing_types must be the dictionary returned by track\.get/,
    );
});

test("routingChoices rejects named dict message forms that track.get does not return", () => {
    assert.throws(
        () => routingChoices(
            ["dictionary", "ccomidi_routing_fixture"],
            "available_output_routing_types",
        ),
        /available_output_routing_types must be the dictionary returned by track\.get/,
    );
});

test("trackIndexFromPath derives the zero-based Live track index", () => {
    assert.equal(trackIndexFromPath("live_set tracks 0"), 0);
    assert.equal(trackIndexFromPath("live_set tracks 6 devices 0"), 6);
});

test("routingChannelForTrackIndex maps Live track ordinal to matching poryaaaa input", () => {
    const choice = routingChannelForTrackIndex([
        { choice: { display_name: "1-poryaaaa", identifier: 9 }, channel: 0 },
        { choice: { display_name: "2-poryaaaa", identifier: 10 }, channel: 1 },
        { choice: { display_name: "3-poryaaaa", identifier: 11 }, channel: 2 },
    ], 1);

    assert.deepEqual(choice, {
        choice: { display_name: "2-poryaaaa", identifier: 10 },
        channel: 1,
    });
});

test("routingChannelForTrackIndex fails when poryaaaa does not expose that track ordinal", () => {
    assert.throws(
        () => routingChannelForTrackIndex([
            { choice: { display_name: "1-poryaaaa", identifier: 9 }, channel: 0 },
        ], 6),
        /poryaaaa exposes no MIDI input 7/,
    );
});

// ---- pending pick (race: pick fires before slots arrive) ----------------

test("pick before any state stashes the index and applies on state", () => {
    const deps = makeMockVoicesDeps();
    const svc = new CcomidiVoicesService(deps);
    svc.start();
    deps.reset();

    svc.pick(1);
    assert.deepEqual(outletArgs(deps), []);

    const slots: SlotEntry[] = [
        { program: 0, name: "A" },
        { program: 1, name: "B" },
    ];
    svc.state(encodedState(slots));

    assert.deepEqual(outletArgs(deps), [
        ...menuPopulate(slots),
        ["slots", "setsymbol", ds("B")],
    ]);
});

test("pending out-of-range pick: menu populates, then shows no voice", () => {
    const deps = makeMockVoicesDeps();
    const svc = new CcomidiVoicesService(deps);
    svc.start();
    svc.pick(50); // saved-set had more slots than current voicegroup
    deps.reset();

    const slots: SlotEntry[] = [
        { program: 0, name: "Only" },
    ];
    svc.state(encodedState(slots));

    assert.deepEqual(outletArgs(deps), [
        ...menuPopulate(slots),
        ["slots", "setsymbol", "(no voice)"],
    ]);
});

// ---- WebSocket state semantics -------------------------------------------

test("every valid state payload applies; there is no seq de-dupe", () => {
    const deps = makeMockVoicesDeps();
    const svc = new CcomidiVoicesService(deps);
    svc.start();
    svc.state(encodedState([
        { program: 0, name: "Fresh" },
    ]));
    deps.reset();

    const slots: SlotEntry[] = [
        { program: 0, name: "Replacement" },
    ];
    svc.state(encodedState(slots));
    assert.deepEqual(outletArgs(deps), [
        ...menuPopulate(slots),
        ["slots", "setsymbol", ds("Replacement")],
    ]);

    deps.reset();
    svc.state(encodedState(slots));
    assert.deepEqual(outletArgs(deps), [
        ...menuPopulate(slots),
        ["slots", "setsymbol", ds("Replacement")],
    ]);
});

test("a new state payload replaces the slot list and replays the saved pick", () => {
    const deps = makeMockVoicesDeps();
    const svc = new CcomidiVoicesService(deps);
    svc.start();
    svc.pick(0); // pending
    svc.state(encodedState([
        { program: 0, name: "Old" },
    ]));
    deps.reset();

    const newSlots: SlotEntry[] = [
        { program: 0, name: "Replaced" },
        { program: 1, name: "New" },
    ];
    svc.state(encodedState(newSlots));

    assert.deepEqual(outletArgs(deps), [
        ...menuPopulate(newSlots),
        ["slots", "setsymbol", ds("Replaced")],
    ]);
});

test("ignored empty-slots payload keeps the gate where it is", () => {
    const deps = makeMockVoicesDeps();
    const svc = new CcomidiVoicesService(deps);
    svc.start();
    deps.reset();

    svc.state(encodedState([]));
    assert.deepEqual(outletArgs(deps), []);

    const slots: SlotEntry[] = [{ program: 0, name: "x" }];
    svc.state(encodedState(slots));
    assert.deepEqual(outletArgs(deps), [
        ...menuPopulate(slots),
        ["slots", "setsymbol", ds("x")],
    ]);
});

// ---- slot parsing ----------------------------------------------------------

test("slots without the ccomidi-used fields are ignored", () => {
    const deps = makeMockVoicesDeps();
    const svc = new CcomidiVoicesService(deps);
    svc.start();
    deps.reset();

    svc.state(encodeURIComponent(JSON.stringify({
        slots: [{ program: 0, name: "Missing Type" }],
    })));

    assert.deepEqual(outletArgs(deps), []);
});

// ---- voice-family menu format --------------------------------------------

test("menu items carry voice-family tags from typeCode", () => {
    const deps = makeMockVoicesDeps();
    const svc = new CcomidiVoicesService(deps);
    svc.start();
    deps.reset();

    svc.state(encodedState([
        { program: 0,  name: "Direct",  typeCode: 0x00 },
        { program: 1,  name: "Sq1",     typeCode: 0x01 },
        { program: 2,  name: "Sq2",     typeCode: 0x02 },
        { program: 3,  name: "Wave",    typeCode: 0x03 },
        { program: 4,  name: "Noise",   typeCode: 0x04 },
        { program: 5,  name: "CryV",    typeCode: 0x20 },
        { program: 6,  name: "Split",   typeCode: 0x40, envelope: null },
        { program: 7,  name: "SplAll", typeCode: 0x80, envelope: null },
    ]));

    const appended = outletArgs(deps)
        .filter((a) => a[0] === "slots" && a[1] === "append")
        .map((a) => a[2]);
    assert.deepEqual(appended, [
        "[DS] Direct",
        "[Sq1] Sq1",
        "[Sq2] Sq2",
        "[Wav] Wave",
        "[Nse] Noise",
        "[Cry] CryV",
        "[Spl] Split",
        "[Spl*] SplAll",
    ]);
});
