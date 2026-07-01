### Task 2: Replace the M4L parser adapter with voicegroup-core-node

**Files:**
- Modify: `packages/poryaaaa-m4l/code-src/test/poryaaaa_node_voicegroup_parser.test.ts`
- Modify: `packages/poryaaaa-m4l/code-src/poryaaaa-node/voicegroup-parser.ts`

**Interfaces:**
- Consumes: `@poryaaaa/voicegroup-core-node` functions:
  - `scanVoicegroupBanks(root: string): string[]`
  - `parseVoicegroup(root: string, bank: string): CoreVoicegroupParseResult`
- Produces unchanged M4L-local interface:
  - `scanVoicegroupBanks(root: string): string[]`
  - `parseVoicegroup(root: string, bank: string): { ok: true; slots: Array<VoiceSlot | null> } | { ok: false; diagnostics: string[] }`

- [ ] **Step 1: Update parser tests for voicegroup-core project semantics**

Replace `packages/poryaaaa-m4l/code-src/test/poryaaaa_node_voicegroup_parser.test.ts` with:

```ts
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
```

- [ ] **Step 2: Run the parser tests and verify they fail**

Run from `packages/poryaaaa-m4l`:

```bash
npm test -- code-src/test/poryaaaa_node_voicegroup_parser.test.ts
```

Expected: FAIL because the current implementation scans all `.inc` files directly and still requires the old `poryaaaa_voicegroup.node` adapter.

- [ ] **Step 3: Replace `voicegroup-parser.ts` implementation**

Replace `packages/poryaaaa-m4l/code-src/poryaaaa-node/voicegroup-parser.ts` with:

```ts
import {
  parseVoicegroup as parseCoreVoicegroup,
  scanVoicegroupBanks as scanCoreVoicegroupBanks,
  type VoicegroupDiagnostic,
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
  } catch (_) {
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
    return { ok: false, diagnostics: diagnosticMessages(result.diagnostics) };
  }

  return {
    ok: true,
    slots: result.slots.map((slot) => slot ? { name: slot.name, typeCode: slot.typeCode } : null),
  };
}

function diagnosticMessages(diagnostics: VoicegroupDiagnostic[]): string[] {
  if (diagnostics.length === 0) return ["voicegroup-core returned no loadable bank"];
  return diagnostics.map((diagnostic) => {
    const location = `${diagnostic.startLine}:${diagnostic.startColumn}`;
    return `${diagnostic.code} at ${location}: ${diagnostic.message}`;
  });
}
```

- [ ] **Step 4: Run the parser tests and verify they pass**

Run from `packages/poryaaaa-m4l`:

```bash
npm test -- code-src/test/poryaaaa_node_voicegroup_parser.test.ts
```

Expected: PASS for all parser adapter tests.

---

