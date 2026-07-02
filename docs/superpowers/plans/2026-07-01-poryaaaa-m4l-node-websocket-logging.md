# PooriA-M4L Node WebSocket Logging Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Route Node-for-Max debug output through the existing voicegroup WebSocket server instead of `maxApi.post`.

**Architecture:** Keep `PoryaaaaVoicegroupService` as the single owner of `127.0.0.1:17777`. Extend the existing WebSocket frame protocol with a live-only `log` frame, let the server broadcast its own logs directly, and let the CCOMI Node client publish log frames through its existing WebSocket connection. V8 scripts and `cleanup.ts` are out of scope.

**Tech Stack:** TypeScript, Node-for-Max, `ws`, Node `--test`, existing `npm run check` and `npm run build:node` package scripts.

---

## Scope

Implement this only in Node-owned runtime files:

- `packages/poryaaaa-m4l/code-src/poryaaaa-node/voicegroup-service.ts`
- `packages/poryaaaa-m4l/code-src/poryaaaa_voicegroup_server.ts`
- `packages/poryaaaa-m4l/code-src/ccomidi_voicegroup_client.ts`
- Matching tests under `packages/poryaaaa-m4l/code-src/test/`

Do not touch:

- `packages/poryaaaa-m4l/code-src/ccomidi_voices.ts`
- `packages/poryaaaa-m4l/code-src/recorder/ccomidi_recorder.ts`
- `packages/poryaaaa-m4l/code-src/cleanup.ts`
- `packages/poryaaaa-m4l/devices/*.amxd`
- Generated `packages/poryaaaa-m4l/javascript/*.js`

The worktree already has unrelated and overlapping uncommitted changes. Before implementation, inspect `git status --short` and `git diff -- packages/poryaaaa-m4l/code-src/poryaaaa-node/voicegroup-service.ts packages/poryaaaa-m4l/code-src/test/poryaaaa_node_state.test.ts`. Preserve those changes and merge with them instead of reverting them.

## File Structure

- `voicegroup-service.ts`: owns the WebSocket server, frame types, direct server log broadcast, and rebroadcast of inbound client log frames.
- `poryaaaa_voicegroup_server.ts`: owns Max handler wiring for the poryaaaa voicegroup Node sidecar. It should no longer call `maxApi.post`; use `service.log(...)` for diagnostics.
- `ccomidi_voicegroup_client.ts`: owns the CCOMI Node WebSocket client. It should no longer accept a `post` callback or call `maxApi.post`; it should try to send `log` frames through its current socket.
- `poryaaaa_node_websocket_state.test.ts`: verifies server-side `log` frame broadcast and inbound rebroadcast.
- `poryaaaa_node_state.test.ts`: updates service harnesses after removing the injected `post` dependency.
- `ccomidi_voicegroup_client.test.ts`: verifies client-side log frames are sent and existing voicegroup frame handling still ignores non-state frames.

## Message Contract

Keep the existing voicegroup frames:

```ts
export type CcomidiVoicegroupFrame =
  | { type: "snapshot"; slots: Array<VoiceSlot | null> }
  | { type: "unavailable" };
```

Add a log frame:

```ts
export type PoryaaaaLogLevel = "debug" | "info" | "warn" | "error";

export type PoryaaaaLogFrame = {
  type: "log";
  source: string;
  level: PoryaaaaLogLevel;
  message: string;
};

export type PoryaaaaWebSocketFrame = CcomidiVoicegroupFrame | PoryaaaaLogFrame;
```

Do not retain logs for late WebSocket clients. New clients still receive only the latest voicegroup state (`snapshot` or `unavailable`) on connect. A local diagnostics listener sees log frames only while connected.

---

### Task 1: Add Server Log Frames

**Files:**
- Modify: `packages/poryaaaa-m4l/code-src/poryaaaa-node/voicegroup-service.ts`
- Modify: `packages/poryaaaa-m4l/code-src/test/poryaaaa_node_websocket_state.test.ts`

- [ ] **Step 1: Write failing tests for direct log broadcast and inbound log rebroadcast**

