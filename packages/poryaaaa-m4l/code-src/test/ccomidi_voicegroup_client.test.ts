import test from "node:test";
import assert from "node:assert/strict";

import {
  CcomidiVoicegroupClient,
  type ClientWebSocket,
} from "../ccomidi_voicegroup_client";
import {
  PoryaaaaVoicegroupService,
} from "../poryaaaa-node/voicegroup-service";

const SLOT = {
  program: 0,
  name: "Lead",
  typeCode: 0,
  envelope: { attack: 1, decay: 2, sustain: 3, release: 4 },
};

class FakeSocket implements ClientWebSocket {
  closed = false;
  private listeners = new Map<string, Array<(event: unknown) => void>>();

  addEventListener(event: string, listener: (event: unknown) => void) {
    const listeners = this.listeners.get(event) ?? [];
    listeners.push(listener);
    this.listeners.set(event, listeners);
  }

  emit(event: string, payload?: unknown) {
    for (const listener of this.listeners.get(event) ?? []) listener(payload);
  }

  emitMessage(data: unknown) {
    this.emit("message", { data });
  }

  close() {
    this.closed = true;
    this.emit("close");
  }
}

function harness() {
  const sockets: FakeSocket[] = [];
  const outlets: unknown[][] = [];
  const posts: string[] = [];
  const timers: Array<{ delay: number; callback: () => void }> = [];
  const cleared: unknown[] = [];
  const client = new CcomidiVoicegroupClient({
    url: "ws://127.0.0.1:17777/",
    reconnectDelayMs: 25,
    outlet: (...args) => outlets.push(args),
    post: (msg) => posts.push(msg),
    webSocketFactory: () => {
      const socket = new FakeSocket();
      sockets.push(socket);
      return socket;
    },
    setTimeout: (callback, delay) => {
      timers.push({ callback, delay });
      return timers.length;
    },
    clearTimeout: (id) => {
      cleared.push(id);
    },
  });
  return { client, sockets, outlets, posts, timers, cleared };
}

function poryaaaaServerHarness() {
  const service = new PoryaaaaVoicegroupService({
    scanBanks: () => ["alpha"],
    parseVoicegroup: () => ({ ok: true, slots: [SLOT] }),
    readVoicegroupState: () => null,
    writeVoicegroupState: () => {},
    output: () => {},
    post: () => {},
    websocketHost: "127.0.0.1",
    websocketPort: 0,
  });
  return service;
}

