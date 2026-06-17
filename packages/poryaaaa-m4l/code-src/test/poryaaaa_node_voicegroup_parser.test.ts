import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, mkdirSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

import {
  parseVoicegroup,
  scanVoicegroupBanks,
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

test("parseVoicegroup forwards native labels and type codes in a 128-slot array", () => {
  const result = parseVoicegroup(
    join(process.cwd(), "tests", "fixtures", "decomp_sample"),
    "voicegroup001",
  );

  assert.equal(result.ok, true);
  if (!result.ok) return;
  assert.equal(result.slots.length, 128);
  assert.deepEqual(result.slots.slice(0, 7), [
    { name: "DirectSoundWaveData_PianoC4", typeCode: 0x00 },
    { name: "Square 1", typeCode: 0x01 },
    { name: "Square 2", typeCode: 0x02 },
    { name: "Noise", typeCode: 0x04 },
    { name: "ProgWave", typeCode: 0x03 },
    { name: "voicegroup_drumkit", typeCode: 0x40 },
    { name: "voicegroup002", typeCode: 0x80 },
  ]);
  assert.equal(result.slots[7], null);
});

test("parseVoicegroup preserves source slot indexes as null holes", () => {
  const root = tempProject();
  writeVoicegroup(root, "offset", `
offset::
  voice_group voicegroup001, 24
  voice_directsound 60, 0, DirectSoundWaveData_PianoC4, 255, 165, 245, 165
`);

  const result = parseVoicegroup(root, "offset");

  assert.equal(result.ok, true);
  if (!result.ok) return;
  assert.equal(result.slots.length, 128);
  assert.equal(result.slots[0], null);
  assert.deepEqual(result.slots[24], {
    name: "DirectSoundWaveData_PianoC4",
    typeCode: 0x00,
  });
});

test("parseVoicegroup returns diagnostics for malformed voice macros", () => {
  const root = tempProject();
  writeVoicegroup(root, "bad", `
bad::
  voice_directsound 60, 0, MissingEnvelopeArgs
  voice_directsounnd 60, 0, Typo, 1, 2, 3, 4
`);

  const result = parseVoicegroup(root, "bad");

  assert.equal(result.ok, false);
  if (result.ok) return;
  assert.ok(result.diagnostics.length > 0);
});
