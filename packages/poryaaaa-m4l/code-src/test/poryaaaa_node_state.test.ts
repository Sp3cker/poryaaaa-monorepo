import test from "node:test";
import assert from "node:assert/strict";

import {
  PoryaaaaVoicegroupService,
  type PoryaaaaVoicegroupOutput,
} from "../poryaaaa-node/voicegroup-service";
import type {
  VoiceSlot,
  VoicegroupParseResult,
} from "../poryaaaa-node/voicegroup-parser";
import type { VoicegroupState } from "../poryaaaa-node/project-store";

const SLOT: VoiceSlot = {
  program: 0,
  name: "Lead",
  typeCode: 0,
  envelope: { attack: 1, decay: 2, sustain: 3, release: 4 },
};

function ok(slots: VoiceSlot[] = [SLOT]): VoicegroupParseResult {
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
  const posts: string[] = [];
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
    post: (msg) => posts.push(msg),
  });
  return { service, outputs, posts, get state() { return state; }, writes };
}

function outputArgs(outputs: PoryaaaaVoicegroupOutput[]): unknown[][] {
  return outputs.map((out) => [out.tag, ...out.args]);
}

test("restore loads saved root/bank, validates bank, emits UI, voicegroup, and snapshot", () => {
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
    ["bank", "setsymbol", "beta"],
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

test("parse failure keeps previous snapshot and does not emit voicegroup or broadcast", () => {
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

  h.service.bankselect("bad");

  assert.deepEqual(h.outputs, []);
  assert.deepEqual(h.service.latestSnapshot(), previous);
  assert.match(h.posts.join("\n"), /malformed voice_directsound/);
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

test("invalid saved bank posts a diagnostic and does not emit or broadcast", () => {
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
  assert.match(h.posts.join("\n"), /bad saved bank/);
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
  assert.match(h.posts.join("\n"), /not found/);
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
