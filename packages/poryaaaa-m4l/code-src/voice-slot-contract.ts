export interface VoiceSlot {
  name: string;
  typeCode: number;
}

export interface VoicegroupSnapshotFrame {
  type: "snapshot";
  slots: Array<VoiceSlot | null>;
}
