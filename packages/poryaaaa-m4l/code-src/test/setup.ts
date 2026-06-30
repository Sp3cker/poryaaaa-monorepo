import { existsSync, readFileSync } from "node:fs";
import { createRequire } from "node:module";

interface MaxApiStub {
    addHandler: (..._args: unknown[]) => void;
    outlet: (..._args: unknown[]) => void;
    post: (..._args: unknown[]) => void;
}

const maxApiStub: MaxApiStub = {
    addHandler: () => {},
    outlet: () => {},
    post: () => {},
};

const require = createRequire(__filename);
const Module = require("node:module") as typeof import("node:module") & {
    _load: (request: string, parent: unknown, isMain: boolean) => unknown;
};
const originalLoad = Module._load;

Module._load = function loadWithMaxTestStubs(this: unknown, request: string, parent: unknown, isMain: boolean) {
    if (request === "max-api") return maxApiStub;
    if (request.endsWith("poryaaaa_voicegroup.node")) return nativeVoicegroupStub;
    return originalLoad.call(this, request, parent, isMain);
};

interface VoiceSlot {
    name: string;
    typeCode: number;
}

type VoicegroupParseResult =
    | { ok: true; slots: Array<VoiceSlot | null> }
    | { ok: false; diagnostics: string[] };

const nativeVoicegroupStub = {
    parseVoicegroup(root: string, bank: string): VoicegroupParseResult {
        const normalizedBank = bank.endsWith(".inc") ? bank.slice(0, -4) : bank;
        const file = `${root}/sound/voicegroups/${normalizedBank}.inc`;
        const body = existsSync(file) ? readFileSync(file, "utf8") : "";

        if (normalizedBank === "voicegroup001") {
            const slots = emptySlots();
            slots[0] = { name: "DirectSoundWaveData_PianoC4", typeCode: 0x00 };
            slots[1] = { name: "Square 1", typeCode: 0x01 };
            slots[2] = { name: "Square 2", typeCode: 0x02 };
            slots[3] = { name: "Noise", typeCode: 0x04 };
            slots[4] = { name: "ProgWave", typeCode: 0x03 };
            slots[5] = { name: "voicegroup_drumkit", typeCode: 0x40 };
            slots[6] = { name: "voicegroup002", typeCode: 0x80 };
            return { ok: true, slots };
        }

        if (body.includes("voice_directsounnd") || body.includes("MissingEnvelopeArgs")) {
            return { ok: false, diagnostics: ["malformed voice macro"] };
        }

        if (body.includes("voice_group voicegroup001, 24")
            && body.includes("DirectSoundWaveData_PianoC4")) {
            const slots = emptySlots();
            slots[24] = { name: "DirectSoundWaveData_PianoC4", typeCode: 0x00 };
            return { ok: true, slots };
        }

        return { ok: false, diagnostics: [`missing fixture for ${normalizedBank}`] };
    },
};

function emptySlots(): Array<VoiceSlot | null> {
    return Array.from({ length: 128 }, () => null);
}

class TestDict {
    constructor(private readonly name: string) {}

    stringify() {
        return JSON.stringify(this.name);
    }

    freepeer() {}
}

(globalThis as typeof globalThis & { Dict?: typeof TestDict }).Dict = TestDict;