Add `PoryaaaaLogFrame` to the test import in `poryaaaa_node_websocket_state.test.ts`:

```ts
import {
  PoryaaaaVoicegroupService,
  type CcomidiSnapshot,
  type PoryaaaaLogFrame,
  type PoryaaaaVoicegroupOutput,
} from "../poryaaaa-node/voicegroup-service";
```

Append these tests near the other WebSocket behavior tests:

```ts
test("server broadcasts log frames without replacing latest voicegroup snapshot", async () => {
  const { service } = serviceHarness();
  await service.startWebSocket();
  const client = await openClientWithInitial(service.websocketUrl());
  try {
    await client.initial;
    const logMessage = nextJson(client.ws);

    service.log("voicegroups", "info", "voicegroups: ready");

    assert.deepEqual(await logMessage, {
      type: "log",
      source: "voicegroups",
      level: "info",
      message: "voicegroups: ready",
    });
    assert.equal(service.latestSnapshot(), null);
  } finally {
    await closeClient(client.ws);
    await service.closeWebSocket();
  }
});

test("server rebroadcasts valid inbound log frames from local websocket clients", async () => {
  const { service } = serviceHarness();
  await service.startWebSocket();
  const publisher = await openClientWithInitial(service.websocketUrl());
  const observer = await openClientWithInitial(service.websocketUrl());
  try {
    await Promise.all([publisher.initial, observer.initial]);
    const observed = nextJson(observer.ws);
    const frame: PoryaaaaLogFrame = {
      type: "log",
      source: "ccomidi_voicegroup_client",
      level: "debug",
      message: "ccomidi voicegroup websocket: connected",
    };

    publisher.ws.send(JSON.stringify(frame));

    assert.deepEqual(await observed, frame);
  } finally {
    await Promise.all([closeClient(publisher.ws), closeClient(observer.ws)]);
    await service.closeWebSocket();
  }
});
```

- [ ] **Step 2: Run the focused server WebSocket test and verify it fails**

Run from `packages/poryaaaa-m4l`:

```bash
node --test-timeout=5000 --import tsx --test code-src/test/poryaaaa_node_websocket_state.test.ts
```

Expected: FAIL because `PoryaaaaLogFrame` and `service.log(...)` do not exist yet.

- [ ] **Step 3: Extend the server frame types and add `log(...)`**

In `voicegroup-service.ts`, replace the frame type section with:

```ts
export type CcomidiVoicegroupFrame =
  | { type: "snapshot"; slots: Array<VoiceSlot | null> }
  | { type: "unavailable" };

export type PoryaaaaLogLevel = "debug" | "info" | "warn" | "error";

export type PoryaaaaLogFrame = {
  type: "log";
  source: string;
  level: PoryaaaaLogLevel;
  message: string;
};

export type PoryaaaaWebSocketFrame = CcomidiVoicegroupFrame | PoryaaaaLogFrame;
```

Change `sendFrame` and `broadcastFrame` to use the broader frame type:

```ts
private sendFrame(ws: WebSocket, frame: PoryaaaaWebSocketFrame): void {
  ws.send(JSON.stringify(frame));
}

private broadcastFrame(frame: PoryaaaaWebSocketFrame): void {
  if (!this.wss) return;
  const payload = JSON.stringify(frame);
  for (const client of this.wss.clients) {
    if (client.readyState === WebSocket.OPEN) {
      client.send(payload);
    }
  }
}
```

Add a small validator and public logger inside `PoryaaaaVoicegroupService`:

```ts
private isLogFrame(frame: unknown): frame is PoryaaaaLogFrame {
  if (!frame || typeof frame !== "object" || Array.isArray(frame)) return false;
  const candidate = frame as Partial<PoryaaaaLogFrame>;
  return (
    candidate.type === "log" &&
    typeof candidate.source === "string" &&
    typeof candidate.message === "string" &&
    (candidate.level === "debug" ||
      candidate.level === "info" ||
      candidate.level === "warn" ||
      candidate.level === "error")
  );
}

log(source: string, level: PoryaaaaLogLevel, message: string): void {
  this.broadcastFrame({
    type: "log",
    source,
    level,
    message,
  });
}
```

