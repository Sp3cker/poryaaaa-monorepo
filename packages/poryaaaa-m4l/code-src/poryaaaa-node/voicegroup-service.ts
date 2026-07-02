import type { AddressInfo } from "node:net";
import WebSocket, { WebSocketServer } from "ws";

import { normalizeRoot } from "../scanner";
import type { VoiceSlot } from "../voice-slot-contract";
import { parseVoicegroup, VoicegroupParseResult } from "./voicegroup-parser";

export interface CcomidiSnapshot {
  slots: Array<VoiceSlot | null>;
}

export type CcomidiVoicegroupFrame =
  | { type: "snapshot"; slots: Array<VoiceSlot | null> }
  | { type: "unavailable" };

export type PoryaaaaVoicegroupOutput =
  | { tag: "bank"; args: string[] }
  | { tag: "path"; args: string[] }
  | { tag: "diag", args: string[] }
  | { tag: "voicegroup"; args: [string, string] };

export interface PoryaaaaVoicegroupServiceDeps {
  scanBanks: (root: string) => string[];
  parseVoicegroup: (root: string, bank: string) => VoicegroupParseResult;
  /* Sends out Max device outlet. Prepend with string so [route] do its thing */
  output: (out: PoryaaaaVoicegroupOutput) => void;
  post: (frame: CcomidiVoicegroupFrame) => void;
  log: (msg: string) => void;
  websocketHost?: string;
  websocketPort?: number;
}

export class PoryaaaaVoicegroupService {
  private currentRoot = "";
  private currentBank = "";
  private latest: CcomidiSnapshot | null = null;
  private readonly websocketHost: string;
  private readonly websocketPort: number;
  private wss: WebSocketServer | null = null;
  private websocketListening = false;

  constructor(private readonly deps: PoryaaaaVoicegroupServiceDeps) {
    this.websocketHost = deps.websocketHost ?? "127.0.0.1";
    this.websocketPort = deps.websocketPort ?? 17777;
  }

  private attachWebSocketHandlers(wss: WebSocketServer): void {
    wss.on("connection", (ws) => {
      ws.on("message", () => {
        // Inbound client messages are reserved for future two-way protocol work.
      });
      if (this.latest) {
        this.sendFrame(ws, { type: "snapshot", slots: this.latest.slots });
        this.deps.output({ tag: "diag", args: ["Sent Voices"] })
      } else {
        // New clients must learn that startup completed with no valid
        // voicegroup; silence leaves ccomidi stuck on "waiting for poryaaaa".
        this.sendFrame(ws, { type: "unavailable" });
      }
    });

    wss.on("error", (err) => {
      this.deps.log(`websocket: ${String(err)}\n`);
    });

    wss.on("close", () => {
      if (this.wss === wss) {
        this.websocketListening = false;
        this.wss = null;
      }
    });
  }

  startWebSocket(): Promise<void> {
    if (this.wss) return Promise.resolve();
    return new Promise((resolve, reject) => {
      const wss = new WebSocketServer({
        host: this.websocketHost,
        port: this.websocketPort,
      });
      this.wss = wss;
      this.attachWebSocketHandlers(wss);

      const onError = (err: Error) => {
        wss.off("listening", onListening);
        if (this.wss === wss) this.wss = null;
        reject(err);
      };
      const onListening = () => {
        wss.off("error", onError);
        this.websocketListening = true;
        resolve();
      };
      wss.once("error", onError);
      wss.once("listening", onListening);
    });
  }

  closeWebSocket(): Promise<void> {
    const wss = this.wss;
    if (!wss) return Promise.resolve();
    return new Promise((resolve, reject) => {
      for (const client of wss.clients) {
        client.terminate();
      }
      wss.close((wssErr) => {
        if (this.wss === wss) {
          this.websocketListening = false;
          this.wss = null;
        }
        if (wssErr) {
          reject(wssErr);
          return;
        }
        resolve();
      });
    });
  }

  isWebSocketListening(): boolean {
    return this.websocketListening;
  }

