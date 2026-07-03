import test from "node:test";
import assert from "node:assert/strict";

import {
  PoryaaaaVoicegroupService,
  type CcomidiVoicegroupFrame,
  type PoryaaaaVoicegroupOutput,
} from "../poryaaaa-node/voicegroup-service";
import type {
  VoiceSlot,
  VoicegroupParseResult,
} from "../poryaaaa-node/voicegroup-parser";

const SLOT: VoiceSlot = {
  name: "Lead",
  typeCode: 0,
};

function ok(slots: Array<VoiceSlot | null> = [SLOT]): VoicegroupParseResult {
  return { ok: true, slots };
}

function bad(message: string): VoicegroupParseResult {
  return { ok: false, diagnostics: [message] };
}

function harness(args: {
  banks?: Record<string, string[]>;
  parses?: Record<string, VoicegroupParseResult>;
} = {}) {
  const outputs: PoryaaaaVoicegroupOutput[] = [];
  const posts: CcomidiVoicegroupFrame[] = [];
  const logs: string[] = [];
  const service = new PoryaaaaVoicegroupService({
    scanBanks: (root) => args.banks?.[root] ?? [],
    parseVoicegroup: (root, bank) =>
      args.parses?.[`${root}/${bank}`] ?? bad(`missing parse fixture ${root}/${bank}`),
    output: (out) => outputs.push(out),
    post: (frame: CcomidiVoicegroupFrame) => posts.push(frame),
    log: (msg: string) => logs.push(msg),
  });
  return { service, outputs, posts, logs };
}

function outputArgs(outputs: PoryaaaaVoicegroupOutput[]): unknown[][] {
  return outputs.map((out) => [out.tag, ...out.args]);
}


test("rawroot scans the root without validating every bank", () => {
  const h = harness({ banks: { "/p": ["alpha", "beta"] } });

  h.service.rawroot("Macintosh HD:/p/");

  assert.deepEqual(outputArgs(h.outputs), [
    ["path", "set", "/p"],
    ["bank", "clear"],
    ["bank", "append", "alpha"],
    ["bank", "append", "beta"],
  ]);
  assert.equal(h.service.latestSnapshot(), null);
});

test("bankselect validates before emitting voicegroup and broadcasting", () => {
  const h = harness({
    banks: { "/p": ["alpha"] },
    parses: { "/p/alpha": ok() },
  });
  h.service.rawroot("/p");
  h.outputs.length = 0;

  h.service.bankselect("alpha");

  assert.deepEqual(outputArgs(h.outputs), [["voicegroup", "/p", "alpha"]]);
  assert.deepEqual(h.service.latestSnapshot(), { slots: [SLOT] });
});

test("bankselect posts the ccomidi snapshot frame instead of websocket JSON text", () => {
  const h = harness({
    banks: { "/p": ["alpha"] },
    parses: { "/p/alpha": ok() },
  });
  h.service.rawroot("/p");
  h.posts.length = 0;

  h.service.bankselect("alpha");

  assert.deepEqual(h.posts, [{ type: "snapshot", slots: [SLOT] }]);
  assert.deepEqual(h.service.latestSnapshot(), { slots: [SLOT] });
});

test("bankselect posts unavailable frame when parsing fails with no previous snapshot", () => {
  const h = harness({
    banks: { "/p": ["bad"] },
    parses: { "/p/bad": bad("bad voice macro") },
  });
  h.service.rawroot("/p");
  h.posts.length = 0;
  h.logs.length = 0;
  h.outputs.length = 0;

  h.service.bankselect("bad");

  assert.deepEqual(h.posts, [{ type: "unavailable" }]);
  assert.deepEqual(outputArgs(h.outputs), [
    ["diag", "voicegroups: bad voice macro\n"],
    ["diag", "Bank not applied"],
  ]);
  assert.equal(h.service.latestSnapshot(), null);
});

test("bankselect ignores a duplicate selection for the already-loaded bank", () => {
  const h = harness({
    banks: { "/p": ["alpha"] },
    parses: { "/p/alpha": ok() },
  });
  h.service.rawroot("/p");
  h.service.bankselect("alpha");
  h.outputs.length = 0;
  h.posts.length = 0;

  h.service.bankselect("alpha");

  assert.deepEqual(outputArgs(h.outputs), [["diag", "Bank not applied"]]);
  assert.deepEqual(h.posts, []);
});

test("parse failure keeps previous snapshot and does not emit voicegroup or replacement frame", () => {
  const h = harness({
    banks: { "/p": ["alpha", "bad"] },
    parses: {
      "/p/alpha": ok(),
      "/p/bad": bad("line 2: malformed voice_directsound"),
    },
  });
  h.service.rawroot("/p");
  h.service.bankselect("alpha");
  const previous = h.service.latestSnapshot();
  h.outputs.length = 0;
  h.posts.length = 0;
  h.logs.length = 0;

  h.service.bankselect("bad");

  assert.deepEqual(outputArgs(h.outputs), [
    ["diag", "voicegroups: line 2: malformed voice_directsound\n"],
    ["diag", "Bank not applied"],
  ]);
  assert.deepEqual(h.posts, []);
  assert.deepEqual(h.service.latestSnapshot(), previous);
});

test("reload reparses and broadcasts even when root and bank are unchanged", () => {
  const h = harness({
    banks: { "/p": ["alpha"] },
    parses: { "/p/alpha": ok() },
  });
  h.service.rawroot("/p");
  h.service.bankselect("alpha");
  h.outputs.length = 0;

  h.service.reload();

  assert.deepEqual(outputArgs(h.outputs), [["voicegroup", "/p", "alpha"]]);
  assert.deepEqual(h.service.latestSnapshot(), { slots: [SLOT] });
});


test("changing roots clears the retained ccomidi snapshot before bank selection", () => {
  const h = harness({
    banks: { "/p": ["alpha"], "/other": ["beta"] },
    parses: { "/p/alpha": ok() },
  });
  h.service.rawroot("/p");
  h.service.bankselect("alpha");

  h.service.rawroot("/other");

  assert.equal(h.service.latestSnapshot(), null);
});
