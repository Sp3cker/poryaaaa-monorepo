// V8 entry point. Loaded as [v8 ccomidi_recorder.js] in devices/poryaaaa.amxd.
//
// Responsibilities:
//   - filename persistence (per project, keyed on Song.file_path)
//   - save-time ccomidi LOM snapshots for tick-0 VIdx and CC state
//   - drive the Save flow: ask poryaaaa~ to dump its MidiBuffer to a temp
//     file, then build the SMF in JS via recorder_smf_writer.ts and write
//     it to the user-chosen path
//
// The C++ external no longer knows about ticks, tempo, loop, voicemap, or
// SMF format. JS owns all of that.

import type { MaxAtom } from "../max-events";
import {
    createSmfWriter,
    type InitialCc,
    type LiveApiAdapter,
    type MidiEvent,
    type SmfWriter,
} from "./recorder_smf_writer";
import {parseDictLike} from '../ccomidi_routing_dicts'
export const RECORDER_DIR = "~/Music/poryaaaa-recordings/";

// Persisted export-range strings (raw user input from the device's Start/Length
// textedit fields). Stored as strings so we don't lose format and so we can
// apply the current values at save time.
export interface RangeStrings {
    start:  string;
    length: string;
}

export interface MarkerStrings {
    start: string;
    end:   string;
}

export interface CcomidiLomSnapshot {
    voicemap:    Map<number, number>;
    initialCcs:  InitialCc[];
    deviceCount: number;
    failures:    Array<{ path: string; reason: string }>;
}

export interface LiveApiLike {
    valid?: unknown;
    getcount:  (child: string) => number;
    get:       (prop: string) => unknown;
    getstring: (prop: string) => unknown;
}

export type LiveApiFactory = (path: string) => LiveApiLike;
export type LomSnapshotLogger = (msg: string) => void;

// Dependency surface injected by tests / Max runtime.
export interface ServiceDeps {
    outlet:       (outletIndex: number, ...args: unknown[]) => void;
    post:         (msg: string) => void;
    getProjectId: () => string;
    readFilename: (projectId: string) => string | null;
    writeFilename:(projectId: string, filename: string) => void;
    // Optional persistence for the Start/Length and marker fields. Tests can
    // omit these; omitting keeps the values in-memory only.
    readRange?:   (projectId: string) => RangeStrings | null;
    writeRange?:  (projectId: string, range: RangeStrings) => void;
    readMarkers?: (projectId: string) => MarkerStrings | null;
    writeMarkers?:(projectId: string, markers: MarkerStrings) => void;
    afterSuccessfulSave?: () => void;
    // Save-time LOM snapshot. Max runtime uses this to collect the current
    // ccomidi VIdx and CC parameter values directly from Live's device graph
    // before SMF validation.
    collectCcomidiSnapshot: () => CcomidiLomSnapshot;
    // Factory hook so the Max runtime can wire concrete dump/IO impls while
    // tests inject mocks.
    makeSmfWriter:(args: {
        voicemap:    () => Map<number, number>;
        initialCcs:  () => InitialCc[];
        outputPath:  () => string;
        range:       () => RangeStrings;
        markerRange: () => MarkerStrings;
    }) => SmfWriter;
}

export interface RecorderService {
    ready:         () => void;
    setFilename:   (filename: string) => void;
    getFilename:   () => string;
    setStartBeat:  (s: string) => void;
    setLengthBeats:(s: string) => void;
    setLoopStart:  (s: string) => void;
    setLoopEnd:    (s: string) => void;
    getStartBeat:  () => string;
    getLengthBeats:() => string;
    resetStatus:   () => void;
    save:          () => Promise<void>;
}

// ---- Exported testable helpers --------------------------------------------
//
// These were originally closures inside installMaxHandlers(); extracted as
// top-level functions so the unit tests can exercise the PRBY-v1 binary
// format and the dump handshake without spinning up Max.

// Subset of Max's `File` API that readBufferFile actually uses.
export interface BinaryReader {
    isopen:        boolean;
    byteorder:     "little" | "big";
    readbytes:     (count: number) => number[];
    readfloat64:   (count: number) => number[];
    close:         () => void;
}

export function readBufferFileWith(
    path:   string,
    openReader: (path: string) => BinaryReader,
): MidiEvent[] {
    const f = openReader(path);
    if (!f.isopen) {
        throw new Error(`cannot open ${path}`);
    }
    try {
        f.byteorder = "little";
        const magic = f.readbytes(4);
        if (magic.length !== 4
            || magic[0] !== 0x50 || magic[1] !== 0x52
            || magic[2] !== 0x42 || magic[3] !== 0x59) {
            throw new Error(`${path}: bad magic`);
        }
        const versionBytes = f.readbytes(2);
        const version = versionBytes[0] | (versionBytes[1] << 8);
        if (version !== 1) {
            throw new Error(`${path}: unsupported version ${version}`);
        }
        f.readbytes(2);                          // reserved
        const cb = f.readbytes(8);
        let count = 0;
        for (let i = 7; i >= 0; i--) {
            count = count * 256 + cb[i];
        }
        const events: MidiEvent[] = new Array(count);
        for (let i = 0; i < count; i++) {
            const beatsArr = f.readfloat64(1);
            const tail     = f.readbytes(4);     // status, d1, d2, pad
            events[i] = {
                beats:  beatsArr[0],
                status: tail[0],
                d1:     tail[1],
                d2:     tail[2],
            };
        }
        return events;
    } finally {
        f.close();
    }
}

