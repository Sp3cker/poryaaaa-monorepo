import test from "node:test";
import assert from "node:assert/strict";

import {
    collectCcomidiStateViaLom,
    createRecorderService,
    RECORDER_DIR,
    type LiveApiFactory,
    type LiveApiLike,
    type ServiceDeps,
} from "../recorder/ccomidi_recorder";
import type { InitialCc, SmfWriter } from "../recorder/recorder_smf_writer";

interface OutletCall {
    outletIndex: number;
    args: unknown[];
}

// ---- deps factory ----------------------------------------------------------

function makeDeps(opts: { projectId?: string; smfSaveError?: Error; smfSaveResult?: boolean } = {}): {
    deps: ServiceDeps;
    outletCalls: OutletCall[];
    postCalls: string[];
    filenames: Map<string, string>;
    smfSaveCalls: number;
    afterSuccessfulSaveCalls: number;
    smfObservedSaves: Array<{ voicemap: Map<number, number>; initialCcs: InitialCc[] }>;
    // hook so a test can inspect the args passed to the SMF writer factory
    smfFactoryArgs: {
        voicemap: () => Map<number, number>;
        initialCcs: () => InitialCc[];
        outputPath: () => string;
        range: () => { start: string; length: string };
        markerRange: () => { start: string; end: string };
    } | null;
} {
    const outletCalls: OutletCall[] = [];
    const postCalls: string[] = [];
    const filenames = new Map<string, string>();
    const projectId = opts.projectId ?? "";
    let smfSaveCalls = 0;
    let afterSuccessfulSaveCalls = 0;
    let smfFactoryArgs: {
        voicemap: () => Map<number, number>;
        initialCcs: () => InitialCc[];
        outputPath: () => string;
        range: () => { start: string; length: string };
        markerRange: () => { start: string; end: string };
    } | null = null;
    const smfObservedSaves: Array<{ voicemap: Map<number, number>; initialCcs: InitialCc[] }> = [];

    const fakeSmfWriter: SmfWriter = {
        save: async () => {
            smfSaveCalls++;
            if (opts.smfSaveError) throw opts.smfSaveError;
            if (smfFactoryArgs) {
                smfObservedSaves.push({
                    voicemap: new Map(smfFactoryArgs.voicemap()),
                    initialCcs: smfFactoryArgs.initialCcs().slice(),
                });
            }
            return opts.smfSaveResult ?? true;
        },
    };

    const deps: ServiceDeps = {
        outlet: (outletIndex, ...args) => {
            outletCalls.push({ outletIndex, args });
        },
        post: (msg) => { postCalls.push(msg); },
        getProjectId: () => projectId,
        readFilename: (id) => filenames.get(id) ?? null,
        writeFilename: (id, filename) => { filenames.set(id, filename); },
        afterSuccessfulSave: () => { afterSuccessfulSaveCalls++; },
        collectCcomidiSnapshot: () => ({
            deviceCount: 0,
            voicemap: new Map(),
            initialCcs: [],
            failures: [],
        }),
        makeSmfWriter: (args) => {
            smfFactoryArgs = args;
            return fakeSmfWriter;
        },
    };

    return {
        deps,
        outletCalls,
        postCalls,
        filenames,
        smfObservedSaves,
        get smfSaveCalls() { return smfSaveCalls; },
        get afterSuccessfulSaveCalls() { return afterSuccessfulSaveCalls; },
        get smfFactoryArgs() { return smfFactoryArgs; },
    } as ReturnType<typeof makeDeps>;
}

// ---- ready -----------------------------------------------------------------

test("`ready` with no saved project logs and emits no outlet calls", () => {
    const { deps, outletCalls, postCalls } = makeDeps({ projectId: "" });
    const svc = createRecorderService(deps);
    svc.ready();
    assert.ok(postCalls.some((m) => m.includes("unsaved")));
    assert.equal(outletCalls.length, 0);
});

