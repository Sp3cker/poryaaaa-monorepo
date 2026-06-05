// Recorder SMF writer.
//
// Save flow (driven from ccomidi_recorder.ts):
//   1. requestBufferDump()  → poryaaaa~ writes a binary blob, replies with
//                             {path, count}. JS owns the path (under /tmp).
//   2. readBufferFile(path) → parse the PRBY-v1 blob into MidiEvent[].
//   3. Query LOM for tempo / loop / time-signature.
//   4. Build SMF with midi-writer-js:
//        - conductor track: tempo, time-sig, loop markers
//        - one music track per MIDI channel actually used
//        - tick 0: captured pre-anchor ccomidi PC/CC state
//        - all captured events in arrival order, deltas computed by hand
//        - if loop on: re-emit each channel's last PC at loopStartTick
//        - held-note flush: note-off any open notes after the last event
//   5. Write SMF to user-chosen path.
//   6. finally: unlink the temp file (regardless of success/failure).
//
// Tempo / loop are queried at Save time. Constant-tempo limitation.
// Tempo automation across the export window is not supported in v0.
//
// Initial-state capture at tick 0 comes from ccomidi MIDI bytes that arrived
// before the first real Note On. Those setup PC/CC events are clamped to the
// export anchor so the SMF imports with the same channel state.

import MidiWriter from "midi-writer-js";

export interface InitialCc {
    channel: number;
    cc:      number;
    value:   number;
}

// Live imports this recorder's SMFs on a 96 PPQ grid. Keep the header division
// and every computed delta on that grid so near-boundary captures snap to the
// same tick positions Live will display.
export const PPQ = 96;

export interface MidiEvent {
    beats:  number;
    status: number;
    d1:     number;
    d2:     number;
}

export interface LoopState {
    on:     boolean;
    start:  number;   // beats
    length: number;   // beats
}

export interface LoopMarkers {
    startBeats: number;
    endBeats:   number;
}

export interface TimeSig {
    num: number;
    den: number;
}

// Narrow interface so the LOM access is mockable in tests.
export interface LiveApiAdapter {
    getTempo:    () => number;
    getLoop:     () => LoopState;
    getTimeSig:  () => TimeSig;
}

// Parse Ableton's bar.beat.sixteenth notation (1-indexed, like "37.1.1")
// into quarter-note beats — the same unit plugsync~ outlet 6 reports and
// the same unit `MidiEvent.beats` uses. Returns null on empty / malformed
// input; caller decides the fallback (typically: treat as no-range).
//
//   "37"       → bar 37, beat 1, 16th 1
//   "37.2"     → bar 37, beat 2, 16th 1
//   "37.2.3"   → bar 37, beat 2, 16th 3
//
// Honors the supplied time signature so 6/8 bars resolve to 3 quarter-beats
// each, not 4. Ableton's bar.beat.sixteenth `beat` is in denominator units.
export function parseBarBeatSixteenth(
    text:   string,
    sigNum: number,
    sigDen: number,
): number | null {
    const t = text.trim();
    if (!t) return null;
    const parts = t.split(".").map((p) => parseInt(p.trim(), 10));
    if (parts.some((p) => !Number.isFinite(p) || p < 1)) return null;
    const bar       = parts[0];
    const beat      = parts.length > 1 ? parts[1] : 1;
    const sixteenth = parts.length > 2 ? parts[2] : 1;
    const quartersPerBeat = 4 / sigDen;
    return (bar - 1) * sigNum * quartersPerBeat
         + (beat - 1) * quartersPerBeat
         + (sixteenth - 1) / 4;
}

export function parseBeatNumber(text: string): number | null {
    const t = text.trim();
    if (!t) return null;
    const n = Number(t);
    if (!Number.isFinite(n) || n < 0) return null;
    return n;
}

export interface SmfWriterDeps {
    post:              (msg: string) => void;
    // Brief UI-bound status string. Fires at every save-flow transition:
    //   - "Saving..."         on entry (after outputPath check)
    //   - "Saved: <name> (N events)"  on success
    //   - "FAILED: <reason>"  on any failure path
    // Optional so tests that don't care can omit it. Distinct from `post`,
    // which keeps verbose console output.
    status?:           (msg: string) => void;
    requestBufferDump: () => Promise<{ path: string; count: number }>;
    readBufferFile:    (path: string) => MidiEvent[];
    unlink:            (path: string) => void;
    writeSmf:          (path: string, bytes: Uint8Array) => boolean;
    liveApi:           LiveApiAdapter;
    voicemap:          () => Map<number, number>;   // channel → program
    outputPath:        () => string;                // resolved Save target
    // Optional raw start/length strings from the device UI fields. Both fields
    // must parse for range trim. If either field is empty or malformed, the
    // writer saves the whole captured buffer without slicing.
    range:             () => { start: string; length: string };
    markerRange:       () => { start: string; end: string };
    // Legacy hook for injected initial CCs. The normal Max path now leaves it
    // empty and relies on captured ccomidi MIDI bytes instead.
    readInitialCcs?:   () => InitialCc[];
    // Optional: wipe the C++ MidiBuffer after a successful save. Without
    // this, the same buffered events get re-saved on the next Save click,
    // OR (if the user keeps Rec armed) the next take appends to the prior
    // one. Failed saves do NOT clear, so the user can fix the issue (e.g.
    // bad path) and retry without losing the take.
    clearBuffer?:      () => void;
}

