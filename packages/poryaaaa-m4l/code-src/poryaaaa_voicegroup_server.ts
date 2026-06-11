import { ProjectStore } from "./poryaaaa-node/project-store";
import { PoryaaaaVoicegroupService } from "./poryaaaa-node/voicegroup-service";
import {
  parseVoicegroup,
  scanVoicegroupBanks,
} from "./poryaaaa-node/voicegroup-parser";

type MaxApi = {
  addHandler: (name: string, handler: (...args: unknown[]) => void) => void;
  outlet: (...args: unknown[]) => void;
  post: (msg: string) => void;
};

const maxApi = require("max-api") as MaxApi;



const store = new ProjectStore();
const service = new PoryaaaaVoicegroupService({
  scanBanks: scanVoicegroupBanks,
  parseVoicegroup,
  readVoicegroupState: () => store.readVoicegroupState(),
  writeVoicegroupState: (state) => store.writeVoicegroupState(state),
  output: (out) => maxApi.outlet(out.tag, ...out.args),
  post: (msg) => maxApi.post(msg),
});

service.startWebSocket().catch((err: unknown) => {
  maxApi.post(`voicegroups websocket: failed to bind 127.0.0.1:17777: ${String(err)}\n`);
});

maxApi.addHandler("rawroot", (...args) => {
  // Convert raw Max args to a single trimmed string for the service (common pattern for string messages).
  service.rawroot(args.map((arg) => String(arg)).join(" ").trim());
});

maxApi.addHandler("bankselect", (...args) => {
  // Convert raw Max args to a single trimmed string for the service (common pattern for string messages).
  service.bankselect(args.map((arg) => String(arg)).join(" ").trim());
});

maxApi.addHandler("reload", () => {
  service.reload();
});

maxApi.addHandler("restore", () => {
  service.restore();
});

maxApi.addHandler("dumpstate", () => {
  service.dumpstate();
});

maxApi.addHandler("wsstatus", () => {
  maxApi.outlet("wsstatus", "set", service.isWebSocketListening() ? "on" : "off");
});

maxApi.outlet("ready");
maxApi.post("voicegroups: ready\n");