test("`ready` with project but no saved filename logs and emits no outlet calls", () => {
    const { deps, outletCalls, postCalls } = makeDeps({ projectId: "/path/to/set.als" });
    const svc = createRecorderService(deps);
    svc.ready();
    assert.equal(outletCalls.length, 0);
    assert.ok(postCalls.some((m) => m.includes("no saved filename")));
});

test("`ready` restores persisted filename and emits to UI textedit only (no setpath to external)", () => {
    const { deps, outletCalls, filenames } = makeDeps({ projectId: "/path/to/set.als" });
    filenames.set("/path/to/set.als", "mytrack.mid");
    const svc = createRecorderService(deps);
    svc.ready();

    // Only one outlet call: to the textedit (outlet 1). The external no
    // longer accepts setpath.
    assert.equal(outletCalls.length, 1);
    assert.equal(outletCalls[0].outletIndex, 1);
    assert.deepEqual(outletCalls[0].args, ["set", "mytrack.mid"]);
});

// ---- setFilename -----------------------------------------------------------

test("`setFilename` persists to disk when project is saved", () => {
    const { deps, filenames } = makeDeps({ projectId: "/path/to/set.als" });
    const svc = createRecorderService(deps);
    svc.setFilename("session.mid");
    assert.equal(filenames.get("/path/to/set.als"), "session.mid");
});

test("`setFilename` does NOT persist when project is unsaved", () => {
    const { deps, filenames } = makeDeps({ projectId: "" });
    const svc = createRecorderService(deps);
    svc.setFilename("session.mid");
    assert.equal(filenames.size, 0);
});

test("`setFilename` does NOT send any outlet messages (no external setpath anymore)", () => {
    const { deps, outletCalls } = makeDeps({ projectId: "/p/set.als" });
    const svc = createRecorderService(deps);
    svc.setFilename("foo.mid");
    assert.equal(outletCalls.length, 0);
});

test("`setFilename` trims whitespace", () => {
    const { deps, filenames } = makeDeps({ projectId: "/p/set.als" });
    const svc = createRecorderService(deps);
    svc.setFilename("  foo.mid  ");
    assert.equal(filenames.get("/p/set.als"), "foo.mid");
});

// ---- getFilename -----------------------------------------------------------

test("`getFilename` returns empty string before any filename is set", () => {
    const { deps } = makeDeps();
    const svc = createRecorderService(deps);
    assert.equal(svc.getFilename(), "");
});

test("`getFilename` returns the current filename after setFilename", () => {
    const { deps } = makeDeps({ projectId: "/p/set.als" });
    const svc = createRecorderService(deps);
    svc.setFilename("out.mid");
    assert.equal(svc.getFilename(), "out.mid");
});

// ---- LOM snapshot ----------------------------------------------------------

interface LomNode {
    props?: Record<string, unknown>;
    children?: Record<string, string[]>;
}

function makeLiveApiFactory(nodes: Record<string, LomNode>): LiveApiFactory {
    return (path: string): LiveApiLike => ({
        valid: nodes[path] ? 1 : 0,
        getcount: (child: string) => nodes[path]?.children?.[child]?.length ?? 0,
        get: (prop: string) => nodes[path]?.props?.[prop] ?? 0,
        getstring: (prop: string) => nodes[path]?.props?.[prop] ?? "",
    });
}

function addDevice(
    nodes: Record<string, LomNode>,
    path: string,
    props: Record<string, unknown>,
    params: Array<[string, number]>,
): void {
    nodes[path] = {
        props,
        children: {
            parameters: params.map((_, i) => `${path} parameters ${i}`),
        },
    };
    params.forEach(([name, value], i) => {
        nodes[`${path} parameters ${i}`] = { props: { name, value } };
    });
}

