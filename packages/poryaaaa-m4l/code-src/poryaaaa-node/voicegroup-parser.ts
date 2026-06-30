import { existsSync, readdirSync } from "node:fs";
import { join } from "node:path";

import type { VoiceSlot } from "../voice-slot-contract";
export type { VoiceSlot } from "../voice-slot-contract";

export type VoicegroupParseResult =
  | { ok: true; slots: Array<VoiceSlot | null> }
  | { ok: false; diagnostics: string[] };

interface NativeVoicegroupParser {
  parseVoicegroup: (root: string, bank: string) => VoicegroupParseResult;
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


export function parseVoicegroup(root: string, bank: string): VoicegroupParseResult {
  const res = native().parseVoicegroup(root, bank.endsWith(".inc") ? bank.slice(0, -4) : bank);

  return res
}

function native(): NativeVoicegroupParser {
  const bundled = join(__dirname, "poryaaaa_voicegroup.node");
  if (existsSync(bundled)) return require(bundled) as NativeVoicegroupParser;

  const testPath = join(__dirname, "..", "..", "javascript", "poryaaaa_voicegroup.node");
  if (existsSync(testPath)) return require(testPath) as NativeVoicegroupParser;

  return require(bundled) as NativeVoicegroupParser;
}