async function waitForStateOutlet(outlets: unknown[][]) {
  const deadline = Date.now() + 1_000;
  while (Date.now() < deadline) {
    const state = outlets.find((outlet) => outlet[0] === "state");
    if (state) return state;
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw new Error("timed out waiting for ccomidi state outlet");
}

test("client forwards only snapshot frames as encoded { slots } state messages", () => {
  const h = harness();
  h.client.start();

  h.sockets[0].emitMessage(JSON.stringify({ type: "ignored", slots: [SLOT] }));
  h.sockets[0].emitMessage("not json");
  h.sockets[0].emitMessage(JSON.stringify({ type: "snapshot", slots: [] }));
  h.sockets[0].emitMessage(JSON.stringify({ type: "snapshot", slots: [SLOT] }));

  const stateMessages = h.outlets.filter((outlet) => outlet[0] === "state");
  assert.equal(stateMessages.length, 1);
  assert.deepEqual(JSON.parse(decodeURIComponent(stateMessages[0][1] as string)), {
    slots: [SLOT],
  });
});

test("client reconnects forever with the configured fixed delay", () => {
  const h = harness();
  h.client.start();

  h.sockets[0].emit("close");

  assert.equal(h.timers.length, 1);
  assert.equal(h.timers[0].delay, 25);
  h.timers[0].callback();

  assert.equal(h.sockets.length, 2);
});

test("client logs when starting each websocket connection attempt", () => {
  const h = harness();
  h.client.start();
  h.sockets[0].emit("close");
  h.timers[0].callback();

  assert.deepEqual(h.posts, [
    "ccomidi voicegroup service: started\n",
    "ccomidi voicegroup websocket: connecting to ws://127.0.0.1:17777/\n",
    "ccomidi voicegroup websocket: connecting to ws://127.0.0.1:17777/\n",
  ]);
});

test("client reports trying and connected states", () => {
  const h = harness();
  h.client.start();
  h.sockets[0].emit("open");

  assert.deepEqual(h.outlets, [
    ["connstatus", "set", "trying"],
    ["connstatus", "set", "connected"],
  ]);
});

test("client reports failed and logs when the outbound connect errors before open", () => {
  const h = harness();
  h.client.start();
  h.sockets[0].emit("error", new Error("ECONNREFUSED"));
  h.sockets[0].emit("close");

  assert.deepEqual(h.outlets, [
    ["connstatus", "set", "trying"],
    ["connstatus", "set", "failed"],
  ]);
  assert.deepEqual(h.posts, [
    "ccomidi voicegroup service: started\n",
    "ccomidi voicegroup websocket: connecting to ws://127.0.0.1:17777/\n",
    "ccomidi voicegroup websocket: failed to connect outside node process: Error: ECONNREFUSED\n",
  ]);
});

test("client does not spam the same connect-failure log across retries", () => {
  const h = harness();
  h.client.start();
  h.sockets[0].emit("error", new Error("ECONNREFUSED"));
  h.sockets[0].emit("close");
  h.timers[0].callback();
  h.sockets[1].emit("error", new Error("ECONNREFUSED"));
  h.sockets[1].emit("close");

  assert.deepEqual(h.posts, [
    "ccomidi voicegroup service: started\n",
    "ccomidi voicegroup websocket: connecting to ws://127.0.0.1:17777/\n",
    "ccomidi voicegroup websocket: failed to connect outside node process: Error: ECONNREFUSED\n",
    "ccomidi voicegroup websocket: connecting to ws://127.0.0.1:17777/\n",
  ]);
});

test("client reports failed and logs when socket construction throws", () => {
  const outlets: unknown[][] = [];
  const posts: string[] = [];
  const timers: Array<{ delay: number; callback: () => void }> = [];
  const client = new CcomidiVoicegroupClient({
    url: "ws://127.0.0.1:17777/",
    reconnectDelayMs: 25,
    outlet: (...args) => outlets.push(args),
    post: (msg) => posts.push(msg),
    webSocketFactory: () => {
      throw new Error("bad socket");
    },
    setTimeout: (callback, delay) => {
      timers.push({ callback, delay });
      return timers.length;
    },
    clearTimeout: () => {},
  });

  client.start();

  assert.deepEqual(outlets, [
    ["connstatus", "set", "trying"],
    ["connstatus", "set", "failed"],
  ]);
  assert.deepEqual(posts, [
    "ccomidi voicegroup service: started\n",
    "ccomidi voicegroup websocket: connecting to ws://127.0.0.1:17777/\n",
    "ccomidi voicegroup websocket: failed before connection left node process: Error: bad socket\n",
  ]);
  assert.equal(timers.length, 1);
});

test("client stop closes the current socket", () => {
  const h = harness();
  h.client.start();

  h.client.stop();

  assert.equal(h.sockets[0].closed, true);
});

test("client stop clears a pending reconnect", () => {
  const h = harness();
  h.client.start();
  h.sockets[0].emit("close");

  h.client.stop();
  h.timers[0].callback();

  assert.deepEqual(h.cleared, [1]);
  assert.equal(h.sockets.length, 1);
});

test("client default native WebSocket receives a poryaaaa server snapshot", async () => {
  assert.equal(typeof globalThis.WebSocket, "function");
  const service = poryaaaaServerHarness();
  const outlets: unknown[][] = [];
  const posts: string[] = [];

  await service.startWebSocket();
  try {
    service.rawroot("/p");
    service.bankselect("alpha");
    const liveClient = new CcomidiVoicegroupClient({
      url: service.websocketUrl(),
      reconnectDelayMs: 25,
      outlet: (...args) => outlets.push(args),
      post: (msg) => posts.push(msg),
    });
    liveClient.start();
    try {
      const state = await waitForStateOutlet(outlets);
      assert.deepEqual(JSON.parse(decodeURIComponent(state[1] as string)), {
        slots: [SLOT],
      });
      assert.deepEqual(outlets.slice(0, 2), [
        ["connstatus", "set", "trying"],
        ["connstatus", "set", "connected"],
      ]);
    } finally {
      liveClient.stop();
    }
  } finally {
    await service.closeWebSocket();
  }
});