test("LOM snapshot: walks ccomidi devices and maps VIdx plus ccomidi shortname controls", () => {
    const nodes: Record<string, LomNode> = {
        "live_set": {
            children: {
                tracks: ["live_set tracks 0", "live_set tracks 1"],
            },
        },
        "live_set tracks 0": {
            props: {
                output_routing_channel: { display_name: "3", identifier: "3" },
            },
            children: {
                devices: ["live_set tracks 0 devices 0", "live_set tracks 0 devices 1"],
            },
        },
        "live_set tracks 1": {
            props: {
                output_routing_channel: { display_name: "6", identifier: "6" },
            },
            children: {
                devices: ["live_set tracks 1 devices 0"],
            },
        },
    };
    addDevice(nodes, "live_set tracks 0 devices 0", { name: "ccomidi" }, [
        ["VIdx", 45],
        ["Vol", 97],
        ["Pan", 64],
        ["Mod", 8],
        ["LFOSpd", 9],
        ["LFODly", 10],
        ["BndRng", 12],
        ["ModTyp", 2],
        ["Tune", -2],
        ["Echo", 22],
        ["EchoVol", 23],
        // Removed PRIO controls may still exist in old sets; snapshot ignores them.
        ["P21", 1],
        ["P27", 0],
    ]);
    addDevice(nodes, "live_set tracks 0 devices 1", { name: "Operator" }, [
        ["VIdx", 99],
        ["Vol", 1],
    ]);
    nodes["live_set tracks 1 devices 0"] = {
        props: { name: "Rack" },
        children: {
            parameters: [],
            chains: ["live_set tracks 1 devices 0 chains 0"],
        },
    };
    nodes["live_set tracks 1 devices 0 chains 0"] = {
        children: {
            devices: ["live_set tracks 1 devices 0 chains 0 devices 0"],
        },
    };
    addDevice(nodes, "live_set tracks 1 devices 0 chains 0 devices 0", { class_display_name: "ccomidi" }, [
        ["VIdx", 12],
        ["Vol", 70],
        ["Pan", 64],
        ["Mod", 0],
        ["Echo", 0],
        ["Tune", 0],
    ]);

    const snap = collectCcomidiStateViaLom(makeLiveApiFactory(nodes));

    assert.equal(snap.deviceCount, 2);
    assert.deepEqual([...snap.voicemap.entries()], [[2, 45], [5, 12]]);
    assert.deepEqual(snap.initialCcs, [
        { channel: 2, cc: 0x07, value: 97 },
        { channel: 2, cc: 0x0A, value: 64 },
        { channel: 2, cc: 0x01, value: 8 },
        { channel: 2, cc: 0x15, value: 9 },
        { channel: 2, cc: 0x1A, value: 10 },
        { channel: 2, cc: 0x14, value: 12 },
        { channel: 2, cc: 0x16, value: 2 },
        { channel: 2, cc: 0x18, value: 62 },
        { channel: 2, cc: 0x1E, value: 0x08 },
        { channel: 2, cc: 0x1D, value: 23 },
        { channel: 2, cc: 0x1E, value: 0x09 },
        { channel: 2, cc: 0x1D, value: 22 },
        { channel: 5, cc: 0x07, value: 70 },
        { channel: 5, cc: 0x0A, value: 64 },
    ]);
});

test("LOM snapshot: maps echo volume and length to the matching XCMD selectors", () => {
    const nodes: Record<string, LomNode> = {
        "live_set": {
            children: {
                tracks: ["live_set tracks 0"],
            },
        },
        "live_set tracks 0": {
            props: {
                output_routing_channel: { display_name: "1", identifier: "1" },
            },
            children: {
                devices: ["live_set tracks 0 devices 0"],
            },
        },
    };
    addDevice(nodes, "live_set tracks 0 devices 0", { name: "ccomidi" }, [
        ["VIdx", 12],
        ["Vol", 64],
        ["Pan", 64],
        ["EchoVol", 22],
        ["EchoLen", 23],
    ]);

    const snap = collectCcomidiStateViaLom(makeLiveApiFactory(nodes));

    assert.deepEqual(snap.initialCcs, [
        { channel: 0, cc: 0x07, value: 64 },
        { channel: 0, cc: 0x0A, value: 64 },
        { channel: 0, cc: 0x1E, value: 0x08 },
        { channel: 0, cc: 0x1D, value: 22 },
        { channel: 0, cc: 0x1E, value: 0x09 },
        { channel: 0, cc: 0x1D, value: 23 },
    ]);
});

