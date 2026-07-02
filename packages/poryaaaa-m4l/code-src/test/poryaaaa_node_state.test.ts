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
import type { VoicegroupState } from "../poryaaaa-node/project-store";

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
  state?: VoicegroupState | null;
} = {}) {
  const outputs: PoryaaaaVoicegroupOutput[] = [];
  const posts: CcomidiVoicegroupFrame[] = [];
  const logs: string[] = [];
  let state = args.state ?? null;
  const writes: VoicegroupState[] = [];
  const service = new PoryaaaaVoicegroupService({
    scanBanks: (root) => args.banks?.[root] ?? [],
    parseVoicegroup: (root, bank) =>
      args.parses?.[`${root}/${bank}`] ?? bad(`missing parse fixture ${root}/${bank}`),
    readVoicegroupState: () => state,
    writeVoicegroupState: (nextState) => {
      writes.push(nextState);
      state = nextState;
    },
    output: (out) => outputs.push(out),
    post: (frame: CcomidiVoicegroupFrame) => posts.push(frame),
    log: (msg: string) => logs.push(msg),
  });
  return { service, outputs, posts, logs, get state() { return state; }, writes };
}

function outputArgs(outputs: PoryaaaaVoicegroupOutput[]): unknown[][] {
  return outputs.map((out) => [out.tag, ...out.args]);
}

test("restore loads saved root/bank, validates bank, emits UI without setting bank symbol, voicegroup, and snapshot", () => {
  const h = harness({
    banks: { "/p": ["alpha", "beta"] },
    parses: { "/p/beta": ok() },
    state: { root: "/p", bank: "beta" },
  });

  h.service.restore();

  assert.deepEqual(outputArgs(h.outputs), [
    ["path", "set", "/p"],
    ["bank", "clear"],
    ["bank", "append", "alpha"],
    ["bank", "append", "beta"],
    ["voicegroup", "/p", "beta"],
  ]);
  assert.deepEqual(h.service.latestSnapshot(), { slots: [SLOT] });
  assert.deepEqual(h.writes.at(-1), { root: "/p", bank: "beta" });
});

test("rawroot scans and persists the root without validating every bank", () => {
  const h = harness({ banks: { "/p": ["alpha", "beta"] } });

  h.service.rawroot("Macintosh HD:/p/");

  assert.deepEqual(outputArgs(h.outputs), [
    ["path", "set", "/p"],
    ["bank", "clear"],
    ["bank", "append", "alpha"],
    ["bank", "append", "beta"],
  ]);
  assert.deepEqual(h.writes.at(-1), { root: "/p", bank: "" });
  assert.equal(h.service.latestSnapshot(), null);
});

test("bankselect validates before persisting, emitting voicegroup, and broadcasting", () => {
  const h = harness({
    banks: { "/p": ["alpha"] },
    parses: { "/p/alpha": ok() },
  });
  h.service.rawroot("/p");
  h.outputs.length = 0;

  h.service.bankselect("alpha");

  assert.deepEqual(outputArgs(h.outputs), [["voicegroup", "/p", "alpha"]]);
  assert.deepEqual(h.service.latestSnapshot(), { slots: [SLOT] });
  assert.deepEqual(h.writes.at(-1), { root: "/p", bank: "alpha" });
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
  assert.deepEqual(h.outputs, []);
  assert.equal(h.service.latestSnapshot(), null);
  assert.match(h.logs.join("\n"), /bad voice macro/);
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

  assert.deepEqual(h.outputs, []);
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

  assert.deepEqual(h.outputs, []);
  assert.deepEqual(h.posts, []);
  assert.deepEqual(h.service.latestSnapshot(), previous);
  assert.match(h.logs.join("\n"), /malformed voice_directsound/);
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

test("invalid saved bank logs a diagnostic and does not emit voicegroup", () => {
  const h = harness({
    banks: { "/p": ["alpha", "beta"] },
    parses: { "/p/beta": bad("bad saved bank") },
    state: { root: "/p", bank: "beta" },
  });

  h.service.restore();

  assert.deepEqual(
    outputArgs(h.outputs).filter((row) => row[0] === "voicegroup"),
    [],
  );
  assert.equal(h.service.latestSnapshot(), null);
  assert.match(h.logs.join("\n"), /bad saved bank/);
});

test("stale saved bank restores root menu but does not emit voicegroup", () => {
  const h = harness({
    banks: { "/p": ["alpha"] },
    state: { root: "/p", bank: "missing" },
  });

  h.service.restore();

  assert.deepEqual(outputArgs(h.outputs), [
    ["path", "set", "/p"],
    ["bank", "clear"],
    ["bank", "append", "alpha"],
  ]);
  assert.equal(h.service.latestSnapshot(), null);
  assert.match(h.logs.join("\n"), /not found/);
});

test("restore with no saved state emits the empty project UI", () => {
  const h = harness();
  h.service.restore();

  assert.equal(h.service.latestSnapshot(), null);
  assert.deepEqual(outputArgs(h.outputs), [
    ["path", "set", "(no project)"],
    ["bank", "clear"],
    ["bank", "append", "(no project loaded)"],
  ]);
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