// Dump-handshake factory. Owns the `pendingDump` slot so it's not leaked
// into module scope. Tests construct one with a spy outlet and drive
// requestBufferDump + dumped manually.
export interface DumpHandshake {
    requestBufferDump: () => Promise<{ path: string; count: number }>;
    dumped:            (path: string, count: number) => void;
    dumpfailed:        (path: string, reason: string) => void;
    isPending:         () => boolean;
}

export function createDumpHandshake(opts: {
    outlet: (idx: number, ...args: unknown[]) => void;
    tempPath: () => string;
    post: (msg: string) => void;
    timeoutMs?: number;
}): DumpHandshake {
    let pending: {
        path:    string;
        resolve: (v: { path: string; count: number }) => void;
        reject:  (e: unknown) => void;
        timer:   number | null;
    } | null = null;
    const timeoutMs = opts.timeoutMs ?? 1500;
    const timers = globalThis as unknown as {
        setTimeout?: (handler: () => void, timeout: number) => number;
        clearTimeout?: (id: number) => void;
    };

    function clearPendingTimer(expected: { timer: number | null }): void {
        if (expected.timer !== null && typeof timers.clearTimeout === "function") {
            timers.clearTimeout(expected.timer);
        }
    }

    function displayDumpReason(reason: string): string {
        switch (reason) {
            case "nothing_recorded":      return "nothing recorded";
            case "export_not_detected":  return "export not detected";
            case "bad_path":             return "bad dump path";
            case "write_failed":         return "dump write failed";
            default:                     return reason || "dump failed";
        }
    }

    return {
        requestBufferDump() {
            if (pending) {
                return Promise.reject(new Error("dump already in flight"));
            }
            const path = opts.tempPath();
            return new Promise((resolve, reject) => {
                let timer: number | null = null;
                if (typeof timers.setTimeout === "function") {
                    timer = timers.setTimeout(() => {
                        if (!pending || pending.path !== path) return;
                        pending = null;
                        opts.post(`recorder: dump timed out waiting for poryaaaa~ reply path=${path}\n`);
                        reject(new Error(`dump timed out waiting for poryaaaa~ reply: ${path}`));
                    }, timeoutMs);
                } else {
                    opts.post("recorder: setTimeout unavailable in this v8 runtime; dump timeout disabled\n");
                }
                pending = { path, resolve, reject, timer };
                opts.post(`recorder: requesting dump from poryaaaa~ path=${path}\n`);
                opts.outlet(0, "dump", path);
            });
        },
        dumped(path, count) {
            if (!pending) {
                opts.post("recorder: ignoring unexpected dumped reply\n");
                return;
            }
            const expected = pending;
            pending = null;
            clearPendingTimer(expected);
            if (path !== expected.path) {
                expected.reject(
                    new Error(`path mismatch: expected ${expected.path}, got ${path}`));
                return;
            }
            if (count <= 0) {
                expected.reject(new Error("nothing recorded"));
                return;
            }
            expected.resolve({ path, count });
        },
        dumpfailed(path, reason) {
            if (!pending) {
                opts.post("recorder: ignoring unexpected dumpfailed reply\n");
                return;
            }
            const expected = pending;
            pending = null;
            clearPendingTimer(expected);
            if (path && path !== expected.path) {
                expected.reject(
                    new Error(`path mismatch: expected ${expected.path}, got ${path}`));
                return;
            }
            expected.reject(new Error(displayDumpReason(reason)));
        },
        isPending() { return pending !== null; },
    };
}

// ---- ccomidi LOM snapshot -------------------------------------------------

function clampInt(value: unknown, lo: number, hi: number): number | null {
    if (value === null || value === undefined || value === "") return null;
    const n = typeof value === "number" ? value : Number(value);
    if (!Number.isFinite(n)) return null;
    const v = Math.floor(n);
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

function normalizeName(name: string): string {
    return name.toLowerCase().replace(/[^a-z0-9]/g, "");
}

function getStringProp(api: LiveApiLike, prop: string): string {
    try {
        const value = api.getstring(prop);
        if (Array.isArray(value)) return value.map(String).join(" ");
        if (value !== null && value !== undefined) return String(value);
    } catch (_) { /* not every LOM object exposes every string prop */ }
    return "";
}

function getNumberProp(api: LiveApiLike, prop: string): number | null {
    try {
        const value = api.get(prop);
        const first = Array.isArray(value) ? value[0] : value;
        const n = typeof first === "number" ? first : Number(first);
        return Number.isFinite(n) ? n : null;
    } catch (_) {
        return null;
    }
}

function safeGetCount(api: LiveApiLike, child: string): number {
    try {
        const n = api.getcount(child);
        return Number.isFinite(n) && n > 0 ? Math.floor(n) : 0;
    } catch (_) {
        return 0;
    }
}

function isValid(api: LiveApiLike): boolean {
    const v = api.valid;
    return v === undefined || v === true || v === 1;
}

interface RoutingChoice {
    display_name: string;
    identifier: string | number;
}


function routingChoice(value: unknown): RoutingChoice {
    const parsed = parseDictLike(value);
    if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) {
        throw new Error("routing value is not a dictionary");
    }
    const wrapped = (parsed as Record<string, unknown>).output_routing_channel;
    if (wrapped) return routingChoice(wrapped);
    const display = (parsed as Record<string, unknown>).display_name;
    const identifier = (parsed as Record<string, unknown>).identifier;
    if (typeof display !== "string"
        || (typeof identifier !== "string" && typeof identifier !== "number")) {
        throw new Error("routing dictionary missing display_name/identifier");
    }
    return { display_name: display, identifier };
}