test("LOM snapshot: logs required PC, Volume, and Pan per ccomidi", () => {
    const logs: string[] = [];
    const nodes: Record<string, LomNode> = {
        "live_set": {
            children: {
                tracks: ["live_set tracks 0"],
            },
        },
        "live_set tracks 0": {
            props: {
                output_routing_channel: { display_name: "3", identifier: "3" },
            },
            children: {
                devices: ["live_set tracks 0 devices 0"],
            },
        },
    };
    addDevice(nodes, "live_set tracks 0 devices 0", { name: "ccomidi" }, [
        ["VIdx", 45],
        ["Vol", 97],
        ["Pan", 64],
    ]);

    collectCcomidiStateViaLom(makeLiveApiFactory(nodes), (msg) => logs.push(msg));

    assert.ok(logs.some((m) =>
        m.includes("state live_set tracks 0 devices 0 ch3")
        && m.includes("PC=45")
        && m.includes("CC7=97")
        && m.includes("CC10=64")
    ));
    assert.ok(logs.some((m) =>
        m.includes("snapshot devices=1 programs=1 CCs=2 failures=0")
    ));
});

test("LOM snapshot: rejects long or prefixed parameter names not exposed by ccomidi LOM", () => {
    const nodes: Record<string, LomNode> = {
        "live_set": {
            children: {
                tracks: ["live_set tracks 0"],
            },
        },
        "live_set tracks 0": {
            props: {
                output_routing_channel: { display_name: "2", identifier: "2" },
            },
            children: {
                devices: ["live_set tracks 0 devices 0"],
            },
        },
    };
    addDevice(nodes, "live_set tracks 0 devices 0", { name: "ccomidi" }, [
        ["VoicePicker::VoiceIdx", 44],
        ["MainControls::Volume", 100],
        ["MainControls::Pan", 61],
        ["MainControls::Mod", 9],
        ["MainControls::LFO Speed", 12],
        ["MainControls::LFO Delay", 14],
        ["ExpressionControls::Bend Range", 4],
        ["ExpressionControls::Mod Type", 2],
        ["ExpressionControls::Tune", -1],
        ["ExpressionControls::Echo", 23],
        ["ExpressionControls::Echo Volume", 24],
        ["Priority 27", 1],
    ]);

    const snap = collectCcomidiStateViaLom(makeLiveApiFactory(nodes));

    assert.deepEqual([...snap.voicemap.entries()], []);
    assert.deepEqual(snap.initialCcs, []);
    assert.deepEqual(snap.failures, [{
        path: "live_set tracks 0 devices 0",
        reason: "missing or invalid VIdx parameter",
    }]);
});

test("LOM snapshot: accepts LiveAPI JSON-string routing dictionaries", () => {
    const nodes: Record<string, LomNode> = {
        "live_set": {
            children: {
                tracks: ["live_set tracks 0"],
            },
        },
        "live_set tracks 0": {
            props: {
                output_routing_channel: [
                    JSON.stringify({ display_name: "4", identifier: "4" }),
                ],
            },
            children: {
                devices: ["live_set tracks 0 devices 0"],
            },
        },
    };
    addDevice(nodes, "live_set tracks 0 devices 0", { name: "ccomidi" }, [
        ["VIdx", 22],
    ]);

    const snap = collectCcomidiStateViaLom(makeLiveApiFactory(nodes));

    assert.deepEqual([...snap.voicemap.entries()], [[3, 22]]);
});

test("LOM snapshot: accepts wrapped output_routing_channel with numeric identifier", () => {
    const nodes: Record<string, LomNode> = {
        "live_set": {
            children: {
                tracks: ["live_set tracks 0"],
            },
        },
        "live_set tracks 0": {
            props: {
                output_routing_channel: {
                    output_routing_channel: {
                        display_name: "3-poryaaaa",
                        identifier: 12,
                    },
                },
            },
            children: {
                devices: ["live_set tracks 0 devices 0"],
            },
        },
    };
    addDevice(nodes, "live_set tracks 0 devices 0", { name: "ccomidi" }, [
        ["VIdx", 31],
    ]);

    const snap = collectCcomidiStateViaLom(makeLiveApiFactory(nodes));

    assert.deepEqual([...snap.voicemap.entries()], [[2, 31]]);
    assert.deepEqual(snap.failures, []);
});