export interface SmfWriter {
    save: () => Promise<boolean>;
}

// ---------------------------------------------------------------------------
// Public factory

// Extract the trailing path component (after the last `/`) for the status
// line — the user wants to see "test2.mid" not the full Music path.
function basename(p: string): string {
    const i = p.lastIndexOf("/");
    return i >= 0 ? p.slice(i + 1) : p;
}

// Class name of a thrown value, for the FAILED status string. Falls back to
// "Error" for plain strings / numbers / null. Kept terse on purpose — the
// status indicator is ~150-200px wide; verbose stack-style messages already
// land in the Max console via `post`.
function errorLabel(e: unknown): string {
    if (e && typeof e === "object" && "message" in e
        && typeof (e as { message: unknown }).message === "string"
        && (e as { message: string }).message.trim()) {
        return (e as { message: string }).message.trim();
    }
    if (e && typeof e === "object" && "name" in e && typeof (e as { name: unknown }).name === "string") {
        return (e as { name: string }).name;
    }
    return "Error";
}

export interface SmfInitialStateValidation {
    ok: boolean;
    missing: string[];
}

interface InitialStateFlags {
    hasNote: boolean;
    pc:      boolean;
    volume:  boolean;
    pan:     boolean;
}

function readU32(bytes: ArrayLike<number>, i: number): number {
    return ((bytes[i] & 0xFF) << 24) | ((bytes[i + 1] & 0xFF) << 16)
         | ((bytes[i + 2] & 0xFF) << 8) | (bytes[i + 3] & 0xFF);
}

function isRealNoteOn(status: number, velocity: number): boolean {
    return (status & 0xF0) === 0x90 && (velocity & 0x7F) > 0;
}

// Validate the fully-rendered SMF bytes immediately before writing them to
// disk. Each music channel track must begin with enough state for a DAW import
// to sound correct without relying on prior playback: Program Change, Volume
// CC7, and Pan CC10 all at absolute tick 0.
export function validateSmfInitialChannelState(bytes: ArrayLike<number>): SmfInitialStateValidation {
    const byChannel = new Map<number, InitialStateFlags>();
    let i = 0;

    function readVarInt(trackEnd: number): number {
        let v = 0;
        while (i < trackEnd) {
            const b = bytes[i++] & 0xFF;
            v = (v << 7) | (b & 0x7F);
            if ((b & 0x80) === 0) return v;
        }
        throw new Error("unterminated SMF variable-length quantity");
    }

    function flagsFor(ch: number): InitialStateFlags {
        let flags = byChannel.get(ch);
        if (!flags) {
            flags = { hasNote: false, pc: false, volume: false, pan: false };
            byChannel.set(ch, flags);
        }
        return flags;
    }

    if (bytes.length < 14
        || bytes[0] !== 0x4D || bytes[1] !== 0x54
        || bytes[2] !== 0x68 || bytes[3] !== 0x64) {
        throw new Error("not an SMF: MThd missing");
    }
    i += 4;
    const headerLen = readU32(bytes, i); i += 4;
    i += headerLen;

    let trackIdx = 0;
    while (i < bytes.length) {
        if (bytes[i] !== 0x4D || bytes[i + 1] !== 0x54
            || bytes[i + 2] !== 0x72 || bytes[i + 3] !== 0x6B) {
            break;
        }
        i += 4;
        const trackLen = readU32(bytes, i); i += 4;
        const trackEnd = i + trackLen;
        let runningStatus = 0;
        let absTick = 0;

        while (i < trackEnd) {
            absTick += readVarInt(trackEnd);
            let status = bytes[i] & 0xFF;
            if (status < 0x80) {
                if (runningStatus === 0) throw new Error("SMF running status without prior status");
                status = runningStatus;
            } else {
                i++;
                if (status < 0xF0) runningStatus = status;
            }

            if (status === 0xFF) {
                i++; // meta type
                const len = readVarInt(trackEnd);
                i += len;
                continue;
            }
            if (status === 0xF0 || status === 0xF7) {
                const len = readVarInt(trackEnd);
                i += len;
                continue;
            }

            const high = status & 0xF0;
            const ch = status & 0x0F;
            const d1 = bytes[i++] & 0xFF;
            const d2 = (high === 0xC0 || high === 0xD0) ? 0 : (bytes[i++] & 0xFF);

            if (trackIdx === 0) continue;

            const flags = flagsFor(ch);
            if (isRealNoteOn(status, d2)) flags.hasNote = true;
            if (absTick === 0) {
                if (high === 0xC0) flags.pc = true;
                else if (high === 0xB0 && d1 === 0x07) flags.volume = true;
                else if (high === 0xB0 && d1 === 0x0A) flags.pan = true;
            }
            void d2;
        }

        i = trackEnd;
        trackIdx++;
    }

    const missing: string[] = [];
    for (const [ch, flags] of [...byChannel.entries()].sort((a, b) => a[0] - b[0])) {
        if (!flags.hasNote) continue;
        const label = `ch${ch + 1}`;
        if (!flags.pc) missing.push(`${label} Program Change`);
        if (!flags.volume) missing.push(`${label} Volume CC7`);
        if (!flags.pan) missing.push(`${label} Pan CC10`);
    }
    return { ok: missing.length === 0, missing };
}

