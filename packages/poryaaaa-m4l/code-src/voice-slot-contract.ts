// todo: No way this needs to be it's own file.
export interface VoiceSlot {
  name: string;
  typeCode: number;
}
// State file (projects.json). Un-read by Max, used by Rust/C++ plugins.
export interface VoicegroupState {
  root: string;
  bank: string;
}

export interface VoicegroupSnapshotFrame {
  type: "snapshot";
  slots: Array<VoiceSlot | null>;
}