  websocketUrl(): string {
    const address = this.wss?.address() as AddressInfo | string | null | undefined;
    const actualPort = typeof address === "object" && address ? address.port : this.websocketPort;
    return `ws://${this.websocketHost}:${actualPort}/`;
  }

  // Lets the composition root wire deps.post to this service's owned WebSocket transport.
  post(frame: CcomidiVoicegroupFrame): void {
    this.broadcastFrame(frame);
  }

  private emit(tag: PoryaaaaVoicegroupOutput["tag"], ...args: string[]): void {
    if (tag === "voicegroup") {
      this.deps.output({ tag, args: args as [string, string] });
      return;
    }
    this.deps.output({ tag, args });
  }

  private emitNoProject(): void {
    this.currentRoot = "";
    this.currentBank = "";
    this.clearLatestSnapshot();
    this.emit("path", "set", "(no project)");
    this.emit("bank", "clear");
    this.emit("bank", "append", "(no project loaded)");
  }

  private sendFrame(ws: WebSocket, frame: CcomidiVoicegroupFrame): void {
    ws.send(JSON.stringify(frame));
  }

  private broadcastFrame(frame: CcomidiVoicegroupFrame): void {
    if (!this.wss) return;
    const payload = JSON.stringify(frame);
    for (const client of this.wss.clients) {
      if (client.readyState === WebSocket.OPEN) {
        client.send(payload);
      }
    }
  }

  private updateLatestSnapshot(snapshot: CcomidiSnapshot): void {
    const frame: CcomidiVoicegroupFrame = { type: "snapshot", slots: snapshot.slots };
    this.deps.post(frame);
  }

  private clearLatestSnapshot(): void {
    this.latest = null;
  }


  private emitBanks(root: string): string[] {
    this.currentRoot = root;
    this.currentBank = "";
    if (root.startsWith("/") === false) { // Needs 2 b root dir
      this.emitNoProject();
      return [];
    }
    this.clearLatestSnapshot();

    const banks = this.deps.scanBanks(root);
    this.emit("path", "set", root);
    this.emit("bank", "clear");
    if (banks.length === 0) {
      this.emit("bank", "append", "(no .inc files in sound/voicegroups)");
      this.deps.log(`no voicegroup banks found under ${root}\n`);
    } else {
      for (const bank of banks) this.emit("bank", "append", bank);
    }
    return banks;
  }

  private applyValidBank(bank: string, forceReload = false): boolean {
    if (this.currentRoot.startsWith("/") === false) return false;
    if (!bank || bank.startsWith("(")) return false;
    if (forceReload === false && bank === this.currentBank) return false;

    const parsed = parseVoicegroup(this.currentRoot, bank);
    if (!parsed.ok) {
      for (const diagnostic of parsed.diagnostics) {
        this.deps.output({ tag: "diag", args: [`voicegroups: ${diagnostic}\n`] });
      }
      // With no previous snapshot to keep showing, tell clients the bank is
      // unavailable instead of leaving their last connection state pending.
      if (!this.latest) this.deps.post({ type: "unavailable" });
      return false;
    }

    this.currentBank = bank;
    this.emit("voicegroup", this.currentRoot, this.currentBank);
    const snapshot = { slots: parsed.slots };
    this.latest = snapshot;

    this.updateLatestSnapshot(snapshot);
    return true;
  }

  rawroot(rootPath: string): void {
    this.emitBanks(normalizeRoot(rootPath));
  }

  bankselect(bankName: string): void {
    const ok = this.applyValidBank(String(bankName ?? "").trim());
    if (!ok) {
      this.deps.output({ tag: "diag", args: ["Bank not applied"] })
    }
  }

  reload(): void {
    if (!this.currentRoot || !this.currentBank) {
      this.deps.log("voicegroups: reload ignored; no voicegroup is selected\n");
      return;
    }
    this.applyValidBank(this.currentBank, true);
  }

  dumpstate(): void {
    this.deps.log(
      `dumpstate: currentRoot="${this.currentRoot}" currentBank="${this.currentBank}" latestSlots=${this.latest?.slots.length ?? 0}\n`,
    );
  }

  latestSnapshot(): CcomidiSnapshot | null {
    return this.latest;
  }
}