export function createSmfWriter(deps: SmfWriterDeps): SmfWriter {
    const status = deps.status ?? (() => {});
    return {
        async save() {
            const outPath = deps.outputPath();
            if (!outPath) {
                deps.post("recorder: no output path set — type a filename and press Enter\n");
                status("FAILED: no filename");
                return false;
            }
            status("Saving...");

            let tempPath = "";
            try {
                const dump = await deps.requestBufferDump();
                tempPath = dump.path;

                const events = deps.readBufferFile(tempPath);
                deps.post(`recorder: dump path=${tempPath} count=${dump.count} parsed=${events.length}\n`);

                const tempo   = deps.liveApi.getTempo();
                const loop    = deps.liveApi.getLoop();
                const timeSig = deps.liveApi.getTimeSig();
                const voicemap = deps.voicemap();
                if (voicemap.size === 0) {
                    deps.post(`recorder: no synthetic voicemap PCs configured; relying on captured MIDI for PC state\n`);
                } else {
                    const entries: string[] = [];
                    for (const [ch, prog] of voicemap) {
                        entries.push(`ch${ch + 1}=prog${prog}`);
                    }
                    deps.post(`recorder: voicemap has ${voicemap.size} entries: ${entries.join(", ")}\n`);
                }

                const initialCcs = deps.readInitialCcs?.() ?? [];
                if (initialCcs.length > 0) {
                    deps.post(`recorder: injecting ${initialCcs.length} legacy initial CCs at tick 0\n`);
                } else {
                    deps.post(`recorder: no legacy initial CCs configured; relying on captured MIDI for CC state\n`);
                }

                // Parse the optional Start/Length fields as beat counts. Both
                // must be present and valid to trim; an empty or malformed
                // side means "save the whole captured buffer."
                let parsedRange: ExportRange | undefined;
                const rangeStrs = deps.range();
                const startStr = rangeStrs.start.trim();
                const lenStr   = rangeStrs.length.trim();
                const sb = startStr ? parseBeatNumber(startStr) : null;
                const lb = lenStr   ? parseBeatNumber(lenStr)   : null;
                if (startStr && sb === null) {
                    deps.post(`recorder: invalid Start "${startStr}" — ignoring\n`);
                }
                if (lenStr && lb === null) {
                    deps.post(`recorder: invalid Length "${lenStr}" — ignoring\n`);
                }
                if (sb !== null && lb !== null) {
                    const startBeats = sb;
                    parsedRange = {
                        startBeats,
                        endBeats:   startBeats + lb,
                    };
                    deps.post(`recorder: export range start=${parsedRange.startBeats} length=${lb} end=${parsedRange.endBeats}\n`);
                } else if (startStr || lenStr) {
                    deps.post("recorder: incomplete export range - saving whole buffer\n");
                }

                // Loop markers are Ableton bar.beat.sixteenth positions.
                let parsedMarkers: LoopMarkers | null = null;
                const markerStrs = deps.markerRange();
                const markerStartStr = markerStrs.start.trim();
                const markerEndStr   = markerStrs.end.trim();
                const mb = markerStartStr ? parseBarBeatSixteenth(markerStartStr, timeSig.num, timeSig.den) : null;
                const me = markerEndStr   ? parseBarBeatSixteenth(markerEndStr,   timeSig.num, timeSig.den) : null;
                if (markerStartStr && mb === null) {
                    deps.post(`recorder: invalid Loop Start "${markerStartStr}" — no loop markers\n`);
                }
                if (markerEndStr && me === null) {
                    deps.post(`recorder: invalid Loop End "${markerEndStr}" — no loop markers\n`);
                }
                if (mb !== null && me !== null) {
                    parsedMarkers = { startBeats: mb, endBeats: me };
                    deps.post(`recorder: loop markers start=${mb} end=${me}\n`);
                } else if (markerStartStr || markerEndStr) {
                    deps.post("recorder: incomplete loop markers - no loop markers\n");
                }
                deps.post(`recorder: Live loop ignored for SMF markers on=${loop.on ? 1 : 0} start=${loop.start} length=${loop.length}\n`);

                const smfBytes = buildSmf({
                    events,
                    tempo,
                    loop,
                    timeSig,
                    voicemap,
                    range: parsedRange,
                    loopMarkers: parsedMarkers,
                    anchorMode: "firstNote",
                    initialCcs,
                });
                deps.post(`recorder: smf bytes=${smfBytes.length}\n`);

                const initialState = validateSmfInitialChannelState(smfBytes);
                if (!initialState.ok) {
                    const missing = initialState.missing.join(", ");
                    deps.post(
                        `recorder: refusing export; missing tick-0 channel state: ${missing}\n`,
                    );
                    status("FAILED: missing tick-0 MIDI");
                    return false;
                }

                const ok = deps.writeSmf(outPath, smfBytes);
                if (!ok) {
                    deps.post(`recorder: FAILED to write ${outPath}\n`);
                    status("FAILED: write error");
                    return false;
                }
                deps.post(`recorder: wrote ${outPath} (${events.length} events)\n`);
                status(`Saved: ${basename(outPath)} (${events.length} events)`);
                // Clear the C++ buffer so a subsequent take doesn't append to
                // the saved one. Only fires on a confirmed write success —
                // failure paths leave the buffer intact for a retry.
                try { deps.clearBuffer?.(); }
                catch (_) { /* swallow — clear is best-effort */ }
                return true;
            } catch (e) {
                deps.post(`recorder: save threw: ${String(e)}\n`);
                status(`FAILED: ${errorLabel(e)}`);
                return false;
            } finally {
                if (tempPath) {
                    try { deps.unlink(tempPath); }
                    catch (_) { /* swallow — unlink is best-effort */ }
                }
            }
        },
    };
}