Update the inbound client message handler:

```ts
wss.on("connection", (ws) => {
  ws.on("message", (data) => {
    let parsed: unknown;
    try {
      parsed = JSON.parse(String(data));
    } catch (_) {
      return;
    }
    if (this.isLogFrame(parsed)) this.broadcastFrame(parsed);
  });
  if (this.latest) {
    this.sendFrame(ws, { type: "snapshot", slots: this.latest.slots });
  } else {
    this.sendFrame(ws, { type: "unavailable" });
  }
});
```

- [ ] **Step 4: Run the focused server WebSocket test and verify it passes**

Run from `packages/poryaaaa-m4l`:

```bash
node --test-timeout=5000 --import tsx --test code-src/test/poryaaaa_node_websocket_state.test.ts
```

Expected: PASS.

- [ ] **Step 5: Commit Task 1**

```bash
git add code-src/poryaaaa-node/voicegroup-service.ts code-src/test/poryaaaa_node_websocket_state.test.ts
git commit -m "feat(m4l): add websocket log frames"
```

---

### Task 2: Replace Voicegroup Server `post` Dependency

**Files:**
- Modify: `packages/poryaaaa-m4l/code-src/poryaaaa-node/voicegroup-service.ts`
- Modify: `packages/poryaaaa-m4l/code-src/poryaaaa_voicegroup_server.ts`
- Modify: `packages/poryaaaa-m4l/code-src/test/poryaaaa_node_state.test.ts`
- Modify: `packages/poryaaaa-m4l/code-src/test/poryaaaa_node_websocket_state.test.ts`

- [ ] **Step 1: Write failing coverage for service diagnostics over WebSocket**

In `poryaaaa_node_websocket_state.test.ts`, add a parse-failure diagnostic assertion:

```ts
test("parse failure diagnostics are emitted as websocket log frames", async () => {
  const service = new PoryaaaaVoicegroupService({
    scanBanks: () => ["bad"],
    parseVoicegroup: () => ({ ok: false, diagnostics: ["bad voice macro"] }),
    readVoicegroupState: () => null,
    writeVoicegroupState: () => {},
    output: () => {},
    websocketHost: "127.0.0.1",
    websocketPort: 0,
  });
  await service.startWebSocket();
  const client = await openClientWithInitial(service.websocketUrl());
  try {
    await client.initial;
    service.rawroot("/p");
    const logFrame = nextJson(client.ws);
    const unavailableFrame = nextJson(client.ws);

    service.bankselect("bad");

    assert.deepEqual(await logFrame, {
      type: "log",
      source: "voicegroups",
      level: "warn",
      message: "voicegroups: bad voice macro",
    });
    assert.deepEqual(await unavailableFrame, {
      type: "unavailable",
    });
  } finally {
    await closeClient(client.ws);
    await service.closeWebSocket();
  }
});
```

This test intentionally omits `post` from `PoryaaaaVoicegroupService` deps.

- [ ] **Step 2: Run tests and verify the dependency removal fails**

Run from `packages/poryaaaa-m4l`:

```bash
node --test-timeout=5000 --import tsx --test code-src/test/poryaaaa_node_websocket_state.test.ts
```

Expected: FAIL because the service still calls `this.deps.post(...)`; without that dependency, the parse-failure path throws instead of broadcasting a log frame.

- [ ] **Step 3: Remove `post` from the service dependency contract**

In `voicegroup-service.ts`, remove this field from `PoryaaaaVoicegroupServiceDeps`:

```ts
post: (msg: string) => void;
```

Replace every `this.deps.post(...)` call with `this.log(...)`. Use these exact mappings:

```ts
this.log("voicegroups", "error", `voicegroups websocket: ${String(err)}`);
this.log("voicegroups", "warn", `voicegroups: no voicegroup banks found under ${root}`);
this.log("voicegroups", "warn", `voicegroups: ${diagnostic}`);
this.log(
  "voicegroups",
  "info",
  `voicegroups: bank selected "${this.currentBank}"; websocket data ${JSON.stringify(frame)}`,
);
this.log("voicegroups", "warn", `voicegroups: saved bank "${state.bank}" not found in current root`);
this.log("voicegroups", "info", "voicegroups: no saved state");
this.log("voicegroups", "info", "voicegroups: reload ignored; no voicegroup is selected");
this.log(
  "voicegroups",
  "debug",
  `dumpstate: currentRoot="${this.currentRoot}" currentBank="${this.currentBank}" latestSlots=${this.latest?.slots.length ?? 0}`,
);
```

Preserve the current `unavailable` broadcast after parse failures. The log frame should be broadcast before the `unavailable` frame because the test expects diagnostics first.

- [ ] **Step 4: Update service test harnesses**

In `poryaaaa_node_websocket_state.test.ts` and `poryaaaa_node_state.test.ts`, remove `post: () => {}` and `post: (msg) => posts.push(msg)` from `new PoryaaaaVoicegroupService(...)` calls.

In `poryaaaa_node_state.test.ts`, remove tests that assert `h.posts` contents. If preserving their behavior is still valuable, move that assertion into `poryaaaa_node_websocket_state.test.ts` where WebSocket frames are observable. For the current uncommitted `bankselect logs the selected bank and websocket data` test, replace it with this WebSocket test:

```ts
test("bankselect emits selected bank diagnostics as websocket log frames", async () => {
  const { service } = serviceHarness();
  await service.startWebSocket();
  const client = await openClientWithInitial(service.websocketUrl());
  try {
    await client.initial;
    service.rawroot("/p");
    const logFrame = nextJson(client.ws);

    service.bankselect("alpha");

    assert.deepEqual(await logFrame, {
      type: "log",
      source: "voicegroups",
      level: "info",
      message:
        'voicegroups: bank selected "alpha"; websocket data {"type":"snapshot","slots":[{"name":"Lead","typeCode":0}]}',
    });
  } finally {
    await closeClient(client.ws);
    await service.closeWebSocket();
  }
});
```

- [ ] **Step 5: Update `poryaaaa_voicegroup_server.ts`**

Remove `post` from the local `MaxApi` type:

```ts
type MaxApi = {
  addHandler: (name: string, handler: (...args: unknown[]) => void) => void;
  outlet: (...args: unknown[]) => void;
};
```

Remove `post` from the service constructor:

```ts
const service = new PoryaaaaVoicegroupService({
  scanBanks: scanVoicegroupBanks,
  parseVoicegroup,
  readVoicegroupState: () => store.readVoicegroupState(),
  writeVoicegroupState: (state) => store.writeVoicegroupState(state),
  output: (out) => maxApi.outlet(out.tag, ...out.args),
});
```

Replace startup logging with WebSocket logging:

```ts
service
  .startWebSocket()
  .then(() => {
    service.log("voicegroups", "info", "voicegroups: ready");
  })
  .catch((err: unknown) => {
    service.log(
      "voicegroups",
      "error",
      `voicegroups websocket: failed to bind 127.0.0.1:17777: ${String(err)}`,
    );
  });
```

Keep `maxApi.outlet("ready");` because it is patcher control flow, not debug logging. Remove `maxApi.post("voicegroups: ready\n");`.

- [ ] **Step 6: Verify no scoped server post calls remain**

Run from repo root:

```bash
rg -n "maxApi\\.post|post:" packages/poryaaaa-m4l/code-src/poryaaaa_voicegroup_server.ts packages/poryaaaa-m4l/code-src/poryaaaa-node/voicegroup-service.ts
```

Expected: no matches.

- [ ] **Step 7: Run focused tests**

Run from `packages/poryaaaa-m4l`:

```bash
node --test-timeout=5000 --import tsx --test code-src/test/poryaaaa_node_websocket_state.test.ts code-src/test/poryaaaa_node_state.test.ts
```

Expected: PASS.