function routingChoices(value: unknown, key: string): RoutingChoice[] {
    const parsed = parseDictLike(value);
    if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) {
        throw new Error(`${key} must be the dictionary returned by track.get("${key}")`);
    }
    const list = (parsed as Record<string, unknown>)[key];
    if (!Array.isArray(list)) throw new Error(`${key} dictionary missing ${key} list`);
    return list.map(routingChoice);
}

function parseMidiChannel(choice: RoutingChoice): number | null {
    const text = choice.display_name;
    if (/track\s*in/i.test(text)) return null;
    const match = text.match(/(?:^|[^0-9])(?:ch(?:annel)?\.?\s*)?([1-9]|1[0-6])(?:[^0-9]|$)/i);
    if (!match) return null;
    return Number(match[1]) - 1;
}

function containingTrackPath(devicePath: string): string {
    const match = devicePath.match(/^(live_set tracks \d+)(?:\s|$)/);
    if (!match) throw new Error(`cannot derive track path from ${devicePath}`);
    return match[1];
}

function routingChannelForDevice(makeApi: LiveApiFactory, devicePath: string): number {
    const trackPath = containingTrackPath(devicePath);
    const track = makeApi(trackPath);
    if (!isValid(track)) throw new Error(`${trackPath} is not valid`);
    const choice = routingChoice(track.get("output_routing_channel"));

    let channel: number | null = null;
    try {
        const available = routingChoices(
            track.get("available_output_routing_channels"),
            "available_output_routing_channels",
        );
        const matched = available.find((candidate) =>
            String(candidate.identifier) === String(choice.identifier)
        );
        if (matched) channel = parseMidiChannel(matched);
    } catch (_) {
        // Older tests and some Live states only expose the current routing
        // channel. Fall back to the current display name, never the identifier.
    }

    if (channel === null) channel = parseMidiChannel(choice);
    if (channel === null) {
        throw new Error(`${trackPath} output_routing_channel is not a MIDI channel`);
    }
    return channel;
}

function parameterMap(makeApi: LiveApiFactory, devicePath: string): Map<string, number> {
    const device = makeApi(devicePath);
    const count = safeGetCount(device, "parameters");
    const out = new Map<string, number>();
    for (let i = 0; i < count; i++) {
        const param = makeApi(`${devicePath} parameters ${i}`);
        if (!isValid(param)) continue;
        const value = getNumberProp(param, "value");
        if (value === null) continue;
        for (const prop of ["name", "original_name"]) {
            const name = getStringProp(param, prop);
            if (!name) continue;
            const key = normalizeName(name);
            if (!out.has(key)) out.set(key, value);
        }
    }
    return out;
}

interface ParamDebugEntry {
    index: number;
    name: string;
    originalName: string;
    value: number | null;
}

function parameterDebugEntries(makeApi: LiveApiFactory, devicePath: string): ParamDebugEntry[] {
    const device = makeApi(devicePath);
    const count = safeGetCount(device, "parameters");
    const out: ParamDebugEntry[] = [];
    for (let i = 0; i < count; i++) {
        const param = makeApi(`${devicePath} parameters ${i}`);
        if (!isValid(param)) continue;
        out.push({
            index:        i,
            name:         getStringProp(param, "name"),
            originalName: getStringProp(param, "original_name"),
            value:        getNumberProp(param, "value"),
        });
    }
    return out;
}

function logDeviceNode(makeApi: LiveApiFactory, devicePath: string, log?: LomSnapshotLogger): void {
    if (!log) return;
    const device = makeApi(devicePath);
    log(
        `recorder: LOM ccomidi node path=${devicePath} `
        + `name="${getStringProp(device, "name")}" `
        + `class_display_name="${getStringProp(device, "class_display_name")}" `
        + `class_name="${getStringProp(device, "class_name")}"\n`,
    );
    const params = parameterDebugEntries(makeApi, devicePath);
    if (params.length === 0) {
        log(`recorder: LOM ccomidi params ${devicePath}: <none>\n`);
        return;
    }
    for (const param of params) {
        log(
            `recorder: LOM ccomidi param ${devicePath} #${param.index} `
            + `name="${param.name}" original_name="${param.originalName}" `
            + `value=${param.value === null ? "<unreadable>" : param.value}\n`,
        );
    }
}

