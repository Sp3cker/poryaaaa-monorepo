export interface VoiceSlot {
  name: string;
  typeCode: number;
}

export interface VoicegroupDiagnostic {
  severity: "error" | "warning";
  code: string;
  message: string;
  startLine: number;
  startColumn: number;
  endLine: number;
  endColumn: number;
}

export type VoicegroupParseResult =
  | {
      ok: true;
      slots: Array<VoiceSlot | null>;
      diagnostics: VoicegroupDiagnostic[];
      sourcePath: string;
    }
  | {
      ok: false;
      diagnostics: VoicegroupDiagnostic[];
    };

export function scanVoicegroupBanks(root: string): string[];

export function parseVoicegroup(root: string, bank: string): VoicegroupParseResult;