test("LOM snapshot: does not treat routing identifier as a MIDI channel", () => {
    const nodes: Record<string, LomNode> = {
        "live_set": {
            children: {
                tracks: ["live_set tracks 0"],
            },
        },
        "live_set tracks 0": {
            props: {
                output_routing_channel: {
                    display_name: "poryaaaa",
                    identifier: 11,
                },
            },
            children: {
                devices: ["live_set tracks 0 devices 0"],
            },
        },
    };
    addDevice(nodes, "live_set tracks 0 devices 0", { name: "ccomidi" }, [
        ["VIdx", 31],
    ]);

    const snap = collectCcomidiStateViaLom(makeLiveApiFactory(nodes));

    assert.deepEqual([...snap.voicemap.entries()], []);
    assert.equal(snap.failures.length, 1);
    assert.match(snap.failures[0].reason, /output_routing_channel is not a MIDI channel/);
});

test("LOM snapshot: resolves current routing through available channel identifiers", () => {
    const nodes: Record<string, LomNode> = {
        "live_set": {
            children: {
                tracks: ["live_set tracks 0"],
            },
        },
        "live_set tracks 0": {
            props: {
                output_routing_channel: {
                    display_name: "poryaaaa",
                    identifier: 11,
                },
                available_output_routing_channels: {
                    available_output_routing_channels: [
                        { display_name: "Ch. 1", identifier: 9 },
                        { display_name: "Ch. 2", identifier: 10 },
                        { display_name: "Ch. 3", identifier: 11 },
                    ],
                },
            },
            children: {
                devices: ["live_set tracks 0 devices 0"],
            },
        },
    };
    addDevice(nodes, "live_set tracks 0 devices 0", { name: "ccomidi" }, [
        ["VIdx", 31],
    ]);

    const snap = collectCcomidiStateViaLom(makeLiveApiFactory(nodes));

    assert.deepEqual([...snap.voicemap.entries()], [[2, 31]]);
    assert.deepEqual(snap.failures, []);
});

test("LOM snapshot: skips ccomidi devices whose routing channel is unavailable", () => {
    const nodes: Record<string, LomNode> = {
        "live_set": {
            children: {
                tracks: ["live_set tracks 0", "live_set tracks 1"],
            },
        },
        "live_set tracks 0": {
            props: {
                output_routing_channel: 0,
            },
            children: {
                devices: ["live_set tracks 0 devices 0"],
            },
        },
        "live_set tracks 1": {
            props: {
                output_routing_channel: { display_name: "2", identifier: "2" },
            },
            children: {
                devices: ["live_set tracks 1 devices 0"],
            },
        },
    };
    addDevice(nodes, "live_set tracks 0 devices 0", { name: "ccomidi" }, [
        ["VIdx", 11],
    ]);
    addDevice(nodes, "live_set tracks 1 devices 0", { name: "ccomidi" }, [
        ["VIdx", 44],
    ]);

    const snap = collectCcomidiStateViaLom(makeLiveApiFactory(nodes));

    assert.equal(snap.deviceCount, 2);
    assert.deepEqual([...snap.voicemap.entries()], [[1, 44]]);
    assert.equal(snap.failures.length, 1);
    assert.match(snap.failures[0].reason, /routing value is not a dictionary/);
});

test("save() tolerates an empty LOM snapshot", async () => {
    const harness = makeDeps({ projectId: "/p/set.als" });
    const svc = createRecorderService(harness.deps);
    svc.setFilename("out.mid");

    await svc.save();

    assert.equal(harness.smfSaveCalls, 1);
    assert.deepEqual([...harness.smfObservedSaves[0].voicemap.entries()], []);
    assert.deepEqual(harness.smfObservedSaves[0].initialCcs, []);
});