- [ ] **Step 8: Commit Task 2**

```bash
git add code-src/poryaaaa-node/voicegroup-service.ts code-src/poryaaaa_voicegroup_server.ts code-src/test/poryaaaa_node_websocket_state.test.ts code-src/test/poryaaaa_node_state.test.ts
git commit -m "refactor(m4l): route voicegroup diagnostics through websocket"
```

---

### Task 3: Publish CCOMI Node Client Logs Through Its Socket

**Files:**
- Modify: `packages/poryaaaa-m4l/code-src/ccomidi_voicegroup_client.ts`
- Modify: `packages/poryaaaa-m4l/code-src/test/ccomidi_voicegroup_client.test.ts`

- [ ] **Step 1: Update fake socket test support**

In `ccomidi_voicegroup_client.test.ts`, update `FakeSocket`:

```ts
class FakeSocket implements ClientWebSocket {
  closed = false;
  sent: string[] = [];
  private listeners = new Map<string, Array<(event: unknown) => void>>();

  addEventListener(event: string, listener: (event: unknown) => void) {
    const listeners = this.listeners.get(event) ?? [];
    listeners.push(listener);
    this.listeners.set(event, listeners);
  }

  send(data: string) {
    this.sent.push(data);
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
```

- [ ] **Step 2: Replace post-based tests with WebSocket log frame tests**

Remove or rewrite tests that assert `h.posts`. Add these tests:

```ts
function sentJson(socket: FakeSocket) {
  return socket.sent.map((message) => JSON.parse(message));
}

test("client sends startup and connection diagnostics as websocket log frames", () => {
  const h = harness();
  h.client.start();
  h.sockets[0].emit("open");

  assert.deepEqual(sentJson(h.sockets[0]), [
    {
      type: "log",
      source: "ccomidi_voicegroup_client",
      level: "info",
      message: "ccomidi voicegroup service: started",
    },
    {
      type: "log",
      source: "ccomidi_voicegroup_client",
      level: "debug",
      message: "ccomidi voicegroup websocket: connected to ws://127.0.0.1:17777/",
    },
  ]);
});

test("client sends invalid JSON diagnostics as websocket log frames", () => {
  const h = harness();
  h.client.start();
  h.sockets[0].emit("open");
  h.sockets[0].sent.length = 0;

  h.sockets[0].emitMessage("not json");

  assert.deepEqual(sentJson(h.sockets[0]), [
    {
      type: "log",
      source: "ccomidi_voicegroup_client",
      level: "warn",
      message: "ccomidi voicegroup websocket: received invalid JSON: not json",
    },
  ]);
});

test("client tries to send connection failure diagnostics through websocket frames", () => {
  const h = harness();
  h.client.start();
  h.sockets[0].sent.length = 0;

  h.sockets[0].emit("error", new Error("ECONNREFUSED"));

  assert.deepEqual(sentJson(h.sockets[0]), [
    {
      type: "log",
      source: "ccomidi_voicegroup_client",
      level: "error",
      message:
        "ccomidi voicegroup websocket: failed to connect outside node process: Error: ECONNREFUSED",
    },
  ]);
});
```

Keep the existing state-forwarding tests, but update their expected post assertions to no longer require `posts`.

- [ ] **Step 3: Run the focused CCOMI client test and verify it fails**

Run from `packages/poryaaaa-m4l`:

```bash
node --test-timeout=5000 --import tsx --test code-src/test/ccomidi_voicegroup_client.test.ts
```

Expected: FAIL because `ClientWebSocket` has no `send(...)` method and the client still calls `opts.post`.

- [ ] **Step 4: Update the client interface and options**

In `ccomidi_voicegroup_client.ts`, change `ClientWebSocket`:

```ts
export interface ClientWebSocket {
  addEventListener: (
    event: string,
    listener: (event: unknown) => void,
  ) => unknown;
  send: (data: string) => void;
  close: () => void;
}
```

Remove `post` from `CcomidiVoicegroupClientOptions`:

