// V8 entry point. Loaded as [v8 ccomidi_voices.js] in devices/ccomidi.amxd.
// The service below is pure of Max globals so tests can import it directly;
// the bottom of the file wires the service to Max's JS API when running in Max.
//
// Voice selection model:
//   - `live.numbox VoiceIdx` (Live parameter) is the source of truth and is
//     persisted automatically with the per-instance parameter blob. Its int
//     value is an index into the slots array published by poryaaaa over
//     WebSocket. Every value change fires `pick <idx>` into this script.
//   - `[umenu]` is the picker + display. Its outlet 0 is wired into the
//     numbox so clicking a menu item drives the source of truth. v8 keeps
//     the umenu visually in sync via `slots setsymbol <label>` after each
//     valid pick. setsymbol does not produce output, so there is no loop.
//   - On `pick`, the patcher has already sent VoiceIdx as the Program Change.
//     This script looks up the selected slot and updates the menu label.
//     Out-of-range picks show "(no voice)".

import {
  parseDictLike,
  routingChoice,
  routingChoices,
  type RoutingChoice,
} from "./ccomidi_routing_dicts";
import type { MaxAtom, MaxEventHandlers } from "./max-events";
export { parseDictLike, routingChoice, routingChoices };

export const REROUTE_CHANNEL = "ccomidi.reroute";

let nextInstanceId = 1;

export interface ServiceDeps {
  outlet: (...args: unknown[]) => void;
  post: (msg: string) => void;
  routeTrack: () => void;
}

export interface VoicesService {
  start: () => void;
  ready: () => void;
  reload: () => void;
  state: (encodedPayload: string) => void;
  pick: (idx: number) => void;
  route: () => void;
  peerReroute: (encodedPayload: string) => void;
  autorouteifnew: (done: number) => void;
  free: () => void;
}

interface StatePayload {
  slots?: unknown;
}

const PLACEHOLDER_WAITING = "(waiting for poryaaaa)";
const NO_VOICE_LABEL = "(no voice)";

// `typeCode` is the raw GBA ToneData.type byte. Multiple values map to the
// same musical family (DirectSound, Square 1, etc. with their _ALT variants).
// Codes from voicegroup_types.h (upstream).
interface Slot {
  program: number;
  name: string;
  typeCode: number;
}

function voiceFamilyTag(typeCode: number): string {
  switch (typeCode) {
    case 0x00:
    case 0x08:
    case 0x10:
      return "DS";
    case 0x01:
    case 0x09:
      return "Sq1";
    case 0x02:
    case 0x0a:
      return "Sq2";
    case 0x03:
    case 0x0b:
      return "Wav";
    case 0x04:
    case 0x0c:
      return "Nse";
    case 0x20:
      return "Cry";
    case 0x30:
      return "Cr-";
    case 0x40:
      return "Spl";
    case 0x80:
      return "Spl*";
    default:
      return "?";
  }
}

function formatVoiceLabel(slot: Slot): string {
  return `[${voiceFamilyTag(slot.typeCode)}] ${slot.name}`;
}

function parseSlots(raw: unknown): Slot[] {
  if (!Array.isArray(raw)) return [];
  const slots: Slot[] = [];
  for (const candidate of raw) {
    if (!candidate || typeof candidate !== "object" || Array.isArray(candidate)) return [];
    const slot = candidate as Record<string, unknown>;
    if (!Number.isInteger(slot.program)) return [];
    if (typeof slot.name !== "string") return [];
    if (!Number.isInteger(slot.typeCode)) return [];
    slots.push({
      program: slot.program as number,
      name: slot.name,
      typeCode: slot.typeCode as number,
    });
  }
  return slots;
}

export class CcomidiVoicesService implements VoicesService {
  private slots: Slot[] = [];
  private gated = true;
  private pendingIdx: number | null = null;

  constructor(private readonly deps: ServiceDeps) {}

  private emitWaiting(): void {
    this.deps.outlet("slots", "clear");
    this.deps.outlet("slots", "append", PLACEHOLDER_WAITING);
  }

  private emitMenu(): void {
    this.deps.outlet("slots", "clear");
    for (const s of this.slots) {
      this.deps.outlet("slots", "append", formatVoiceLabel(s));
    }
  }

  private applyPick(idx: number): void {
    if (idx < 0 || idx >= this.slots.length) {
      this.deps.outlet("slots", "setsymbol", NO_VOICE_LABEL);
      return;
    }
    const slot = this.slots[idx];
    this.deps.outlet("slots", "setsymbol", formatVoiceLabel(slot));
  }

  private reapplyPending(): void {
    if (this.gated) return;
    this.applyPick(this.pendingIdx ?? 0);
  }