function findParam(params: Map<string, number>, name: string): number | null {
    const value = params.get(normalizeName(name));
    return value === undefined ? null : value;
}

function pushCc(out: InitialCc[], channel: number, cc: number, value: number): void {
    const c = clampInt(cc, 0, 127);
    const v = clampInt(value, 0, 127);
    if (c === null || v === null) return;
    out.push({ channel, cc: c, value: v });
}

function pushOptionalCc(
    out: InitialCc[],
    channel: number,
    cc: number,
    value: number | null,
    neutral = 0,
): void {
    if (value === null || value === neutral) return;
    pushCc(out, channel, cc, value);
}

function pushOptionalXcmd(
    out: InitialCc[],
    channel: number,
    selector: number,
    value: number | null,
    neutral = 0,
): void {
    if (value === null || value === neutral) return;
    pushCc(out, channel, 0x1E, selector);
    pushCc(out, channel, 0x1D, value);
}

function ccomidiDeviceState(params: Map<string, number>, channel: number): { program: number; ccs: InitialCc[] } | null {
    const program = clampInt(findParam(params, "VIdx"), 0, 127);
    if (program === null) return null;

    const ccs: InitialCc[] = [];
    const volume = findParam(params, "Vol");
    const pan = findParam(params, "Pan");
    if (volume !== null) pushCc(ccs, channel, 0x07, volume);
    if (pan !== null) pushCc(ccs, channel, 0x0A, pan);

    pushOptionalCc(ccs, channel, 0x01, findParam(params, "Mod"));
    pushOptionalCc(ccs, channel, 0x15, findParam(params, "LFOSpd"));
    pushOptionalCc(ccs, channel, 0x1A, findParam(params, "LFODly"));
    pushOptionalCc(ccs, channel, 0x14, findParam(params, "BndRng"));
    pushOptionalCc(ccs, channel, 0x16, findParam(params, "ModTyp"));
    const tuneSigned = clampInt(findParam(params, "Tune"), -64, 63);
    if (tuneSigned !== null && tuneSigned !== 0) pushCc(ccs, channel, 0x18, tuneSigned + 64);
    pushOptionalXcmd(ccs, channel, 0x08, findParam(params, "EchoVol"));
    pushOptionalXcmd(ccs, channel, 0x09, findParam(params, "EchoLen") ?? findParam(params, "Echo"));

    return { program, ccs };
}

function logFoundState(
    log: LomSnapshotLogger,
    devicePath: string,
    channel: number,
    state: { program: number; ccs: InitialCc[] },
): void {
    // Look up the current value for a specific CC (by number) from the list
    // of initial CCs we captured via LOM at save time. Used only for the
    // human-readable status log of what ccomidi state will be written.
    const volume = (() => {
        const found = state.ccs.find((entry) => entry.cc === 0x07);
        return found ? found.value : null;
    })();
    const pan = (() => {
        const found = state.ccs.find((entry) => entry.cc === 0x0A);
        return found ? found.value : null;
    })();
    log(
        `recorder: LOM ccomidi state ${devicePath} `
        + `ch${channel + 1} PC=${state.program} `
        + `CC7=${volume === null ? "<missing>" : volume} `
        + `CC10=${pan === null ? "<missing>" : pan} `
        + `CCs=${state.ccs.length}\n`,
    );
}

function logSnapshotSummary(
    log: LomSnapshotLogger,
    snap: CcomidiLomSnapshot,
): void {
    log(
        `recorder: LOM ccomidi snapshot devices=${snap.deviceCount} `
        + `programs=${snap.voicemap.size} CCs=${snap.initialCcs.length} `
        + `failures=${snap.failures.length}\n`,
    );
}

function isCcomidiDevice(api: LiveApiLike): boolean {
    const haystack = [
        getStringProp(api, "name"),
        getStringProp(api, "class_display_name"),
        getStringProp(api, "class_name"),
    ].join(" ").toLowerCase();
    return haystack.includes("ccomidi");
}

function isRackDevice(api: LiveApiLike): boolean {
    const haystack = [
        getStringProp(api, "name"),
        getStringProp(api, "class_display_name"),
        getStringProp(api, "class_name"),
    ].join(" ").toLowerCase();
    return haystack.includes("rack");
}

function collectDevicePaths(makeApi: LiveApiFactory, containerPath: string, childName: string, out: string[]): void {
    const container = makeApi(containerPath);
    const count = safeGetCount(container, childName);
    for (let i = 0; i < count; i++) {
        walkDevice(makeApi, `${containerPath} ${childName} ${i}`, out);
    }
}

function walkDevice(makeApi: LiveApiFactory, devicePath: string, out: string[]): void {
    const device = makeApi(devicePath);
    if (!isValid(device)) return;
    if (isCcomidiDevice(device)) out.push(devicePath);
    if (!isRackDevice(device)) return;

    for (const childName of ["chains", "return_chains"]) {
        const chainCount = safeGetCount(device, childName);
        for (let i = 0; i < chainCount; i++) {
            collectDevicePaths(makeApi, `${devicePath} ${childName} ${i}`, "devices", out);
        }
    }
}