// ---------------------------------------------------------------------------
// Pure SMF assembly. Exported for unit-testing.

// Optional export-range trim. When provided, tick 0 of the SMF = `startBeats`
// in song-absolute beats, and any captured event outside [startBeats, endBeats]
// is dropped. Loop markers, PC-replay, and held-note flush are repositioned
// relative to the anchor. Omitting `range` keeps the previous song-absolute
// behavior (events stamped at their plugsync~-reported beat position).
export interface ExportRange {
    startBeats: number;
    endBeats:   number;
}

export interface BuildSmfInput {
    events:   MidiEvent[];
    tempo:    number;          // bpm
    loop:     LoopState;
    timeSig:  TimeSig;
    voicemap: Map<number, number>;
    range?:   ExportRange;
    loopMarkers?: LoopMarkers | null;
    // The recorder save path uses firstNote so captured ccomidi PC/CC setup
    // before the first played note becomes tick-0 state. Omit this for the
    // legacy song-absolute builder behavior.
    anchorMode?: "song" | "firstNote";
    // Legacy CC injection hook. Normal exports keep this empty so captured
    // ccomidi MIDI is the tick-0 source of truth.
    initialCcs?: InitialCc[];
}

function capturedEventTick(beats: number, anchor: number): number {
    const tick = Math.round((beats - anchor) * PPQ);
    // [midiin] events arrive on the scheduler after the latest plugsync~ beat
    // sample has already been latched, so real exports land one SMF tick early
    // on the grid. Compensate after quantization, but keep the true start at 0.
    return tick > 0 ? tick + 1 : 0;
}

function loopStateFromMarkers(markers: LoopMarkers): LoopState {
    return {
        on:     true,
        start:  markers.startBeats,
        length: Math.max(0, markers.endBeats - markers.startBeats),
    };
}

