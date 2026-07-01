import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
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

function writeProjectFile(root: string, relativePath: string, body: string): void {
  writeFileSync(join(root, relativePath), body);
}

test("scanVoicegroupBanks lists banks declared by sound/voice_groups.inc", () => {
  const root = tempProject();
  try {
    writeProjectFile(root, "sound/voice_groups.inc", '.include "sound/voicegroups/alpha.inc"\n');
    writeProjectFile(root, "sound/voicegroups/alpha.inc", "voice_group alpha\n  voice_square_1 60, 0, 0, 2, 1, 2, 8, 3\n");
    writeProjectFile(root, "sound/voicegroups/unlisted.inc", "voice_group unlisted\n  voice_square_2 60, 0, 2, 1, 2, 8, 3\n");

    assert.deepEqual(scanVoicegroupBanks(root), ["alpha"]);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("parseVoicegroup returns M4L-compatible names and type codes in a 128-slot array", () => {
  const root = tempProject();
  try {
    writeProjectFile(root, "sound/voice_groups.inc", '.include "sound/voicegroups/alpha.inc"\n');
    writeProjectFile(
      root,
      "sound/voicegroups/alpha.inc",
      "voice_group alpha\n" +
        "  voice_square_1 60, 0, 0, 2, 1, 2, 8, 3\n" +
        "  voice_square_2 60, 0, 2, 1, 2, 8, 3\n" +
        "  voice_noise 61, 0, 1, 1, 2, 8, 3\n",
    );

    const result = parseVoicegroup(root, "alpha");

    assert.equal(result.ok, true);
    if (!result.ok) return;
    assert.equal(result.slots.length, 128);
    assert.deepEqual(result.slots.slice(0, 4), [
      { name: "Square 1", typeCode: 0x01 },
      { name: "Square 2", typeCode: 0x02 },
      { name: "Noise", typeCode: 0x04 },
      null,
    ]);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("parseVoicegroup strips a .inc suffix before asking voicegroup-core for the bank", () => {
  const root = tempProject();
  try {
    writeProjectFile(root, "sound/voice_groups.inc", '.include "sound/voicegroups/alpha.inc"\n');
    writeProjectFile(root, "sound/voicegroups/alpha.inc", "voice_group alpha\n  voice_square_1 60, 0, 0, 2, 1, 2, 8, 3\n");

    const result = parseVoicegroup(root, "alpha.inc");

    assert.equal(result.ok, true);
    if (!result.ok) return;
    assert.equal(result.slots[0]?.name, "Square 1");
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("parseVoicegroup returns diagnostic strings for missing project-index banks", () => {
  const root = tempProject();
  try {
    writeProjectFile(root, "sound/voice_groups.inc", "");

    const result = parseVoicegroup(root, "missing");

    assert.equal(result.ok, false);
    if (result.ok) return;
    assert.ok(result.diagnostics.length > 0);
    assert.match(result.diagnostics[0], /missing|project index/i);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});
