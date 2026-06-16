import { parseDictLike } from "../ccomidi_routing_dicts";
import type { InitialCc } from "./recorder_smf_writer";

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
    const volume = state.ccs.find((entry) => entry.cc === 0x07)?.value ?? null;
    const pan = state.ccs.find((entry) => entry.cc === 0x0A)?.value ?? null;
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
