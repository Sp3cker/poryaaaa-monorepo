import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, mkdirSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

import {
  parseVoicegroup,
  parseVoicegroupSource,
  scanVoicegroupBanks,
  validateVoiceSlots,
} from "../poryaaaa-node/voicegroup-parser";

function tempProject(): string {
  const root = mkdtempSync(join(tmpdir(), "poryaaaa-vg-"));
  mkdirSync(join(root, "sound", "voicegroups"), { recursive: true });
  return root;
}

function writeVoicegroup(root: string, bank: string, body: string): void {
  writeFileSync(join(root, "sound", "voicegroups", `${bank}.inc`), body);
}

test("scanVoicegroupBanks lists non-se .inc banks only, preserving display case and sorting case-insensitively", () => {
  const root = tempProject();
  const dir = join(root, "sound", "voicegroups");
  writeFileSync(join(dir, "Beta.inc"), "");
  writeFileSync(join(dir, "alpha.inc"), "");
  writeFileSync(join(dir, "voicegroup.s"), "");
  writeFileSync(join(dir, "se_hidden.inc"), "");
  writeFileSync(join(dir, "notes.txt"), "");

  assert.deepEqual(scanVoicegroupBanks(root), ["alpha", "Beta"]);
});

test("parseVoicegroup emits compact ccomidi slots for the voice macro forms poryaaaa uses", () => {
  const result = parseVoicegroup(
    join(process.cwd(), "tests", "fixtures", "decomp_sample"),
    "voicegroup001",
  );

  assert.equal(result.ok, true);
  if (!result.ok) return;
  assert.deepEqual(result.slots, [
    {
      program: 0,
      name: "DirectSoundWaveData_PianoC4",
      typeCode: 0x00,
      envelope: { attack: 255, decay: 165, sustain: 245, release: 165 },
    },
    {
      program: 1,
      name: "Square 1",
      typeCode: 0x01,
      envelope: { attack: 5, decay: 5, sustain: 5, release: 0 },
    },
    {
      program: 2,
      name: "Square 2",
      typeCode: 0x02,
      envelope: { attack: 5, decay: 5, sustain: 5, release: 0 },
    },
    {
      program: 3,
      name: "Noise",
      typeCode: 0x04,
      envelope: { attack: 5, decay: 5, sustain: 5, release: 0 },
    },
    {
      program: 4,
      name: "ProgrammableWaveData_Sine",
      typeCode: 0x03,
      envelope: { attack: 5, decay: 5, sustain: 5, release: 0 },
    },
    { program: 5, name: "voicegroup_drumkit", typeCode: 0x40, envelope: null },
    { program: 6, name: "voicegroup002", typeCode: 0x80, envelope: null },
  ]);
});

test("parseVoicegroup uses a trailing comment as the keysplit display name", () => {
  const root = tempProject();
  writeVoicegroup(root, "with_comment", `
with_comment::
  voice_keysplit KeySplitTable_Drumkit, voicegroup_drumkit @ Drums
`);

  const result = parseVoicegroup(root, "with_comment");

  assert.equal(result.ok, true);
  if (!result.ok) return;
  assert.deepEqual(result.slots, [
    { program: 0, name: "Drums", typeCode: 0x40, envelope: null },
  ]);
});

test("parseVoicegroup accepts voice_group declarations without a starting offset", () => {
  const root = tempProject();
  writeVoicegroup(root, "modern", `
voice_group modern
  voice_directsound 60, 0, DirectSoundWaveData_PianoC4, 255, 165, 245, 165
`);

  const result = parseVoicegroup(root, "modern");

  assert.equal(result.ok, true);
  if (!result.ok) return;
  assert.deepEqual(result.slots, [
    {
      program: 0,
      name: "DirectSoundWaveData_PianoC4",
      typeCode: 0x00,
      envelope: { attack: 255, decay: 165, sustain: 245, release: 165 },
    },
  ]);
});

test("parseVoicegroup rejects malformed voice macro syntax and preserves previous state opportunity", () => {
  const root = tempProject();
  writeVoicegroup(root, "bad", `
bad::
  voice_directsound 60, 0, MissingEnvelopeArgs
`);

  const result = parseVoicegroup(root, "bad");

  assert.equal(result.ok, false);
  if (result.ok) return;
  assert.match(result.diagnostics.join("\n"), /voice_directsound/i);
});

test("parseVoicegroup accepts voice_group starting offsets while keeping ccomidi slots compact", () => {
  const root = tempProject();
  writeVoicegroup(root, "offset", `
offset::
  voice_group voicegroup001, 24
  voice_directsound 60, 0, DirectSoundWaveData_PianoC4, 255, 165, 245, 165
`);

  const result = parseVoicegroup(root, "offset");

  assert.equal(result.ok, true);
  if (!result.ok) return;
  assert.deepEqual(result.slots.map((slot) => slot.program), [0]);
});

test("parseVoicegroup accepts legacy programmable-wave lines missing the wave/attack comma", () => {
  const result = parseVoicegroupSource(`
    .align 2
voicegroup191:: @ DPPt main									@ dummy
	voice_directsound 60, 0, DirectSoundWaveData_dp_reverscyn_16, 255, 0, 255, 16
	voice_programmable_wave 60, 0, ProgrammableWaveData_5 0, 200, 53, 1 @ love song
	voice_keysplit_all voicegroup192				 @ drums 1
`, "voicegroup191");

  assert.equal(result.ok, true);
  if (!result.ok) return;
  assert.deepEqual(result.slots, [
    {
      program: 0,
      name: "DirectSoundWaveData_dp_reverscyn_16",
      typeCode: 0x00,
      envelope: { attack: 255, decay: 0, sustain: 255, release: 16 },
    },
    {
      program: 1,
      name: "ProgrammableWaveData_5",
      typeCode: 0x03,
      envelope: { attack: 0, decay: 0, sustain: 5, release: 1 },
    },
    { program: 2, name: "drums 1", typeCode: 0x80, envelope: null },
  ]);
});

test("validateVoiceSlots rejects producer bugs before WebSocket broadcast", () => {
  assert.deepEqual(validateVoiceSlots([
    {
      program: 0,
      name: "ok",
      typeCode: 0,
      envelope: { attack: 1, decay: 2, sustain: 3, release: 4 },
    },
    { program: 1, name: "split", typeCode: 0x40, envelope: null },
  ]), [
    {
      program: 0,
      name: "ok",
      typeCode: 0,
      envelope: { attack: 1, decay: 2, sustain: 3, release: 4 },
    },
    { program: 1, name: "split", typeCode: 0x40, envelope: null },
  ]);

  assert.throws(() => validateVoiceSlots([
    { program: 0, name: "missing type", envelope: null },
  ]), /typeCode/);
  assert.throws(() => validateVoiceSlots([
    { program: 0, name: "bad envelope", typeCode: 0, envelope: { attack: 1 } },
  ]), /envelope/);
  assert.throws(() => validateVoiceSlots([]), /empty/);
});