  private applyState(payload: StatePayload): void {
    const next = parseSlots(payload.slots);
    if (next.length === 0) return;
    this.slots = next;
    this.gated = false;
    this.emitMenu();
    this.reapplyPending();
  }

  private load(): void {
    if (this.gated) this.emitWaiting();
  }

  private applyEncodedState(encodedPayload: string): void {
    let payload: unknown;
    try {
      payload = JSON.parse(decodeURIComponent(encodedPayload));
    } catch (_) {
      return;
    }
    if (!payload || typeof payload !== "object" || Array.isArray(payload)) return;
    this.applyState(payload as StatePayload);
  }

  private routeLocal(): void {
    this.deps.routeTrack();
    this.deps.outlet("autorouted", 1);
  }

  start(): void {
    this.load();
  }

  ready(): void {
    // Accept leaked Node/script readiness messages without changing picker state.
  }

  reload(): void {
    this.load();
  }

  state(encodedPayload: string): void {
    this.applyEncodedState(encodedPayload);
  }

  pick(idx: number): void {
    if (!Number.isInteger(idx)) return;
    this.pendingIdx = idx;
    if (this.gated) return;
    this.applyPick(idx);
  }

  route(): void {
    this.routeLocal();
  }

  peerReroute(_encodedPayload: string): void {
    this.routeLocal();
  }

  autorouteifnew(done: number): void {
    if (done !== 0) return;
    this.routeLocal();
  }

  free(): void {
    // Kept as a no-op handler because generated devices still send it
    // on patcher teardown; recorder state now comes from save-time LOM.
  }
}

function parseMidiChannel(choice: RoutingChoice): number | null {
  const text = `${choice.display_name} ${choice.identifier}`;
  if (/track\s*in/i.test(text)) return null;
  const match = text.match(
    /(?:^|[^0-9])(?:ch(?:annel)?\.?\s*)?([1-9]|1[0-6])(?:[^0-9]|$)/i,
  );
  if (!match) return null;
  return Number(match[1]) - 1;
}

function containingTrackPath(devicePath: string): string {
  const match = devicePath.match(/^(live_set tracks \d+)(?:\s|$)/);
  if (!match) throw new Error(`cannot derive track path from ${devicePath}`);
  return match[1];
}

export function trackIndexFromPath(trackPath: string): number {
  const match = trackPath.match(/^live_set tracks (\d+)(?:\s|$)/);
  if (!match) throw new Error(`cannot derive track index from ${trackPath}`);
  return Number(match[1]);
}

export function routingChannelForTrackIndex(
  channelChoices: readonly { choice: RoutingChoice; channel: number }[],
  trackIndex: number,
): { choice: RoutingChoice; channel: number } {
  const match = channelChoices.find((entry) => entry.channel === trackIndex);
  if (!match) {
    throw new Error(
      `poryaaaa exposes no MIDI input ${trackIndex + 1} for Live track ${trackIndex + 1}`,
    );
  }
  return match;
}

function isValidLiveApi(api: LiveAPI): boolean {
  const valid = (api as unknown as { valid?: unknown }).valid;
  return valid === undefined || valid === true || valid === 1;
}

function getThisDevice(): LiveAPI {
  const api = new LiveAPI(null, "this_device");
  if (!isValidLiveApi(api)) throw new Error("this_device is not valid");
  return api;
}

