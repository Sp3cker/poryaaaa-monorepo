const assert = require("node:assert/strict");
const { mkdtempSync, mkdirSync, rmSync, writeFileSync } = require("node:fs");
const { join } = require("node:path");
const { tmpdir } = require("node:os");
const test = require("node:test");

const { parseVoicegroup, scanVoicegroupBanks } = require("..");

// Creates the minimal project tree voicegroup-core expects for index-based bank loading.

function makeProject(name) {
  const root = mkdtempSync(join(tmpdir(), `${name}-`));
  mkdirSync(join(root, "sound", "voicegroups"), { recursive: true });
  return root;
}

// Writes fixture files relative to the synthetic project root.

function writeProjectFile(root, relativePath, contents) {
  writeFileSync(join(root, relativePath), contents);
}

test("scanVoicegroupBanks returns only banks declared by sound/voice_groups.inc", () => {
  const root = makeProject("voicegroup-core-node-scan");
  try {
    writeProjectFile(root, "sound/voice_groups.inc", '.include "sound/voicegroups/petalburg.inc"\n');
    writeProjectFile(
      root,
      "sound/voicegroups/petalburg.inc",
      "voice_group petalburg\n\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3\n",
    );
    writeProjectFile(
      root,
      "sound/voicegroups/unlisted.inc",
      "voice_group unlisted\n\tvoice_square_2 60, 0, 2, 1, 2, 8, 3\n",
    );

    assert.deepEqual(scanVoicegroupBanks(root), ["petalburg"]);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("parseVoicegroup returns 128 slots with M4L-compatible names and type codes", () => {
  const root = makeProject("voicegroup-core-node-parse");
  try {
    writeProjectFile(root, "sound/voice_groups.inc", '.include "sound/voicegroups/petalburg.inc"\n');
    writeProjectFile(
      root,
      "sound/voicegroups/petalburg.inc",
      "voice_group petalburg\n" +
        "\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3\n" +
        "\tvoice_square_2 60, 0, 2, 1, 2, 8, 3\n" +
        "\tvoice_noise 61, 0, 1, 1, 2, 8, 3\n",
    );

    const result = parseVoicegroup(root, "petalburg");

    assert.equal(result.ok, true);
    assert.equal(result.sourcePath, "sound/voicegroups/petalburg.inc");
    assert.deepEqual(result.diagnostics, []);
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

test("parseVoicegroup reports diagnostics for missing project-index banks", () => {
  const root = makeProject("voicegroup-core-node-missing");
  try {
    writeProjectFile(root, "sound/voice_groups.inc", "");

    const result = parseVoicegroup(root, "missing");

    assert.equal(result.ok, false);
    assert.equal(Object.hasOwn(result, "slots"), false);
    assert.equal(result.diagnostics.length, 1);
    assert.equal(result.diagnostics[0].code, "missing-voicegroup");
    assert.match(result.diagnostics[0].message, /project index/);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});
