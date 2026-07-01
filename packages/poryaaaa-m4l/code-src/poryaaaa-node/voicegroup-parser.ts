import {
  parseVoicegroup as parseCoreVoicegroup,
  scanVoicegroupBanks as scanCoreVoicegroupBanks,
  type VoicegroupParseResult as CoreVoicegroupParseResult,
} from "@poryaaaa/voicegroup-core-node";

import type { VoiceSlot } from "../voice-slot-contract";
export type { VoiceSlot } from "../voice-slot-contract";

export type VoicegroupParseResult =
  | { ok: true; slots: Array<VoiceSlot | null> }
  | { ok: false; diagnostics: string[] };

// Lists project-indexed voicegroup banks for the poryaaaa Max device menu.
export function scanVoicegroupBanks(root: string): string[] {
  try {
    return scanCoreVoicegroupBanks(root);
  } catch {
    return [];
  }
}

// Loads a bank through voicegroup-core-node and returns the small M4L slot contract.
export function parseVoicegroup(root: string, bank: string): VoicegroupParseResult {
  const bankName = bank.endsWith(".inc") ? bank.slice(0, -".inc".length) : bank;
  let result: CoreVoicegroupParseResult;
  try {
    result = parseCoreVoicegroup(root, bankName);
  } catch (err) {
    return { ok: false, diagnostics: [String(err)] };
  }

  if (!result.ok) {
    return {
      ok: false,
      diagnostics:
        result.diagnostics.length === 0
          ? ["voicegroup-core returned no loadable bank"]
          : result.diagnostics.map((diagnostic) => {
              const location = `${diagnostic.startLine}:${diagnostic.startColumn}`;
              return `${diagnostic.code} at ${location}: ${diagnostic.message}`;
            }),
    };
  }

  return {
    ok: true,
    slots: result.slots.map((slot) =>
      slot ? { name: slot.name, typeCode: slot.typeCode } : null,
    ),
  };
}
