import type { AddressInfo } from "node:net";
import WebSocket, { WebSocketServer } from "ws";

import { normalizeRoot } from "../scanner";
import type { VoiceSlot } from "../voice-slot-contract";
import type { VoicegroupState } from "./project-store";
import type { VoicegroupParseResult } from "./voicegroup-parser";

export interface CcomidiSnapshot {
  slots: VoiceSlot[];
}

export type PoryaaaaVoicegroupOutput =
  | { tag: "bank"; args: unknown[] }
  | { tag: "path"; args: unknown[] }
  | { tag: "voicegroup"; args: [string, string] };

export interface PoryaaaaVoicegroupServiceDeps {
  scanBanks: (root: string) => string[];
  parseVoicegroup: (root: string, bank: string) => VoicegroupParseResult;
  readVoicegroupState: () => VoicegroupState | null;
  writeVoicegroupState: (state: VoicegroupState) => void;
  output: (out: PoryaaaaVoicegroupOutput) => void;
  post: (msg: string) => void;
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
      if (this.latest) this.sendSnapshot(ws, this.latest);
    });

    wss.on("error", (err) => {
      this.deps.post(`voicegroups websocket: ${String(err)}\n`);
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

  private emit(tag: PoryaaaaVoicegroupOutput["tag"], ...args: unknown[]): void {
    if (tag === "voicegroup") {
      this.deps.output({ tag, args: args as [string, string] });
      return;
    }
    this.deps.output({ tag, args });
  }

  private isAbsoluteRoot(root: string): boolean {
    return root.startsWith("/");
  }

  private emitNoProject(): void {
    this.currentRoot = "";
    this.currentBank = "";
    this.clearLatestSnapshot();
    this.emit("path", "set", "(no project)");
    this.emit("bank", "clear");
    this.emit("bank", "append", "(no project loaded)");
  }

  private sendSnapshot(ws: WebSocket, snapshot: CcomidiSnapshot): void {
    ws.send(JSON.stringify({ type: "snapshot", slots: snapshot.slots }));
  }

  private broadcastSnapshot(snapshot: CcomidiSnapshot): void {
    if (!this.wss) return;
    const frame = JSON.stringify({ type: "snapshot", slots: snapshot.slots });
    for (const client of this.wss.clients) {
      if (client.readyState === WebSocket.OPEN) {
        client.send(frame);
      }
    }
  }

  private updateLatestSnapshot(snapshot: CcomidiSnapshot): void {
    this.latest = snapshot;
    this.broadcastSnapshot(snapshot);
  }

  private clearLatestSnapshot(): void {
    this.latest = null;
  }

  private writeStateToDisk(): void {
    if (!this.currentRoot) return;
    this.deps.writeVoicegroupState({
      root: this.currentRoot,
      bank: this.currentBank,
    });
  }

  private emitBanks(root: string, persist: boolean): string[] {
    this.currentRoot = root;
    this.currentBank = "";
    if (!this.isAbsoluteRoot(root)) {
      this.emitNoProject();
      return [];
    }
    this.clearLatestSnapshot();

    const banks = this.deps.scanBanks(root);
    this.emit("path", "set", root);
    this.emit("bank", "clear");
    if (banks.length === 0) {
      this.emit("bank", "append", "(no .inc files in sound/voicegroups)");
      this.deps.post(`voicegroups: no voicegroup banks found under ${root}\n`);
    } else {
      for (const bank of banks) this.emit("bank", "append", bank);
    }
    if (persist) this.writeStateToDisk();
    return banks;
  }

  private applyValidBank(bank: string, persist: boolean): boolean {
    if (!this.isAbsoluteRoot(this.currentRoot)) return false;
    if (!bank || bank.startsWith("(")) return false;

    const parsed = this.deps.parseVoicegroup(this.currentRoot, bank);
    if (!parsed.ok) {
      for (const diagnostic of parsed.diagnostics) {
        this.deps.post(`voicegroups: ${diagnostic}\n`);
      }
      return false;
    }

    this.currentBank = bank;
    if (persist) this.writeStateToDisk();
    this.emit("voicegroup", this.currentRoot, this.currentBank);
    this.updateLatestSnapshot({ slots: parsed.slots });
    return true;
  }

  private restoreSavedState(state: VoicegroupState): void {
    const banks = this.emitBanks(state.root, false);
    if (!state.bank) return;
    if (!banks.includes(state.bank)) {
      this.deps.post(`voicegroups: saved bank "${state.bank}" not found in current root\n`);
      return;
    }
    this.emit("bank", "setsymbol", state.bank);
    this.applyValidBank(state.bank, true);
  }

  restore(): void {
    const saved = this.deps.readVoicegroupState();
    if (!saved) {
      this.deps.post("voicegroups: no saved state\n");
      this.emitNoProject();
      return;
    }
    this.restoreSavedState(saved);
  }

  rawroot(rootPath: string): void {
    this.emitBanks(normalizeRoot(rootPath), true);
  }

  bankselect(bankName: string): void {
    this.applyValidBank(String(bankName ?? "").trim(), true);
  }

  reload(): void {
    if (!this.currentRoot || !this.currentBank) {
      this.deps.post("voicegroups: reload ignored; no voicegroup is selected\n");
      return;
    }
    this.applyValidBank(this.currentBank, false);
  }

  dumpstate(): void {
    this.deps.post(
      `dumpstate: currentRoot="${this.currentRoot}" currentBank="${this.currentBank}" latestSlots=${this.latest?.slots.length ?? 0}\n`,
    );
  }

  latestSnapshot(): CcomidiSnapshot | null {
    return this.latest;
  }
}