function installMaxHandlers(): void {
  inlets = 1;
  outlets = 2;

  function routeTrack(): void {
    const thisDevice = getThisDevice();
    const devicePath = thisDevice.unquotedpath;
    const trackPath = containingTrackPath(devicePath);
    const track = new LiveAPI(null, trackPath);
    if (!isValidLiveApi(track)) throw new Error(`${trackPath} is not valid`);

    const types = routingChoices(
      track.get("available_output_routing_types"),
      "available_output_routing_types",
    );

    const poryaaaaTypes = types.filter((choice) =>
      `${choice.identifier}\n${choice.display_name}`
        .toLowerCase()
        .includes("poryaaaa"),
    );
    if (poryaaaaTypes.length === 0) {
      throw new Error("no poryaaaa output routing target is available");
    }
    if (poryaaaaTypes.length > 1) {
      throw new Error("multiple poryaaaa output routing targets are available");
    }
    const targetType = poryaaaaTypes[0];
    track.set("output_routing_type", targetType);

    const channels = routingChoices(
      track.get("available_output_routing_channels"),
      "available_output_routing_channels",
    );
    const channelChoices = channels
      .map((choice) => ({ choice, channel: parseMidiChannel(choice) }))
      .filter(
        (entry): entry is { choice: RoutingChoice; channel: number } =>
          entry.channel !== null,
      );
    if (channelChoices.length === 0) {
      throw new Error("poryaaaa target exposes no MIDI channel routing choices");
    }

    const next = routingChannelForTrackIndex(
      channelChoices,
      trackIndexFromPath(trackPath),
    );
    track.set("output_routing_channel", next.choice);
    post(
      `ccomidi_voices: routed ${trackPath} to ${targetType.display_name} channel ${next.channel + 1}\n`,
    );
  }

  function isRoutingCorrect(): boolean {
    try {
      const thisDevice = getThisDevice();
      const devicePath = thisDevice.unquotedpath;
      const trackPath = containingTrackPath(devicePath);
      const track = new LiveAPI(null, trackPath);
      if (!isValidLiveApi(track)) return false;

      const myTrackIndex = trackIndexFromPath(trackPath);

      // Check if currently routed to a poryaaaa target
      const typeRaw = track.get("output_routing_type");
      const typeParsed = parseDictLike(typeRaw);
      const typeText = typeParsed && typeof typeParsed === "object" && !Array.isArray(typeParsed)
        ? `${(typeParsed as any).identifier || ""}\n${(typeParsed as any).display_name || ""}`
        : String(typeRaw || "");
      if (!typeText.toLowerCase().includes("poryaaaa")) {
        return false;
      }

      // Check if the current MIDI output channel number matches our track index
      // (poryaaaa exposes one channel per track index)
      const chRaw = track.get("output_routing_channel");
      const chParsed = parseDictLike(chRaw);
      let currentCh: number | null = null;
      const candidate = chParsed && typeof chParsed === "object" && !Array.isArray(chParsed)
        ? chParsed
        : (Array.isArray(chRaw) && chRaw.length > 0 ? chRaw[0] : null);
      if (candidate && typeof candidate === "object") {
        currentCh = parseMidiChannel(candidate as RoutingChoice);
      }
      return currentCh === myTrackIndex;
    } catch (_) {
      return false;
    }
  }

  const service = new CcomidiVoicesService({
    outlet: (...args) => outlet(0, ...args),
    post: (msg) => post(msg),
    routeTrack,
  });

  function atomsToString(messageName: string, args: MaxAtom[]): string | null {
    if (args.length === 0) {
      post(`ccomidi_voices: ${messageName} requires a string argument\n`);
      return null;
    }
    return args
      .map((arg) => String(arg))
      .join(" ")
      .trim();
  }

  function atomToInteger(messageName: string, args: MaxAtom[]): number | null {
    if (args.length !== 1 || typeof args[0] !== "number") {
      post(`ccomidi_voices: ${messageName} requires one number argument\n`);
      return null;
    }
    const value = args[0];
    if (!Number.isInteger(value)) {
      post(`ccomidi_voices: ${messageName} requires an integer argument\n`);
      return null;
    }
    return value;
  }

  function start(): void {
    service.start();
  }

  function ready(): void {
    service.ready();
  }

  function reload(): void {
    service.reload();
  }

  function pick(...args: MaxAtom[]): void {
    const idx = atomToInteger("pick", args);
    if (idx === null) return;
    service.pick(idx);
  }

  function canRouteNow(): boolean {
    try {
      const api = new LiveAPI(null, "this_device");
      return isValidLiveApi(api);
    } catch (_) {
      return false;
    }
  }

  function route(): void {
    if (!canRouteNow()) {
      post("ccomidi_voices: route ignored because this_device is not valid yet\n");
      return;
    }
    service.route();
    messnamed(REROUTE_CHANNEL, "reroute");
  }

  function autorouteifnew(...args: MaxAtom[]): void {
    const done = atomToInteger("autorouteifnew", args);
    if (done === null) return;
    if (done !== 0) {
      service.autorouteifnew(done);
      return;
    }
    // done === 0 means "perform autoroute now" (initial load case).
    // This message can arrive very early during device/patcher initialization,
    // before this_device is usable. Guard to avoid throwing.
    if (!canRouteNow()) {
      post("ccomidi_voices: autorouteifnew(0) ignored because this_device is not valid yet\n");
      return;
    }
    service.autorouteifnew(done);
  }

  function state(...args: MaxAtom[]): void {
    const encoded = atomsToString("state", args);
    if (encoded === null || !encoded) return;
    service.state(encoded);
  }

  function reroute(..._args: MaxAtom[]): void {
    if (isRoutingCorrect()) return;
    service.peerReroute("");
  }

  // free — fired by live.thisdevice outlet 3 (device freed).
  function free(): void {
    service.free();
  }

  const handlers: MaxEventHandlers<
    "start" | "ready" | "reload" | "pick" | "route" | "autorouteifnew" | "state" | "reroute" | "free"
  > = {
    start,
    ready,
    reload,
    pick,
    route,
    autorouteifnew,
    state,
    reroute,
    free,
  };

  Object.assign(globalThis, handlers);
}

if (typeof outlet === "function" && typeof messnamed === "function")
  installMaxHandlers();