test("LOM snapshot: does not ask non-rack devices for chain children", () => {
    const getcountCalls: Array<{ path: string; child: string }> = [];
    const nodes: Record<string, LomNode> = {
        "live_set": {
            children: {
                tracks: ["live_set tracks 0"],
            },
        },
        "live_set tracks 0": {
            props: {
                output_routing_channel: { display_name: "1", identifier: "1" },
            },
            children: {
                devices: ["live_set tracks 0 devices 0"],
            },
        },
    };
    addDevice(nodes, "live_set tracks 0 devices 0", { name: "ccomidi" }, [
        ["VIdx", 9],
    ]);
    const makeApi: LiveApiFactory = (path: string): LiveApiLike => ({
        valid: nodes[path] ? 1 : 0,
        getcount: (child: string) => {
            getcountCalls.push({ path, child });
            return nodes[path]?.children?.[child]?.length ?? 0;
        },
        get: (prop: string) => nodes[path]?.props?.[prop] ?? 0,
        getstring: (prop: string) => nodes[path]?.props?.[prop] ?? "",
    });

    collectCcomidiStateViaLom(makeApi);

    assert.equal(
        getcountCalls.some((call) =>
            call.path === "live_set tracks 0 devices 0"
            && (call.child === "chains" || call.child === "return_chains")
        ),
        false,
    );
});

test("save() with LOM snapshot injects it into the writer", async () => {
    const harness = makeDeps({ projectId: "/p/set.als" });
    const deps: ServiceDeps = {
        ...harness.deps,
        collectCcomidiSnapshot: () => ({
            deviceCount: 1,
            voicemap: new Map([[4, 33]]),
            initialCcs: [{ channel: 4, cc: 7, value: 88 }],
            failures: [],
        }),
    };
    const svc = createRecorderService(deps);
    svc.setFilename("out.mid");
    await svc.save();

    assert.equal(harness.smfSaveCalls, 1);
    assert.deepEqual([...harness.smfObservedSaves[0].voicemap.entries()], [[4, 33]]);
    assert.deepEqual(harness.smfObservedSaves[0].initialCcs, [{ channel: 4, cc: 7, value: 88 }]);
});

// ---- save() ----------------------------------------------------------------

test("save() delegates to the injected SMF writer", async () => {
    const harness = makeDeps({ projectId: "/p/set.als" });
    const svc = createRecorderService(harness.deps);
    svc.setFilename("out.mid");
    await svc.save();
    assert.equal(harness.smfSaveCalls, 1);
});

test("save() runs successful-save hook after the SMF writer resolves", async () => {
    const harness = makeDeps({ projectId: "/p/set.als" });
    const svc = createRecorderService(harness.deps);
    svc.setFilename("out.mid");
    await svc.save();
    assert.equal(harness.afterSuccessfulSaveCalls, 1);
});

test("save() does not run successful-save hook when the SMF writer rejects", async () => {
    const harness = makeDeps({
        projectId: "/p/set.als",
        smfSaveError: new Error("write failed"),
    });
    const svc = createRecorderService(harness.deps);
    svc.setFilename("out.mid");
    await assert.rejects(() => svc.save(), /write failed/);
    assert.equal(harness.afterSuccessfulSaveCalls, 0);
});

test("save() does not run successful-save hook when the SMF writer reports failure", async () => {
    const harness = makeDeps({ projectId: "/p/set.als", smfSaveResult: false });
    const svc = createRecorderService(harness.deps);
    svc.setFilename("out.mid");
    await svc.save();
    assert.equal(harness.afterSuccessfulSaveCalls, 0);
});

test("outputPath resolves bare filenames under RECORDER_DIR", () => {
    const harness = makeDeps({ projectId: "/p/set.als" });
    const svc = createRecorderService(harness.deps);
    svc.setFilename("out.mid");
    assert.equal(harness.smfFactoryArgs!.outputPath(), RECORDER_DIR + "out.mid");
});

test("outputPath keeps absolute paths intact", () => {
    const harness = makeDeps({ projectId: "/p/set.als" });
    const svc = createRecorderService(harness.deps);
    svc.setFilename("/Users/me/out.mid");
    assert.equal(harness.smfFactoryArgs!.outputPath(), "/Users/me/out.mid");
});