```ts
export interface CcomidiVoicegroupClientOptions {
  url?: string;
  reconnectDelayMs?: number;
  outlet: (...args: unknown[]) => void;
  webSocketFactory?: (url: string) => ClientWebSocket;
  setTimeout?: (callback: () => void, delay: number) => unknown;
  clearTimeout?: (id: unknown) => void;
}
```

Add a source constant near the defaults:

```ts
const LOG_SOURCE = "ccomidi_voicegroup_client";
```

- [ ] **Step 5: Add the client log sender**

Near the source constant, add a local frame type:

```ts
type ClientLogFrame = {
  type: "log";
  source: typeof LOG_SOURCE;
  level: "debug" | "info" | "warn" | "error";
  message: string;
};
```

Inside `CcomidiVoicegroupClient`, add a pending queue field:

```ts
private pendingLogs: ClientLogFrame[] = [];
```

Add these private methods:

```ts
private sendLogFrame(frame: ClientLogFrame): boolean {
  const socket = this.socket;
  if (!socket) return false;
  try {
    socket.send(JSON.stringify(frame));
    return true;
  } catch (_) {
    return false;
  }
}

private flushPendingLogs(): void {
  const pending = this.pendingLogs;
  this.pendingLogs = [];
  for (const frame of pending) {
    if (!this.sendLogFrame(frame)) this.pendingLogs.push(frame);
  }
}

private log(level: "debug" | "info" | "warn" | "error", message: string): void {
  const frame: ClientLogFrame = {
    type: "log",
    source: LOG_SOURCE,
    level,
    message,
  };
  if (!this.sendLogFrame(frame)) this.pendingLogs.push(frame);
}
```

Replace `this.opts.post(...)` calls:

```ts
this.log("warn", `ccomidi voicegroup websocket: received invalid JSON: ${String(raw)}`);
this.log("error", `ccomidi voicegroup websocket: failed before connection left node process: ${String(err)}`);
this.log("error", `ccomidi voicegroup websocket: failed to connect outside node process: ${String(err)}`);
this.log("info", "ccomidi voicegroup service: started");
```

In the `open` event handler, add:

```ts
this.flushPendingLogs();
this.log("debug", `ccomidi voicegroup websocket: connected to ${this.url}`);
```

Remove the pre-connect `connecting to ${this.url}` post. It cannot be reliably delivered through the WebSocket before the socket exists or opens, and the new `connected` log covers successful connection attempts.

In `start()`, call `this.log("info", "ccomidi voicegroup service: started");` after `this.connect();`. If native `send(...)` throws before the socket is open, the pending queue flushes the message in the `open` handler.

- [ ] **Step 6: Update top-level Max wiring**

Remove `post` from the exported `MaxApi` interface:

```ts
export interface MaxApi {
  outlet: (...args: unknown[]) => void;
  addHandler: (name: string, handler: (...args: unknown[]) => void) => void;
}
```

Remove `post` from the client construction:

```ts
client = new CcomidiVoicegroupClient({
  outlet: (...args) => maxApi.outlet(...args),
});
```

Replace the already-started diagnostic:

```ts
client.logAlreadyStarted();
```

Add this public method to the class:

```ts
logAlreadyStarted() {
  this.log("info", "ccomidi voicegroup service: already started");
}
```

Remove `maxApi.post("ccomidi voicegroup service: ready\n");`. Keep `maxApi.outlet("ready");`.

- [ ] **Step 7: Verify no scoped CCOMI post calls remain**

Run from repo root:

```bash
rg -n "maxApi\\.post|opts\\.post|post:" packages/poryaaaa-m4l/code-src/ccomidi_voicegroup_client.ts
```

Expected: no matches.

- [ ] **Step 8: Run focused CCOMI client tests**

Run from `packages/poryaaaa-m4l`:

```bash
node --test-timeout=5000 --import tsx --test code-src/test/ccomidi_voicegroup_client.test.ts
```

Expected: PASS.

- [ ] **Step 9: Commit Task 3**

```bash
git add code-src/ccomidi_voicegroup_client.ts code-src/test/ccomidi_voicegroup_client.test.ts
git commit -m "refactor(m4l): publish ccomidi node logs over websocket"
```

