import { PoryaaaaVoicegroupService, type CcomidiVoicegroupFrame } from "./poryaaaa-node/voicegroup-service";
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

let service: PoryaaaaVoicegroupService;
const postFrame = (frame: CcomidiVoicegroupFrame) => service.post(frame);
service = new PoryaaaaVoicegroupService({
  scanBanks: scanVoicegroupBanks,
  parseVoicegroup,
  output: (out) => maxApi.outlet(out.tag, ...out.args),
  post: postFrame,
  log: (msg) => maxApi.outlet("diag", msg),
});

service.startWebSocket().then(() => {
  maxApi.outlet("ready");
  maxApi.post("voicegroups: ready\n");
}).catch((err: unknown) => {
  maxApi.outlet("diag", `websocket: failed to bind 127.0.0.1:17777: ${String(err)}\n`);
  maxApi.outlet("wsstatus", "set", "off");
});
// Handles Open Project dialog output.
maxApi.addHandler("rawroot", (...args) => {
  // Convert raw Max args to a single trimmed string for the service (common pattern for string messages).
  service.rawroot(args.map((arg) => String(arg)).join(" ").trim());
});

maxApi.addHandler("bankselect", (...args) => {
  service.bankselect(args.map((arg) => String(arg)).join(" ").trim());
});

maxApi.addHandler("reload", () => {
  service.reload();
});

maxApi.addHandler("dumpstate", () => {
  service.dumpstate();
});

maxApi.addHandler("wsstatus", () => {
  maxApi.outlet("wsstatus", "set", service.isWebSocketListening() ? "on" : "off");
});
