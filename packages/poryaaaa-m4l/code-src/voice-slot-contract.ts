export interface VoiceEnvelope {
  attack: number;
  decay: number;
  sustain: number;
  release: number;
}

export interface VoiceSlot {
  program: number;
  name: string;
  typeCode: number;
  envelope: VoiceEnvelope | null;
}

export interface VoicegroupSnapshotFrame {
  type: "snapshot";
  slots: VoiceSlot[];
}

const ENVELOPE_FIELDS = ["attack", "decay", "sustain", "release"] as const;

export function validateVoiceSlots(raw: unknown): VoiceSlot[] {
  if (!Array.isArray(raw)) {
    throw new Error("slots must be an array");
  }
  if (raw.length === 0) {
    throw new Error("empty slots are invalid");
  }

  return raw.map((entry, index) => {
    if (!entry || typeof entry !== "object" || Array.isArray(entry)) {
      throw new Error(`slot ${index}: expected object`);
    }
    const r = entry as Record<string, unknown>;
    if (!Number.isInteger(r.program) || r.program !== index) {
      throw new Error(`slot ${index}: program must be compact index ${index}`);
    }
    if (typeof r.name !== "string" || r.name.length === 0) {
      throw new Error(`slot ${index}: name must be a non-empty string`);
    }
    if (!Number.isInteger(r.typeCode)) {
      throw new Error(`slot ${index}: typeCode must be an integer`);
    }
    return {
      program: r.program as number,
      name: r.name,
      typeCode: r.typeCode as number,
      envelope: validateEnvelope(r.envelope, index),
    };
  });
}

function validateEnvelope(raw: unknown, index: number): VoiceEnvelope | null {
  if (raw === null) return null;
  if (!raw || typeof raw !== "object" || Array.isArray(raw)) {
    throw new Error(`slot ${index}: envelope must be an object or null`);
  }
  const r = raw as Record<string, unknown>;
  for (const key of ENVELOPE_FIELDS) {
    if (typeof r[key] !== "number" || !Number.isFinite(r[key])) {
      throw new Error(`slot ${index}: envelope.${key} must be a finite number`);
    }
  }
  return {
    attack: r.attack as number,
    decay: r.decay as number,
    sustain: r.sustain as number,
    release: r.release as number,
  };
}
