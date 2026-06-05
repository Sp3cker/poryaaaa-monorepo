import { readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";

import { validateVoiceSlots, type VoiceSlot } from "../voice-slot-contract";
export { validateVoiceSlots, type VoiceEnvelope, type VoiceSlot } from "../voice-slot-contract";

export type VoicegroupParseResult =
  | { ok: true; slots: VoiceSlot[] }
  | { ok: false; diagnostics: string[] };

const VOICE_DIRECTSOUND = 0x00;
const VOICE_SQUARE_1 = 0x01;
const VOICE_SQUARE_2 = 0x02;
const VOICE_PROGRAMMABLE_WAVE = 0x03;
const VOICE_NOISE = 0x04;
const VOICE_DIRECTSOUND_NO_RESAMPLE = 0x08;
const VOICE_SQUARE_1_ALT = 0x09;
const VOICE_SQUARE_2_ALT = 0x0a;
const VOICE_PROGRAMMABLE_WAVE_ALT = 0x0b;
const VOICE_NOISE_ALT = 0x0c;
const VOICE_DIRECTSOUND_ALT = 0x10;
const VOICE_CRY = 0x20;
const VOICE_CRY_REVERSE = 0x30;
const VOICE_KEYSPLIT = 0x40;
const VOICE_KEYSPLIT_ALL = 0x80;

interface LineParts {
  code: string;
  comment: string;
}

type MacroParser = (
  args: string,
  lineNumber: number,
  comment: string,
  diagnostics: string[],
) => VoiceSlot | null;

interface MacroDef {
  keyword: string;
  parse: MacroParser;
}

export function scanVoicegroupBanks(root: string): string[] {
  const dir = join(root, "sound", "voicegroups");
  let filenames: string[];
  try {
    filenames = readdirSync(dir);
  } catch (_) {
    return [];
  }
  return filenames
    .filter((name) => name.endsWith(".inc"))
    .filter((name) => !name.startsWith("se_"))
    .map((name) => name.slice(0, -".inc".length))
    .sort((a, b) => a.localeCompare(b, undefined, { sensitivity: "base" }));
}

export function parseVoicegroup(
  root: string,
  bank: string,
): VoicegroupParseResult {
  const bankName = bank.endsWith(".inc") ? bank.slice(0, -".inc".length) : bank;
  const path = join(root, "sound", "voicegroups", `${bankName}.inc`);
  let source: string;
  try {
    source = readFileSync(path, "utf8");
  } catch (err) {
    return {
      ok: false,
      diagnostics: [`cannot read voicegroup "${bankName}": ${String(err)}`],
    };
  }
  return parseVoicegroupSource(source, bankName);
}

export function parseVoicegroupSource(
  source: string,
  bankName = "(inline)",
): VoicegroupParseResult {
  const diagnostics: string[] = [];
  const slots: VoiceSlot[] = [];
  const lines = source.split(/\r?\n/);

  for (let i = 0; i < lines.length; i += 1) {
    const lineNumber = i + 1;
    const { code, comment } = stripLineComment(lines[i]);
    const line = code.trim();
    if (!line) continue;
    if (line.startsWith(".")) continue;
    if (isLabel(line)) continue;

    if (/^voice_group(?:\s|$)/.test(line)) {
      continue;
    }

    const parsed = parseVoiceMacro(line, lineNumber, comment, diagnostics);
    if (!parsed) continue;
    slots.push({
      ...parsed,
      program: slots.length,
    });
  }

  if (diagnostics.length > 0) {
    return { ok: false, diagnostics };
  }

  try {
    return { ok: true, slots: validateVoiceSlots(slots) };
  } catch (err) {
    return {
      ok: false,
      diagnostics: [`${bankName}: ${err instanceof Error ? err.message : String(err)}`],
    };
  }
}

function stripLineComment(line: string): LineParts {
  const idx = line.indexOf("@");
  if (idx < 0) return { code: line, comment: "" };
  return {
    code: line.slice(0, idx),
    comment: line.slice(idx + 1).trim(),
  };
}

function isLabel(line: string): boolean {
  return /^[A-Za-z_.$][\w.$]*::$/.test(line);
}

const MACROS: MacroDef[] = [
  { keyword: "voice_directsound_no_resample", parse: directsound(VOICE_DIRECTSOUND_NO_RESAMPLE) },
  { keyword: "voice_directsound_alt", parse: directsound(VOICE_DIRECTSOUND_ALT) },
  { keyword: "voice_directsound", parse: directsound(VOICE_DIRECTSOUND) },
  { keyword: "voice_square_1_alt", parse: square1(VOICE_SQUARE_1_ALT, "Square 1 (alt)") },
  { keyword: "voice_square_1", parse: square1(VOICE_SQUARE_1, "Square 1") },
  { keyword: "voice_square_2_alt", parse: square2(VOICE_SQUARE_2_ALT, "Square 2 (alt)") },
  { keyword: "voice_square_2", parse: square2(VOICE_SQUARE_2, "Square 2") },
  { keyword: "voice_programmable_wave_alt", parse: programmableWave(VOICE_PROGRAMMABLE_WAVE_ALT) },
  { keyword: "voice_programmable_wave", parse: programmableWave(VOICE_PROGRAMMABLE_WAVE) },
  { keyword: "voice_noise_alt", parse: noise(VOICE_NOISE_ALT, "Noise (alt)") },
  { keyword: "voice_noise", parse: noise(VOICE_NOISE, "Noise") },
  { keyword: "voice_keysplit_all", parse: keysplitAll },
  { keyword: "voice_keysplit", parse: keysplit },
  { keyword: "cry_reverse", parse: cry(VOICE_CRY_REVERSE) },
  { keyword: "cry", parse: cry(VOICE_CRY) },
];

function parseVoiceMacro(
  line: string,
  lineNumber: number,
  comment: string,
  diagnostics: string[],
): VoiceSlot | null {
  for (const macro of MACROS) {
    if (!line.startsWith(macro.keyword)) continue;
    const tail = line.slice(macro.keyword.length);
    if (!/^\s/.test(tail)) continue;
    return macro.parse(tail.trim(), lineNumber, comment, diagnostics);
  }
  diagnostics.push(`line ${lineNumber}: unsupported voicegroup syntax: ${line}`);
  return null;
}

function directsound(typeCode: number): MacroParser {
  return (args, lineNumber, _comment, diagnostics) => {
    const parsed = parseCommaArgs(args, 7, lineNumber, "voice_directsound", diagnostics);
    if (!parsed) return null;
    const attack = parseNumericField(parsed[3], lineNumber, "attack", diagnostics);
    const decay = parseNumericField(parsed[4], lineNumber, "decay", diagnostics);
    const sustain = parseNumericField(parsed[5], lineNumber, "sustain", diagnostics);
    const release = parseNumericField(parsed[6], lineNumber, "release", diagnostics);
    if ([attack, decay, sustain, release].some((value) => value === null)) return null;
    return {
      program: -1,
      name: parsed[2],
      typeCode,
      envelope: {
        attack: attack as number,
        decay: decay as number,
        sustain: sustain as number,
        release: release as number,
      },
    };
  };
}

function square1(typeCode: number, name: string): MacroParser {
  return (args, lineNumber, _comment, diagnostics) => {
    const parsed = parseCommaArgs(args, 8, lineNumber, "voice_square_1", diagnostics);
    if (!parsed) return null;
    return cgbSlot(parsed.slice(4, 8), lineNumber, name, typeCode, diagnostics);
  };
}

function square2(typeCode: number, name: string): MacroParser {
  return (args, lineNumber, _comment, diagnostics) => {
    const parsed = parseCommaArgs(args, 7, lineNumber, "voice_square_2", diagnostics);
    if (!parsed) return null;
    return cgbSlot(parsed.slice(3, 7), lineNumber, name, typeCode, diagnostics);
  };
}

function programmableWave(typeCode: number): MacroParser {
  return (args, lineNumber, _comment, diagnostics) => {
    const parsed = parseProgrammableWaveArgs(args, lineNumber, diagnostics);
    if (!parsed) return null;
    return cgbSlot(parsed.slice(3, 7), lineNumber, parsed[2], typeCode, diagnostics);
  };
}

function noise(typeCode: number, name: string): MacroParser {
  return (args, lineNumber, _comment, diagnostics) => {
    const parsed = parseCommaArgs(args, 7, lineNumber, "voice_noise", diagnostics);
    if (!parsed) return null;
    return cgbSlot(parsed.slice(3, 7), lineNumber, name, typeCode, diagnostics);
  };
}

function cgbSlot(
  fields: string[],
  lineNumber: number,
  name: string,
  typeCode: number,
  diagnostics: string[],
): VoiceSlot | null {
  const attack = parseNumericField(fields[0], lineNumber, "attack", diagnostics);
  const decay = parseNumericField(fields[1], lineNumber, "decay", diagnostics);
  const sustain = parseNumericField(fields[2], lineNumber, "sustain", diagnostics);
  const release = parseNumericField(fields[3], lineNumber, "release", diagnostics);
  if ([attack, decay, sustain, release].some((value) => value === null)) return null;
  return {
    program: -1,
    name,
    typeCode,
    envelope: {
      attack: (attack as number) & 0x07,
      decay: (decay as number) & 0x07,
      sustain: (sustain as number) & 0x0f,
      release: (release as number) & 0x07,
    },
  };
}

function keysplit(
  args: string,
  lineNumber: number,
  comment: string,
  diagnostics: string[],
): VoiceSlot | null {
  const parsed = parseCommaArgs(args, 2, lineNumber, "voice_keysplit", diagnostics);
  if (!parsed) return null;
  return {
    program: -1,
    name: comment || voicegroupNameFromKeysplitArgs(parsed),
    typeCode: VOICE_KEYSPLIT,
    envelope: null,
  };
}

function keysplitAll(
  args: string,
  lineNumber: number,
  comment: string,
  diagnostics: string[],
): VoiceSlot | null {
  const name = args.trim().split(/\s+/)[0] ?? "";
  if (!name) {
    diagnostics.push(`line ${lineNumber}: voice_keysplit_all expects one argument`);
    return null;
  }
  return {
    program: -1,
    name: comment || name,
    typeCode: VOICE_KEYSPLIT_ALL,
    envelope: null,
  };
}

function cry(typeCode: number): MacroParser {
  return (args, lineNumber, _comment, diagnostics) => {
    const name = args.trim().split(/\s+/)[0] ?? "";
    if (!name) {
      diagnostics.push(`line ${lineNumber}: cry expects one argument`);
      return null;
    }
    return {
      program: -1,
      name,
      typeCode,
      envelope: { attack: 0xff, decay: 0, sustain: 0xff, release: 0 },
    };
  };
}

function voicegroupNameFromKeysplitArgs(args: string[]): string {
  return args.find((arg) => arg.startsWith("voicegroup_")) ?? args[1] ?? args[0];
}

function parseCommaArgs(
  args: string,
  expectedCount: number,
  lineNumber: number,
  macro: string,
  diagnostics: string[],
): string[] | null {
  const parsed = args.split(",").map((arg) => arg.trim()).filter(Boolean);
  if (parsed.length !== expectedCount) {
    diagnostics.push(
      `line ${lineNumber}: ${macro} expects ${expectedCount} arguments, got ${parsed.length}`,
    );
    return null;
  }
  return parsed;
}

function parseProgrammableWaveArgs(
  args: string,
  lineNumber: number,
  diagnostics: string[],
) {
  const parsed = args.split(",").map((arg) => arg.trim()).filter(Boolean);
  if (parsed.length === 6) {
    const fixedWaveAndAttack = splitMissingWaveAttackComma(parsed[2]);
    if (fixedWaveAndAttack) parsed.splice(2, 1, ...fixedWaveAndAttack);
  }
  if (parsed.length !== 7) {
    diagnostics.push(
      `line ${lineNumber}: voice_programmable_wave expects 7 arguments, got ${parsed.length}`,
    );
    return null;
  }
  return parsed;
}

function splitMissingWaveAttackComma(field: string) {
  const match = field.match(/^(\S+)\s+([-+]?(?:\d+|0x[0-9a-f]+))$/i);
  if (!match) return null;
  return [match[1], match[2]];
}

function parseNumericField(
  raw: string,
  lineNumber: number,
  field: string,
  diagnostics: string[],
): number | null {
  const parsed = parseInteger(raw);
  if (parsed === null) {
    diagnostics.push(`line ${lineNumber}: ${field} must be an integer`);
  }
  return parsed;
}

function parseInteger(raw: string): number | null {
  const trimmed = raw.trim();
  if (!/^[-+]?(?:\d+|0x[0-9a-f]+)$/i.test(trimmed)) return null;
  const value = Number(trimmed);
  if (!Number.isInteger(value)) return null;
  return value;
}
