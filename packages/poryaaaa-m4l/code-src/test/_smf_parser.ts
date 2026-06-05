// Extended SMF parser used by adversarial tests. Decodes varint deltas to
// reconstruct absolute ticks for every event (channel-voice AND meta), and
// reports both the event kind and the track it lives in.
//
// This is not a general SMF parser. It knows just enough to support the
// invariants the recorder refactor is supposed to preserve.

export interface VoiceEvent {
    kind:     "voice";
    track:    number;          // 0-based MTrk index
    tick:     number;          // absolute tick within that track
    status:   number;          // full status byte (channel-encoded)
    d1:       number;
    d2:       number;
}

export interface MetaEvent {
    kind:     "meta";
    track:    number;
    tick:     number;
    metaType: number;          // 0x51 = tempo, 0x58 = time sig, 0x06 = marker, ...
    data:     number[];        // raw payload bytes
}

export type SmfEvent = VoiceEvent | MetaEvent;

export interface SmfHeader {
    format:   number;          // 0, 1, or 2
    tracks:   number;
    division: number;          // PPQ when MSB == 0
}

export interface ParsedSmf {
    header: SmfHeader;
    events: SmfEvent[];
}

function readU16(bytes: ArrayLike<number>, i: number): number {
    return ((bytes[i] & 0xFF) << 8) | (bytes[i + 1] & 0xFF);
}
function readU32(bytes: ArrayLike<number>, i: number): number {
    return ((bytes[i] & 0xFF) << 24) | ((bytes[i + 1] & 0xFF) << 16)
         | ((bytes[i + 2] & 0xFF) << 8) | (bytes[i + 3] & 0xFF);
}

export function parseSmf(input: ArrayLike<number>): ParsedSmf {
    const bytes: ArrayLike<number> = input;
    let i = 0;

    if (bytes[i] !== 0x4D || bytes[i + 1] !== 0x54
        || bytes[i + 2] !== 0x68 || bytes[i + 3] !== 0x64) {
        throw new Error("not an SMF: MThd missing");
    }
    i += 4;
    const headerLen = readU32(bytes, i); i += 4;
    if (headerLen !== 6) throw new Error(`MThd length ${headerLen} != 6`);
    const format   = readU16(bytes, i); i += 2;
    const tracks   = readU16(bytes, i); i += 2;
    const division = readU16(bytes, i); i += 2;

    const events: SmfEvent[] = [];

    function readVarInt(): number {
        let v = 0;
        while (true) {
            const b = bytes[i++] & 0xFF;
            v = (v << 7) | (b & 0x7F);
            if ((b & 0x80) === 0) return v;
        }
    }

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
            const delta = readVarInt();
            absTick += delta;
            let status = bytes[i] & 0xFF;
            if (status < 0x80) {
                status = runningStatus;
            } else {
                i++;
                if (status < 0xF0) runningStatus = status;
            }
            if (status === 0xFF) {
                const metaType = bytes[i++] & 0xFF;
                const metaLen  = readVarInt();
                const data: number[] = [];
                for (let k = 0; k < metaLen; k++) data.push(bytes[i + k] & 0xFF);
                i += metaLen;
                events.push({
                    kind:  "meta", track: trackIdx, tick: absTick,
                    metaType, data,
                });
                continue;
            }
            if (status === 0xF0 || status === 0xF7) {
                const sysLen = readVarInt();
                i += sysLen;
                continue;
            }
            const high = status & 0xF0;
            const d1 = bytes[i++] & 0xFF;
            const d2 = (high === 0xC0 || high === 0xD0) ? 0 : (bytes[i++] & 0xFF);
            events.push({
                kind: "voice", track: trackIdx, tick: absTick,
                status, d1, d2,
            });
        }
        i = trackEnd;
        trackIdx++;
    }

    return { header: { format, tracks, division }, events };
}

export function voiceEvents(parsed: ParsedSmf): VoiceEvent[] {
    return parsed.events.filter((e): e is VoiceEvent => e.kind === "voice");
}
export function metaEvents(parsed: ParsedSmf): MetaEvent[] {
    return parsed.events.filter((e): e is MetaEvent => e.kind === "meta");
}
export function byStatusNibble(events: VoiceEvent[], nib: number): VoiceEvent[] {
    return events.filter((e) => (e.status & 0xF0) === (nib & 0xF0));
}
