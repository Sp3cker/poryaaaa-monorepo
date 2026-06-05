import test from "node:test";
import assert from "node:assert/strict";
import WebSocket from "ws";

import {
  PoryaaaaVoicegroupService,
  type CcomidiSnapshot,
  type PoryaaaaVoicegroupOutput,
} from "../poryaaaa-node/voicegroup-service";
import type { VoicegroupState } from "../poryaaaa-node/project-store";

const SNAPSHOT: CcomidiSnapshot = {
  slots: [{
    program: 0,
    name: "Lead",
    typeCode: 0,
    envelope: { attack: 1, decay: 2, sustain: 3, release: 4 },
  }],
};

async function openClient(url: string): Promise<WebSocket> {
  const ws = new WebSocket(url);
  await new Promise<void>((resolve, reject) => {
    ws.once("open", resolve);
    ws.once("error", reject);
  });
  return ws;
}

async function waitForOpen(ws: WebSocket): Promise<void> {
  if (ws.readyState === WebSocket.OPEN) return;
  await new Promise<void>((resolve, reject) => {
    ws.once("open", resolve);
    ws.once("error", reject);
  });
}

function nextJson(ws: WebSocket): Promise<unknown> {
  return new Promise((resolve, reject) => {
    ws.once("message", (data) => {
      try {
        resolve(JSON.parse(String(data)));
      } catch (err) {
        reject(err);
      }
    });
    ws.once("error", reject);
  });
}

function closeClient(ws: WebSocket): Promise<void> {
  if (ws.readyState === WebSocket.CLOSED) return Promise.resolve();
  return new Promise((resolve) => {
    ws.once("close", () => resolve());
    ws.close();
  });
}

function serviceHarness() {
  const outputs: PoryaaaaVoicegroupOutput[] = [];
  let state: VoicegroupState | null = null;
  const service = new PoryaaaaVoicegroupService({
    scanBanks: () => ["alpha"],
    parseVoicegroup: () => ({ ok: true, slots: SNAPSHOT.slots }),
    readVoicegroupState: () => state,
    writeVoicegroupState: (nextState) => {
      state = nextState;
    },
    output: (out) => outputs.push(out),
    post: () => {},
    websocketHost: "127.0.0.1",
    websocketPort: 0,
  });
  return { service, outputs };
}

test("server listens on an ephemeral test port and sends latest snapshot to a newly connected root client", async () => {
  const { service } = serviceHarness();
  assert.equal(service.isWebSocketListening(), false);
  await service.startWebSocket();
  assert.equal(service.isWebSocketListening(), true);
  try {
    service.rawroot("/p");
    service.bankselect("alpha");
    const ws = new WebSocket(service.websocketUrl());
    const msg = nextJson(ws);
    await waitForOpen(ws);
    try {
      assert.deepEqual(await msg, {
        type: "snapshot",
        slots: SNAPSHOT.slots,
      });
    } finally {
      await closeClient(ws);
    }
  } finally {
    await service.closeWebSocket();
  }
  assert.equal(service.isWebSocketListening(), false);
});

test("server accepts local websocket clients without path filtering", async () => {
  const { service } = serviceHarness();
  await service.startWebSocket();
  let ws: WebSocket | null = null;
  try {
    ws = await openClient(`${service.websocketUrl()}not-root`);
    assert.equal(ws.readyState, WebSocket.OPEN);
  } finally {
    if (ws) await closeClient(ws);
    await service.closeWebSocket();
  }
});

test("new snapshots broadcast to all connected clients and inbound client messages no-op", async () => {
  const { service } = serviceHarness();
  await service.startWebSocket();
  const a = await openClient(service.websocketUrl());
  const b = await openClient(service.websocketUrl());
  try {
    a.send(JSON.stringify({ type: "client-message" }));
    const aMsg = nextJson(a);
    const bMsg = nextJson(b);
    service.rawroot("/p");
    service.bankselect("alpha");

    assert.deepEqual(await aMsg, {
      type: "snapshot",
      slots: SNAPSHOT.slots,
    });
    assert.deepEqual(await bMsg, {
      type: "snapshot",
      slots: SNAPSHOT.slots,
    });
  } finally {
    await Promise.all([closeClient(a), closeClient(b)]);
    await service.closeWebSocket();
  }
});

test("clearSnapshot removes retained snapshot for late joiners", async () => {
  const { service } = serviceHarness();
  await service.startWebSocket();
  try {
    service.rawroot("/p");
    service.bankselect("alpha");
    assert.deepEqual(service.latestSnapshot(), SNAPSHOT);

    service.rawroot("/other");

    assert.equal(service.latestSnapshot(), null);
  } finally {
    await service.closeWebSocket();
  }
});