export function collectCcomidiStateViaLom(
    makeApi: LiveApiFactory,
    log?: LomSnapshotLogger,
): CcomidiLomSnapshot {
    const devicePaths: string[] = [];
    const liveSet = makeApi("live_set");
    const trackCount = safeGetCount(liveSet, "tracks");
    for (let trackIndex = 0; trackIndex < trackCount; trackIndex++) {
        collectDevicePaths(makeApi, `live_set tracks ${trackIndex}`, "devices", devicePaths);
    }

    const voicemap = new Map<number, number>();
    const ccsByChannel = new Map<number, InitialCc[]>();
    const failures: CcomidiLomSnapshot["failures"] = [];
    for (const devicePath of devicePaths) {
        logDeviceNode(makeApi, devicePath, log);
        const channel = (() => {
            try {
                return routingChannelForDevice(makeApi, devicePath);
            } catch (e) {
                const reason = String(e);
                failures.push({ path: devicePath, reason });
                if (log) log(`recorder: LOM ccomidi skipped ${devicePath}: ${reason}\n`);
                return null;
            }
        })();
        if (channel === null) continue;
        const state = ccomidiDeviceState(parameterMap(makeApi, devicePath), channel);
        if (!state) {
            failures.push({ path: devicePath, reason: "missing or invalid VIdx parameter" });
            if (log) log(`recorder: LOM ccomidi state ${devicePath} ch${channel + 1} missing VIdx\n`);
            continue;
        }
        if (log) logFoundState(log, devicePath, channel, state);
        voicemap.set(channel, state.program);
        if (ccsByChannel.has(channel)) ccsByChannel.delete(channel);
        ccsByChannel.set(channel, state.ccs);
    }

    const initialCcs: InitialCc[] = [];
    for (const ccs of ccsByChannel.values()) initialCcs.push(...ccs);
    const snap = { voicemap, initialCcs, deviceCount: devicePaths.length, failures };
    if (log) logSnapshotSummary(log, snap);
    return snap;
}

// ---- Service factory -------------------------------------------------------

export function createRecorderService(deps: ServiceDeps): RecorderService {
    let currentFilename = "";
    let currentStartBeat = "";
    let currentLengthBeats = "";
    let currentLoopStart = "";
    let currentLoopEnd = "";
    let activeSnapshot: CcomidiLomSnapshot | null = null;

    function persistRange(): void {
        const projectId = deps.getProjectId();
        if (!projectId || !deps.writeRange) return;
        deps.writeRange(projectId, {
            start:  currentStartBeat,
            length: currentLengthBeats,
        });
    }

    function persistMarkers(): void {
        const projectId = deps.getProjectId();
        if (!projectId || !deps.writeMarkers) return;
        deps.writeMarkers(projectId, {
            start: currentLoopStart,
            end:   currentLoopEnd,
        });
    }

    const smfWriter = deps.makeSmfWriter({
        voicemap:   () => activeSnapshot?.voicemap ?? new Map(),
        initialCcs: () => activeSnapshot?.initialCcs ?? [],
        // Resolve the user-provided filename: keep absolute or ~-prefixed paths
        // as-is; otherwise place bare names under the standard recordings directory.
        outputPath: () => currentFilename
            ? (currentFilename.startsWith("/") || currentFilename.startsWith("~")
                ? currentFilename
                : RECORDER_DIR + currentFilename)
            : "",
        range:      () => ({ start: currentStartBeat, length: currentLengthBeats }),
        markerRange:() => ({ start: currentLoopStart, end: currentLoopEnd }),
    });

    return {
        ready() {
            const projectId = deps.getProjectId();
            if (!projectId) {
                deps.post("ccomidi_recorder: Live Set unsaved, no filename to restore\n");
                return;
            }
            const saved = deps.readFilename(projectId);
            if (saved) {
                currentFilename = saved;
                deps.post(`ccomidi_recorder: restored filename "${currentFilename}"\n`);
                deps.outlet(1, "set", currentFilename);
            } else {
                deps.post(`ccomidi_recorder: no saved filename for project "${projectId}"\n`);
            }
            // Restore Start/Length from persistence, if available. Independent
            // of the filename — a saved range is useful even without one.
            if (deps.readRange) {
                const r = deps.readRange(projectId);
                if (r) {
                    currentStartBeat   = r.start  ?? "";
                    currentLengthBeats = r.length ?? "";
                    if (currentStartBeat)   deps.outlet(3, "set", currentStartBeat);
                    if (currentLengthBeats) deps.outlet(4, "set", currentLengthBeats);
                }
            }
            if (deps.readMarkers) {
                const r = deps.readMarkers(projectId);
                if (r) {
                    currentLoopStart = r.start ?? "";
                    currentLoopEnd   = r.end   ?? "";
                    if (currentLoopStart) deps.outlet(5, "set", currentLoopStart);
                    if (currentLoopEnd)   deps.outlet(6, "set", currentLoopEnd);
                }
            }
        },

        setFilename(filename) {
            currentFilename = filename.trim();
            if (!currentFilename) {
                deps.post("ccomidi_recorder: filename cleared\n");
                return;
            }
            const projectId = deps.getProjectId();
            if (projectId) {
                deps.writeFilename(projectId, currentFilename);
                deps.post(`ccomidi_recorder: filename "${currentFilename}" saved to project state\n`);
            } else {
                deps.post(`ccomidi_recorder: filename "${currentFilename}" set (Live Set unsaved — held in memory only)\n`);
            }
        },

        getFilename() {
            return currentFilename;
        },

        setStartBeat(s) {
            currentStartBeat = s.trim();
            persistRange();
        },

        setLengthBeats(s) {
            currentLengthBeats = s.trim();
            persistRange();
        },

        setLoopStart(s) {
            currentLoopStart = s.trim();
            persistMarkers();
        },

        setLoopEnd(s) {
            currentLoopEnd = s.trim();
            persistMarkers();
        },

        getStartBeat() { return currentStartBeat; },
        getLengthBeats() { return currentLengthBeats; },

        resetStatus() {
            activeSnapshot = null;
            deps.outlet(2, "set", "Ready");
        },

        async save() {
            activeSnapshot = deps.collectCcomidiSnapshot();
            try {
                const saved = await smfWriter.save();
                if (saved) deps.afterSuccessfulSave?.();
            } finally {
                activeSnapshot = null;
            }
        },
    };
}

