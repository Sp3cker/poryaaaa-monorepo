import { validateVoiceSlots } from "./voice-slot-contract";

export interface MaxApi {
  outlet: (...args: unknown[]) => void;
  post: (msg: string) => void;
  addHandler: (name: string, handler: (...args: unknown[]) => void) => void;
}
// DO NOT TOUCH THIS, CODEX
const maxApi = require("max-api") as MaxApi;

let client: CcomidiVoicegroupClient | undefined;

export interface ClientWebSocket {
  addEventListener: (
    event: string,
    listener: (event: unknown) => void,
  ) => unknown;
  close: () => void;
}

export interface CcomidiVoicegroupClientOptions {
  url?: string;
  reconnectDelayMs?: number;
  outlet: (...args: unknown[]) => void;
  post: (msg: string) => void;
  webSocketFactory?: (url: string) => ClientWebSocket;
  setTimeout?: (callback: () => void, delay: number) => unknown;
  clearTimeout?: (id: unknown) => void;
}

const DEFAULT_URL = "ws://127.0.0.1:17777/";
const DEFAULT_RECONNECT_DELAY_MS = 500;

export class CcomidiVoicegroupClient {
  private readonly url: string;
  private readonly reconnectDelayMs: number;
  private readonly makeSocket: (url: string) => ClientWebSocket;
  private readonly setTimer: (callback: () => void, delay: number) => unknown;
  private readonly clearTimer: (id: unknown) => void;
  private socket: ClientWebSocket | null = null;
  private reconnectTimer: unknown = null;
  private loggedConnectFailure = false;
  private stopped = true;

  constructor(private readonly opts: CcomidiVoicegroupClientOptions) {
    this.url = opts.url ?? DEFAULT_URL;
    this.reconnectDelayMs = opts.reconnectDelayMs ?? DEFAULT_RECONNECT_DELAY_MS;
    this.makeSocket =
      opts.webSocketFactory ??
      ((socketUrl: string) => {
        const WebSocketCtor = globalThis.WebSocket as
          | (new (url: string) => ClientWebSocket)
          | undefined;
        if (typeof WebSocketCtor !== "function") {
          throw new Error("native WebSocket is unavailable");
        }
        return new WebSocketCtor(socketUrl);
      });
    this.setTimer =
      opts.setTimeout ??
      ((callback: () => void, delay: number) =>
        globalThis.setTimeout(callback, delay));
    this.clearTimer =
      opts.clearTimeout ??
      ((id: unknown) =>
        globalThis.clearTimeout(id as ReturnType<typeof setTimeout>));
  }

  private scheduleReconnect(): void {
    if (this.stopped || this.reconnectTimer !== null) return;
    this.reconnectTimer = this.setTimer(() => {
      this.reconnectTimer = null;
      this.connect();
    }, this.reconnectDelayMs);
  }

  private status(status: "trying" | "connected" | "failed"): void {
    this.opts.outlet("connstatus", "set", status);
  }

  private handleMessage(raw: unknown): void {
    let parsed: unknown;
    try {
      parsed = JSON.parse(String(raw));
    } catch (_) {
      this.opts.post(
        `ccomidi voicegroup websocket: received invalid JSON: ${String(raw)}\n`,
      );
      return;
    }
    if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) return;
    const frame = parsed as { type?: unknown; slots?: unknown };
    if (frame.type !== "snapshot") return;
    let slots;
    try {
      slots = validateVoiceSlots(frame.slots);
    } catch (_) {
      return;
    }
    const encoded = encodeURIComponent(JSON.stringify({ slots }));
    this.opts.outlet("state", encoded);
  }

  private connect(): void {
    if (this.stopped) return;
    this.opts.post(`ccomidi voicegroup websocket: connecting to ${this.url}\n`);
    this.status("trying");
    let opened = false;
    let failed = false;
    const fail = () => {
      if (failed) return;
      failed = true;
      this.status("failed");
    };
    try {
      this.socket = this.makeSocket(this.url);
    } catch (err) {
      fail();
      if (!this.loggedConnectFailure) {
        this.loggedConnectFailure = true;
        this.opts.post(
          `ccomidi voicegroup websocket: failed before connection left node process: ${String(err)}\n`,
        );
      }
      this.scheduleReconnect();
      return;
    }
    this.socket.addEventListener("open", () => {
      opened = true;
      this.loggedConnectFailure = false;
      this.status("connected");
    });
    this.socket.addEventListener("message", (event) => {
      const raw =
        event && typeof event === "object" && "data" in event
          ? (event as { data: unknown }).data
          : event;
      this.handleMessage(raw);
    });
    this.socket.addEventListener("close", () => {
      this.socket = null;
      if (!this.stopped) fail();
      this.scheduleReconnect();
    });
    this.socket.addEventListener("error", (err) => {
      if (!opened) {
        fail();
        if (!this.loggedConnectFailure) {
          this.loggedConnectFailure = true;
          this.opts.post(
            `ccomidi voicegroup websocket: failed to connect outside node process: ${String(err)}\n`,
          );
        }
      }
    });
  }

  start() {
    if (!this.stopped) return;
    this.stopped = false;
    this.opts.post("ccomidi voicegroup service: started\n");
    this.connect();
  }

  stop() {
    this.stopped = true;
    if (this.reconnectTimer !== null) {
      this.clearTimer(this.reconnectTimer);
      this.reconnectTimer = null;
    }
    if (this.socket) {
      const current = this.socket;
      this.socket = null;
      current.close();
    }
  }
}

maxApi.addHandler("start", () => {
  if (!client) {
    client = new CcomidiVoicegroupClient({
      outlet: (...args) => maxApi.outlet(...args),
      post: (msg) => maxApi.post(msg),
    });
    client.start();
    return
  }
  maxApi.post("ccomidi voicegroup service: already started\n");
});
maxApi.addHandler("stop", () => {
  if (client) client.stop();
});
maxApi.addHandler("bang", () => {
  // force re-get state, useful for debugging
  if (client) {
    // client.connect();
  }
});
maxApi.outlet("ready");
maxApi.post("ccomidi voicegroup service: ready\n");
