// todo: No way this needs to be it's own file.
export interface VoiceSlot {
  name: string;
  typeCode: number;
}

export interface VoicegroupSnapshotFrame {
  type: "snapshot";
  slots: Array<VoiceSlot | null>;
}