// ---- Max runtime wiring ----------------------------------------------------

function installMaxHandlers(): void {
    inlets = 1;
    // outlet 0: messages routed through the patcher's [route unlink].
    //   - `dump <path>`   → falls through [route unlink]'s unmatched outlet
    //                       into [poryaaaa~]
    //   - `unlink <path>` → matched by [route unlink] and forwarded to the
    //                       [node.script cleanup.js] sidecar, which actually
    //                       removes the temp file from disk.
    // outlet 1: messages to filename textedit (`set <name>`).
    // outlet 2: save status indicator (`set <text>` for the [comment] in
    //           presentation mode — fired at every save-flow transition so
    //           the user sees Saving.../Saved.../FAILED... without opening
    //           the Max console).
    // outlet 3: Start textedit (`set <beat>` to restore the persisted value
    //           on device load).
    // outlet 4: Length textedit (same — restores persisted beat count).
    // outlet 5: Loop Start textedit.
    // outlet 6: Loop End textedit.
    outlets = 7;

    // ---- disk-backed persistence ------------------------------------------
    const STATE_PATHS_BY_OS: Record<string, string> = {
        macintosh: "~/Library/Application Support/poryaaaa/projects.json",
        windows: "~/AppData/Roaming/poryaaaa/projects.json",
    };

    interface ProjectStateFile {
        [projectId: string]: {
            root?:             string;
            bank?:             string;
            recorderFilename?: string;
            recorderStart?:    string;
            recorderLength?:   string;
            recorderLoopStart?:string;
            recorderLoopEnd?:  string;
        };
    }

    function statePath(): string | null {
        return STATE_PATHS_BY_OS[max.os] || null;
    }

    function readFileString(path: string): string | null {
        const f = new File(path, "read");
        if (!f.isopen) return null;
        try {
            const len = f.eof || 1 << 20;
            return f.readstring(len);
        } finally {
            f.close();
        }
    }

    function writeFileString(path: string, payload: string): void {
        const f = new File(path, "write");
        if (!f.isopen) {
            post(`ccomidi_recorder: could not open ${path} for write\n`);
            return;
        }
        try {
            f.writestring(payload);
        } finally {
            f.close();
        }
    }

    function readAllStates(): ProjectStateFile {
        const path = statePath();
        if (!path) return {};
        const raw = readFileString(path);
        if (!raw) return {};
        try {
            const parsed = JSON.parse(raw);
            if (!parsed || typeof parsed !== "object") return {};
            return parsed as ProjectStateFile;
        } catch (_) {
            return {};
        }
    }

    function writeAllStates(states: ProjectStateFile): void {
        const path = statePath();
        if (!path) return;
        const previous = readFileString(path);
        let payload = JSON.stringify(states, null, 2) + "\n";
        if (previous && payload.length < previous.length) {
            payload += " ".repeat(previous.length - payload.length);
        }
        writeFileString(path, payload);
    }

    function getProjectId(): string {
        try {
            const liveSet = new LiveAPI(null, "live_set");
            const v = liveSet.get("file_path");
            if (Array.isArray(v) && v.length > 0 && typeof v[0] === "string") return v[0];
            if (typeof v === "string") return v;
            return "";
        } catch (_) {
            return "";
        }
    }

    function readFilename(projectId: string): string | null {
        const all = readAllStates();
        const entry = all[projectId];
        if (!entry || typeof entry !== "object") return null;
        return typeof entry.recorderFilename === "string" ? entry.recorderFilename : null;
    }

    function writeFilename(projectId: string, filename: string): void {
        const all = readAllStates();
        if (!all[projectId]) all[projectId] = {};
        all[projectId].recorderFilename = filename;
        writeAllStates(all);
    }

    function readRange(projectId: string): RangeStrings | null {
        const all = readAllStates();
        const entry = all[projectId];
        if (!entry || typeof entry !== "object") return null;
        const start  = typeof entry.recorderStart  === "string" ? entry.recorderStart  : "";
        const length = typeof entry.recorderLength === "string" ? entry.recorderLength : "";
        if (!start && !length) return null;
        return { start, length };
    }

    function writeRange(projectId: string, range: RangeStrings): void {
        const all = readAllStates();
        if (!all[projectId]) all[projectId] = {};
        all[projectId].recorderStart  = range.start;
        all[projectId].recorderLength = range.length;
        writeAllStates(all);
    }

    function readMarkers(projectId: string): MarkerStrings | null {
        const all = readAllStates();
        const entry = all[projectId];
        if (!entry || typeof entry !== "object") return null;
        const start = typeof entry.recorderLoopStart === "string" ? entry.recorderLoopStart : "";
        const end   = typeof entry.recorderLoopEnd   === "string" ? entry.recorderLoopEnd   : "";
        if (!start && !end) return null;
        return { start, end };
    }

    function writeMarkers(projectId: string, markers: MarkerStrings): void {
        const all = readAllStates();
        if (!all[projectId]) all[projectId] = {};
        all[projectId].recorderLoopStart = markers.start;
        all[projectId].recorderLoopEnd   = markers.end;
        writeAllStates(all);
    }

    // ---- LiveAPI adapter ---------------------------------------------------

    const liveApi: LiveApiAdapter = {
        getTempo() {
            try {
                const ls = new LiveAPI(null, "live_set");
                const v = ls.get("tempo");
                return Array.isArray(v) ? Number(v[0]) : Number(v);
            } catch (_) { return 120.0; }
        },
        getLoop() {
            try {
                const ls = new LiveAPI(null, "live_set");
                const num = (v: unknown): number => {
                    if (Array.isArray(v)) return Number(v[0]) || 0;
                    return Number(v) || 0;
                };
                return {
                    on:     num(ls.get("loop")) !== 0,
                    start:  num(ls.get("loop_start")),
                    length: num(ls.get("loop_length")),
                };
            } catch (_) {
                return { on: false, start: 0, length: 0 };
            }
        },
        getTimeSig() {
            try {
                const ls = new LiveAPI(null, "live_set");
                const num = (v: unknown): number => {
                    if (Array.isArray(v)) return Number(v[0]) || 0;
                    return Number(v) || 0;
                };
                const n = num(ls.get("signature_numerator")) || 4;
                const d = num(ls.get("signature_denominator")) || 4;
                return { num: n, den: d };
            } catch (_) {
                return { num: 4, den: 4 };
            }
        },
    };

    // ---- temp-file path + dump handshake -----------------------------------

    function tempPath(): string {
        // Max's [v8] doesn't expose pid; use Date.now() + a random suffix.
        const stamp = Date.now();
        const rand  = Math.floor(Math.random() * 1e6);
        return `/tmp/poryaaaa-${stamp}-${rand}.bin`;
    }

    // One in-flight dump at a time. Save is user-driven (single click), so a
    // queue isn't justified. If the user double-clicks Save and the previous
    // dump hasn't replied yet, the second click no-ops with a warning.
    const handshake = createDumpHandshake({
        outlet:   (idx, ...args) => outlet(idx, ...args),
        tempPath,
        post:     (m) => post(m),
    });
    const requestBufferDump = handshake.requestBufferDump;

    // ---- temp-file binary read --------------------------------------------
    // PRBY-v1 layout (little-endian):
    //   "PRBY" (4) | u16 version | u16 reserved | u64 count | events[count]
    // Each event = double beats (8) | u8 status | u8 d1 | u8 d2 | u8 _pad

    function readBufferFile(path: string): MidiEvent[] {
        return readBufferFileWith(path, (p) => {
            const file = new File(p, "read");
            return file as unknown as BinaryReader;
        });
    }

    function unlinkTemp(path: string): void {
        // Forward to the [node.script cleanup.js] sidecar via outlet 0. The
        // patcher's [route unlink] splits this off before the message reaches
        // [poryaaaa~]. Node's fs.unlinkSync actually removes the file from
        // disk; v8's File class has no delete() (see docs/max-gotchas.md).
        outlet(0, "unlink", path);
    }

    // ---- SMF write to disk -------------------------------------------------

    function writeSmf(path: string, bytes: Uint8Array): boolean {
        // Resolve ~ in the path. Max's File class accepts ~ via conform-path
        // sometimes but not always; do it explicitly via a stat fallback.
        // The simplest robust approach: open in write mode, which Max
        // resolves against the search path and ~ handling.
        const f = new File(path, "write", "MIDI");
        if (!f.isopen) return false;
        try {
            f.byteorder = "little";
            // SMF is big-endian per spec; we pass raw bytes via writebytes
            // so endianness only matters for typed writes (we use none).
            // Max's File.writebytes takes number[], not Uint8Array.
            const arr = Array.from(bytes);
            (f as unknown as { writebytes: (data: number[], count: number) => void })
                .writebytes(arr, arr.length);
            return true;
        } finally {
            f.close();
        }
    }

    // ---- scheduleDeferred via Max Task (kept for potential future use) ----

    // Promise polyfill — Max's [v8] is es2020 so native Promise exists.
    // No additional shim needed.

    // ---- service wiring ----------------------------------------------------

    function collectCcomidiSnapshot(): CcomidiLomSnapshot {
        return collectCcomidiStateViaLom(
            (path) => new LiveAPI(null, path) as unknown as LiveApiLike,
            (msg) => post(msg),
        );
    }

    let recordArmed = false;

    const service = createRecorderService({
        outlet: (idx, ...args) => outlet(idx, ...args),
        post:   (msg) => post(msg),
        getProjectId,
        readFilename,
        writeFilename,
        readRange,
        writeRange,
        readMarkers,
        writeMarkers,
        collectCcomidiSnapshot,
        afterSuccessfulSave: () => {
            if (!recordArmed) return;
            post(`recorder: re-arming poryaaaa~ after successful save\n`);
            outlet(0, "record", 1);
        },
        makeSmfWriter: ({ voicemap, initialCcs, outputPath, range, markerRange }) => createSmfWriter({
            post: (m) => post(m),
            status: (m) => outlet(2, "set", m),
            requestBufferDump,
            readBufferFile,
            unlink: unlinkTemp,
            writeSmf,
            liveApi,
            voicemap,
            outputPath,
            range,
            markerRange,
            readInitialCcs: initialCcs,
            // Fire `clear` to poryaaaa~ via outlet 0. The patcher's
            // [route unlink] doesn't match `clear`, so it falls through to
            // poryaaaa~ which resets its MidiBuffer.
            clearBuffer: () => {
                post(`recorder: sending clear to poryaaaa~ via outlet 0\n`);
                outlet(0, "clear");
            },
        }),
    });

    // ---- message handlers --------------------------------------------------

    function ready(): void {
        service.ready();
    }

    function filename(...args: MaxAtom[]): void {
        post(`recorder: filename received raw args=${JSON.stringify(args)}\n`);
        if (args.length === 0) return;
        const name = args.map((a) => String(a)).join(" ").trim();
        if (!name) return;
        service.setFilename(name);
    }

    // Raw user-entered beat counts. Empty text clears the field; any
    // whitespace-only payload also clears.
    function startbeat(...args: MaxAtom[]): void {
        post(`recorder: startbeat received raw args=${JSON.stringify(args)}\n`);
        const s = args.map((a) => String(a)).join(" ");
        service.setStartBeat(s);
    }

    function lengthbeats(...args: MaxAtom[]): void {
        post(`recorder: lengthbeats received raw args=${JSON.stringify(args)}\n`);
        const s = args.map((a) => String(a)).join(" ");
        service.setLengthBeats(s);
    }

    function loopstart(...args: MaxAtom[]): void {
        post(`recorder: loopstart received raw args=${JSON.stringify(args)}\n`);
        const s = args.map((a) => String(a)).join(" ");
        service.setLoopStart(s);
    }

    function loopend(...args: MaxAtom[]): void {
        post(`recorder: loopend received raw args=${JSON.stringify(args)}\n`);
        const s = args.map((a) => String(a)).join(" ");
        service.setLoopEnd(s);
    }

    function record(...args: MaxAtom[]): void {
        post(`recorder: record received raw args=${JSON.stringify(args)}\n`);
        recordArmed = Number(args[0] ?? 0) !== 0;
        service.resetStatus();
    }

    function save(): void {
        // Fire-and-forget — async errors are surfaced via post().
        post(`recorder: save clicked currentFilename="${service.getFilename()}"\n`);
        service.save().catch((e) => {
            const err = e as { name?: unknown; message?: unknown; stack?: unknown };
            post(`recorder: save rejected: ${String(e)}\n`);
            post(`recorder: save rejected detail name=${String(err.name ?? "")} message=${String(err.message ?? "")}\n`);
            if (err.stack) post(`recorder: save rejected stack=${String(err.stack)}\n`);
        });
    }

    // `dumped <path> <count>` / `dumpfailed <path> <reason>` from poryaaaa~'s
    // status outlet. These settle the pending dump promise so Save cannot
    // remain stuck at "Saving...".
    function dumped(...args: MaxAtom[]): void {
        post(`recorder: dumped received raw args=${JSON.stringify(args)}\n`);
        const path  = String(args[0] ?? "");
        const count = Number(args[1] ?? 0);
        handshake.dumped(path, count);
    }

    function dumpfailed(...args: MaxAtom[]): void {
        post(`recorder: dumpfailed received raw args=${JSON.stringify(args)}\n`);
        const path = String(args[0] ?? "");
        const reason = String(args[1] ?? "dump_failed");
        handshake.dumpfailed(path, reason);
    }

    const handlers: Record<string, (...args: MaxAtom[]) => void> = {
        ready,
        filename,
        startbeat,
        lengthbeats,
        loopstart,
        loopend,
        record,
        save,
        dumped,
        dumpfailed,
    };

    Object.assign(globalThis, handlers);
}

// Detect whether we are running inside Max's [v8] object (where the
// global `outlet` and `messnamed` functions exist). In tests / plain
// Node we skip installing the Max-specific handlers.
if (typeof outlet === "function" && typeof messnamed === "function") installMaxHandlers();