type PendingDescriptor =
    | { kind: "noteOn";  channel: number; pitch: number; velocity: number; tick: number; insOrder: number }
    | { kind: "noteOff"; channel: number; pitch: number; velocity: number; tick: number; insOrder: number }
    | { kind: "cc";      channel: number; cn: number; cv: number;          tick: number; insOrder: number;
                         synthetic?: boolean }
    | { kind: "pc";      channel: number; program: number;                 tick: number; insOrder: number;
                         synthetic?: boolean; clamped?: boolean }
    | { kind: "bend";    channel: number; bend: number;                    tick: number; insOrder: number };

const XCMD_SELECTOR_CC = 0x1E;
const XCMD_VALUE_CC = 0x1D;
const VOLUME_CC = 7;
const PAN_CC = 10;

interface BuiltMidiChunk {
    type: number[];
    size: number[];
    data: number[];
}

interface BuildDataWriter {
    buildData: () => BuiltMidiChunk[];
}

function buildWriterFile(writer: BuildDataWriter): Uint8Array {
    const chunks = writer.buildData();
    let total = 0;
    for (const chunk of chunks) {
        total += chunk.type.length + chunk.size.length + chunk.data.length;
    }
    const bytes = new Uint8Array(total);
    let offset = 0;
    for (const chunk of chunks) {
        for (const part of [chunk.type, chunk.size, chunk.data]) {
            bytes.set(part, offset);
            offset += part.length;
        }
    }
    return bytes;
}

function xcmdPairKey(first: PendingDescriptor, second: PendingDescriptor): string | null {
    if (first.kind !== "cc" || second.kind !== "cc") return null;
    if (first.cn !== XCMD_SELECTOR_CC || second.cn !== XCMD_VALUE_CC) return null;
    if (first.tick !== second.tick) return null;
    return `${first.tick}|${first.cv}|${second.cv}`;
}

function markDuplicateXcmdPairs(arr: PendingDescriptor[], superseded: Set<number>): void {
    const seen = new Set<string>();
    for (let i = 0; i < arr.length - 1; i++) {
        const key = xcmdPairKey(arr[i], arr[i + 1]);
        if (key === null) continue;
        if (seen.has(key)) {
            superseded.add(i);
            superseded.add(i + 1);
        } else {
            seen.add(key);
        }
        i++;
    }
}