---

### Task 4: Final Verification

**Files:**
- Verify only; no file edits expected.

- [ ] **Step 1: Check scoped `maxApi.post` usage**

Run from repo root:

```bash
rg -n "maxApi\\.post|opts\\.post|post:" packages/poryaaaa-m4l/code-src/poryaaaa_voicegroup_server.ts packages/poryaaaa-m4l/code-src/poryaaaa-node/voicegroup-service.ts packages/poryaaaa-m4l/code-src/ccomidi_voicegroup_client.ts
```

Expected: no matches.

Run this to confirm excluded files still contain their existing logging where applicable:

```bash
rg -n "maxApi\\.post|post:" packages/poryaaaa-m4l/code-src/cleanup.ts packages/poryaaaa-m4l/code-src/ccomidi_voices.ts packages/poryaaaa-m4l/code-src/recorder/ccomidi_recorder.ts
```

Expected: matches are allowed because `cleanup.ts` and V8 scripts are out of scope.

- [ ] **Step 2: Run focused Node transport tests**

Run from `packages/poryaaaa-m4l`:

```bash
node --test-timeout=5000 --import tsx --test code-src/test/poryaaaa_node_websocket_state.test.ts code-src/test/poryaaaa_node_state.test.ts code-src/test/ccomidi_voicegroup_client.test.ts
```

Expected: PASS.

- [ ] **Step 3: Run package typecheck**

Run from `packages/poryaaaa-m4l`:

```bash
npm run check
```

Expected: PASS.

- [ ] **Step 4: Build Node bundles**

Run from `packages/poryaaaa-m4l`:

```bash
npm run build:node
```

Expected: PASS. Generated `javascript/*.js` files may change as build output; inspect and include them only if this repo normally commits built Node bundles for the current task.

- [ ] **Step 5: Inspect final diff**

Run from repo root:

```bash
git diff -- packages/poryaaaa-m4l/code-src/poryaaaa-node/voicegroup-service.ts packages/poryaaaa-m4l/code-src/poryaaaa_voicegroup_server.ts packages/poryaaaa-m4l/code-src/ccomidi_voicegroup_client.ts packages/poryaaaa-m4l/code-src/test/poryaaaa_node_websocket_state.test.ts packages/poryaaaa-m4l/code-src/test/poryaaaa_node_state.test.ts packages/poryaaaa-m4l/code-src/test/ccomidi_voicegroup_client.test.ts
```

Expected:

- Only Node transport files and matching tests changed.
- No V8 files changed.
- No `cleanup.ts` changes.
- No new standalone logging module.
- Existing voicegroup `snapshot` and `unavailable` frame behavior preserved.

- [ ] **Step 6: Commit final verification fixes if needed**

If final verification required small fixes, commit them:

```bash
git add code-src/poryaaaa-node/voicegroup-service.ts code-src/poryaaaa_voicegroup_server.ts code-src/ccomidi_voicegroup_client.ts code-src/test/poryaaaa_node_websocket_state.test.ts code-src/test/poryaaaa_node_state.test.ts code-src/test/ccomidi_voicegroup_client.test.ts
git commit -m "test(m4l): verify websocket diagnostics contract"
```

Skip this commit if Task 4 produced no code changes.

## Self-Review

- Spec coverage: The plan keeps one WebSocket server, adds `log` frames, routes Node-for-Max diagnostics through that server, excludes V8 scripts, and excludes `cleanup.ts`.
- Placeholder scan: No implementation step contains deferred work or undefined function names. `logAlreadyStarted()` is introduced before use in the same task.
- Type consistency: `PoryaaaaLogLevel`, `PoryaaaaLogFrame`, and `PoryaaaaWebSocketFrame` are defined before use. `ClientWebSocket.send(...)` and `ClientLogFrame` are added before fake and runtime clients rely on them.
- Scope check: The plan touches one subsystem, Node transport, plus its tests. It does not require AMXD edits or C++ external changes.