export function buildSmf(input: BuildSmfInput): Uint8Array {
    const { events, tempo, loop, timeSig, voicemap, range } = input;

    // Anchor + filter window: tick 0 of the SMF corresponds to `anchor` in
    // song-absolute beats. With a range, the range start wins. Without a range,
    // the default is legacy song-absolute output unless firstNote mode is
    // requested by the recorder save path. The +/- 1e-9 slack absorbs float
    // jitter from plugsync~.
    const firstNote = input.anchorMode === "firstNote"
        ? events.find((e) => isRealNoteOn(e.status, e.d2))
        : undefined;
    const anchor   = range ? range.startBeats : firstNote?.beats ?? 0;
    const rangeEnd = range ? range.endBeats   : Number.POSITIVE_INFINITY;
    const explicitMarkers = input.loopMarkers;
    const markerLoop = "loopMarkers" in input
        ? explicitMarkers === null || explicitMarkers === undefined
            ? { on: false, start: 0, length: 0 }
            : loopStateFromMarkers(explicitMarkers)
        : range && Number.isFinite(range.endBeats)
            ? { on: true, start: range.startBeats, length: Math.max(0, range.endBeats - range.startBeats) }
            : loop.on && events.length > 0
                ? { on: true, start: 0, length: loop.length }
                : loop;
    const legacyLoopStartReplay = !("loopMarkers" in input) && !range && loop.on && events.length > 0;

    // Conductor track (track 0): tempo, time-sig, loop markers.
    // Marker text uses the same `[` / `]` convention the previous C++ writer
    // emitted so any downstream tooling that recognised those markers keeps
    // working.
    const conductor = new MidiWriter.Track();
    conductor.setTempo(tempo);
    conductor.setTimeSignature(timeSig.num, timeSig.den, 24, 8);
    if (markerLoop.on) {
        // MarkerEvent bakes `delta` at construction time, so we pass the
        // delta directly. Marker positions are anchor-relative; if the
        // loop region falls entirely before the anchor both deltas land
        // at 0 (clamped) and the open marker ends up at tick 0 instead
        // of being dropped — acceptable per the "user enters values, they
        // get what they entered" stance.
        const openTickAbs  = Math.round((markerLoop.start - anchor) * PPQ);
        const closeTickAbs = Math.round((markerLoop.start + markerLoop.length - anchor) * PPQ);
        const openDelta    = Math.max(0, openTickAbs);
        const closeDelta   = Math.max(0, closeTickAbs - Math.max(0, openTickAbs));
        conductor.addEvent(new MidiWriter.MarkerEvent({
            text:  "[",
            delta: openDelta,
        }));
        conductor.addEvent(new MidiWriter.MarkerEvent({
            text:  "]",
            delta: closeDelta,
        }));
    }

    // Per-channel pending descriptors + book-keeping. Construction of the
    // midi-writer-js events is deferred to step 5 so we can pass the correct
    // delta via each event's load-bearing constructor field (NoteOnEvent
    // reads `wait`, NoteOffEvent reads `delta` late, the rest bake `delta`
    // at construction).
    const pending = new Map<number, PendingDescriptor[]>();
    const loopStartPcValue = new Map<number, number>();
    const loopStartCcValue = new Map<number, Map<number, number>>();
    const firstCapturedPcValue = new Map<number, number>();
    const openNotes = new Map<number, Set<number>>();   // ch -> open pitches
    let insCounter = 0;
    const loopStartTick = markerLoop.on
        ? Math.max(0, Math.round((markerLoop.start - anchor) * PPQ))
        : 0;

    function ensureChan(ch: number): PendingDescriptor[] {
        let arr = pending.get(ch);
        if (!arr) { arr = []; pending.set(ch, arr); }
        return arr;
    }

    function rememberLoopStartCc(ch: number, cc: number, value: number): void {
        if (cc !== VOLUME_CC && cc !== PAN_CC) return;
        let byCc = loopStartCcValue.get(ch);
        if (!byCc) { byCc = new Map(); loopStartCcValue.set(ch, byCc); }
        byCc.set(cc, value);
    }

    // Step 1: legacy synthetic tick-0 PCs. The normal Max recorder path passes
    // an empty map and relies on captured ccomidi MIDI instead.
    for (const [ch, prog] of voicemap) {
        if (ch < 0 || ch > 15) continue;
        if (prog < 0) continue;
        ensureChan(ch).push({
            kind: "pc", channel: ch, program: prog,
            tick: 0, insOrder: insCounter++,
            synthetic: true,
        });
        if (markerLoop.on) loopStartPcValue.set(ch, prog);
    }

    // Step 1b: legacy synthetic tick-0 CCs. These come after synthetic PCs so
    // callers that still use this hook get PC-then-CC ordering.
    if (input.initialCcs) {
        for (const ic of input.initialCcs) {
            if (ic.channel < 0 || ic.channel > 15) continue;
            if (ic.cc < 0      || ic.cc      > 127) continue;
            if (ic.value < 0   || ic.value   > 127) continue;
            ensureChan(ic.channel).push({
                kind: "cc", channel: ic.channel, cn: ic.cc, cv: ic.value,
                tick: 0, insOrder: insCounter++,
            });
            if (markerLoop.on) rememberLoopStartCc(ic.channel, ic.cc, ic.value);
        }
    }

    // Step 2: walk the captured events. Ticks are rounded at compute time so
    // the per-event sort and delta math stay on integer ticks (no rounding
    // drift across consecutive non-grid beats). When `range` is set, events
    // outside [anchor, rangeEnd] are dropped here so they never reach the
    // per-channel emit loop.
    let lastEventTick = 0;
    for (const e of events) {
        const type = (e.status >> 4) & 0x0F;
        const ch   = e.status & 0x0F;
        const d1   = e.d1 & 0x7F;
        const d2   = e.d2 & 0x7F;
        const beforeAnchor = e.beats < anchor - 1e-9;
        if (beforeAnchor && (range || (type !== 0xB && type !== 0xC))) continue;
        if (e.beats > rangeEnd + 1e-9) continue;
        const tick = beforeAnchor ? 0 : capturedEventTick(e.beats, anchor);
        if (tick > lastEventTick) lastEventTick = tick;

        switch (type) {
            case 0x8: {
                const open = openNotes.get(ch);
                if (open?.has(d1)) {
                    ensureChan(ch).push({
                        kind: "noteOff", channel: ch, pitch: d1, velocity: d2,
                        tick, insOrder: insCounter++,
                    });
                    open.delete(d1);
                }
                break;
            }
            case 0x9: {
                if (d2 === 0) {
                    // running-status note-off
                    const open = openNotes.get(ch);
                    if (open?.has(d1)) {
                        ensureChan(ch).push({
                            kind: "noteOff", channel: ch, pitch: d1, velocity: 0,
                            tick, insOrder: insCounter++,
                        });
                        open.delete(d1);
                    }
                } else {
                    let open = openNotes.get(ch);
                    if (!open) { open = new Set(); openNotes.set(ch, open); }
                    if (open.has(d1)) {
                        ensureChan(ch).push({
                            kind: "noteOff", channel: ch, pitch: d1, velocity: 0,
                            tick, insOrder: insCounter++,
                        });
                    }
                    ensureChan(ch).push({
                        kind: "noteOn", channel: ch, pitch: d1, velocity: d2,
                        tick, insOrder: insCounter++,
                    });
                    open.add(d1);
                }
                break;
            }
            case 0xB: {
                ensureChan(ch).push({
                    kind: "cc", channel: ch, cn: d1, cv: d2,
                    tick, insOrder: insCounter++,
                });
                if (markerLoop.on && tick <= loopStartTick) {
                    rememberLoopStartCc(ch, d1, d2);
                }
                break;
            }
            case 0xC: {
                ensureChan(ch).push({
                    kind: "pc", channel: ch, program: d1,
                    tick, insOrder: insCounter++,
                    clamped: beforeAnchor,
                });
                if (markerLoop.on && tick <= loopStartTick) {
                    loopStartPcValue.set(ch, d1);
                }
                if (legacyLoopStartReplay && !firstCapturedPcValue.has(ch)) {
                    firstCapturedPcValue.set(ch, d1);
                }
                break;
            }
            case 0xE: {
                const value14 = (d2 << 7) | d1;
                // midi-writer-js's PitchBendEvent expects a -1..1 bend.
                const normalized = (value14 - 8192) / 8192;
                ensureChan(ch).push({
                    kind: "bend", channel: ch, bend: normalized,
                    tick, insOrder: insCounter++,
                });
                break;
            }
            default:
                /* poly AT (0xA), channel AT (0xD), system messages — drop */
                break;
        }
    }

    // Step 3: PC-replay at loop start. For each channel with a known last PC,
    // re-emit the program active at loopStartTick so a looping DAW import
    // re-seats program state when the playhead wraps.
    if (markerLoop.on) {
        if (legacyLoopStartReplay) {
            for (const [ch, prog] of firstCapturedPcValue) {
                if (!loopStartPcValue.has(ch)) loopStartPcValue.set(ch, prog);
            }
        }
        for (const [ch, prog] of loopStartPcValue) {
            ensureChan(ch).push({
                kind: "pc", channel: ch, program: prog,
                tick: loopStartTick, insOrder: insCounter++,
                synthetic: true,
            });
        }
        for (const [ch, byCc] of loopStartCcValue) {
            for (const [cc, value] of byCc) {
                ensureChan(ch).push({
                    kind: "cc", channel: ch, cn: cc, cv: value,
                    tick: loopStartTick, insOrder: insCounter++,
                    synthetic: true,
                });
            }
        }
    }

    // Step 4: held-note flush. Any note-on still open at buffer end gets a
    // matching note-off at lastEventTick + 1 to keep the SMF from leaking
    // hanging notes.
    const flushTick = lastEventTick + 1;
    for (const [ch, open] of openNotes) {
        for (const pitch of open) {
            ensureChan(ch).push({
                kind: "noteOff", channel: ch, pitch, velocity: 0,
                tick: flushTick, insOrder: insCounter++,
            });
        }
    }

    // Step 5: per-channel stable sort + per-class event construction.
    // Each event class flows its delta into the byte stream through a
    // different field — see midi-writer-js build/index.js:
    //   - NoteOnEvent:           `wait: "T<n>"` (string form)
    //   - NoteOffEvent:          `delta` read late inside buildData
    //   - ControllerChangeEvent: `delta` baked at ctor
    //   - ProgramChangeEvent:    `delta` baked at ctor
    //   - PitchBendEvent:        `delta` baked at ctor
    // Insertion order is the tiebreaker for same-tick events (verified in
    // code-src/test/midi_writer_probe.test.ts).
    const tracks: InstanceType<typeof MidiWriter.Track>[] = [conductor];
    const sortedChans = [...pending.keys()].sort((a, b) => a - b);
    for (const ch of sortedChans) {
        const arr = pending.get(ch)!;
        arr.sort((a, b) =>
            a.tick !== b.tick ? a.tick - b.tick : a.insOrder - b.insOrder);

        // Pre-pass: mark same-tick CC entries that get overwritten by a later
        // sibling on the same (controller, tick). User-visible symptom this
        // fixes: ccomidi emits its Volume CC multiple times at clip-launch
        // boundaries (64 default then 97 configured, repeating), all landing
        // at the same tick post-rounding. The user wants the LAST emission
        // (the configured value) to win. XCMD CCs are handled as ordered
        // pairs first so the individual-CC pass cannot corrupt pair order.
        const superseded = new Set<number>();
        markDuplicateXcmdPairs(arr, superseded);
        const lastIdxByCcAtTick = new Map<string, number>();
        const lastIdxByPcAtTick = new Map<number, number>();
        for (let i = 0; i < arr.length; i++) {
            const p = arr[i];
            if (superseded.has(i)) continue;
            if (p.kind === "pc" && p.clamped) {
                const prev = lastIdxByPcAtTick.get(p.tick);
                if (prev !== undefined) superseded.add(prev);
                lastIdxByPcAtTick.set(p.tick, i);
            }
            if (p.kind === "cc") {
                if (p.cn === XCMD_VALUE_CC || p.cn === XCMD_SELECTOR_CC) continue;
                const key = `${p.cn}|${p.tick}`;
                const prev = lastIdxByCcAtTick.get(key);
                if (prev !== undefined) superseded.add(prev);
                lastIdxByCcAtTick.set(key, i);
            }
        }

        const t = new MidiWriter.Track();
        let prevTick = 0;
        // Per-channel last captured program. Drops consecutive *captured* PCs
        // that don't change the program — ccomidi fires emit_program on every
        // program attribute change (Live triggers it on automation breakpoints
        // and clip launches), so identical PCs pile up at clip boundaries.
        // MIDI semantics: duplicate PC is a no-op, so dropping is safe.
        // SYNTHETIC PCs (voicemap injection at tick 0, loop-replay at loop
        // start) are exempt and do not seed this dedupe state. They're
        // deliberate re-statements the recorder owns, while captured PCs still
        // need to survive at their original ticks.
        let lastCapturedProgram = -1;
        // Per-controller last emitted value. Drops a captured CC whose value
        // equals what's already on the wire for that (channel, cc). Legacy
        // tick-0 initialCcs also seed this map through normal event emission.
        // XCMD CCs are only deduped as ordered pairs in the same-tick pre-pass.
        const lastCcValue = new Map<number, number>();
        for (let i = 0; i < arr.length; i++) {
            const p = arr[i];
            if (superseded.has(i)) continue;
            if (p.kind === "pc" && !p.synthetic && p.program === lastCapturedProgram) {
                continue;
            }
            if (p.kind === "cc" && !p.synthetic
                && p.cn !== XCMD_VALUE_CC && p.cn !== XCMD_SELECTOR_CC
                && lastCcValue.get(p.cn) === p.cv) {
                continue;
            }
            const delta = Math.max(0, p.tick - prevTick);
            switch (p.kind) {
                case "noteOn":
                    t.addEvent(new MidiWriter.NoteOnEvent({
                        channel:  p.channel + 1,
                        pitch:    p.pitch,
                        velocity: p.velocity,
                        wait:     "T" + delta,
                    }));
                    break;
                case "noteOff":
                    t.addEvent(new MidiWriter.NoteOffEvent({
                        channel:  p.channel + 1,
                        pitch:    p.pitch,
                        velocity: p.velocity,
                        duration: "T1",     // unused; buildData reads this.delta late
                        delta,
                    }));
                    break;
                case "cc":
                    t.addEvent(new MidiWriter.ControllerChangeEvent({
                        channel:          p.channel + 1,
                        controllerNumber: p.cn,
                        controllerValue:  p.cv,
                        delta,
                    }));
                    break;
                case "pc":
                    // midi-writer-js ProgramChangeEvent expects 0-indexed channel
                    // (no internal -1 unlike NoteOn/Off/CC); passing p.channel+1
                    // would place ch 15 events at 0xD0 (ChannelPressure) and
                    // corrupt the MTrk parse downstream.
                    t.addEvent(new MidiWriter.ProgramChangeEvent({
                        channel:    p.channel,
                        instrument: p.program,
                        delta,
                    }));
                    break;
                case "bend":
                    // Same 0-indexed channel convention as ProgramChangeEvent;
                    // passing p.channel+1 would emit ch 15 bend as 0xF0 (SysEx).
                    t.addEvent(new MidiWriter.PitchBendEvent({
                        channel: p.channel,
                        bend:    p.bend,
                        delta,
                    }));
                    break;
            }
            if (p.kind === "pc" && !p.synthetic) lastCapturedProgram = p.program;
            if (p.kind === "cc") lastCcValue.set(p.cn, p.cv);
            prevTick = p.tick;
        }
        tracks.push(t);
    }

    // ticksPerBeat MUST match the PPQ used to compute every delta above.
    // Many parsers trust the header, so a mismatched division shifts every
    // placement even if the event deltas are internally consistent.
    const writer = new MidiWriter.Writer(tracks, { ticksPerBeat: PPQ });
    return buildWriterFile(writer as unknown as BuildDataWriter);
}
