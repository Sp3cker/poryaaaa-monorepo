import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";

interface AmxdBox {
  id: string;
  maxclass: string;
  text?: string;
}

interface AmxdLine {
  source: [string, number];
  destination: [string, number];
}

interface AmxdDevice {
  patcher: {
    boxes: Array<{ box: AmxdBox }>;
    lines: Array<{ patchline: AmxdLine }>;
  };
}

function loadPoryaaaaDevice(): AmxdDevice {
  const raw = readFileSync("devices/poryaaaa.amxd", "utf8");
  const start = raw.indexOf("{");
  assert.notEqual(start, -1, "poryaaaa.amxd contains patcher JSON");
  return JSON.parse(raw.slice(start).replace(/\0+$/, "")) as AmxdDevice;
}

function boxByText(device: AmxdDevice, text: string): AmxdBox | undefined {
  return device.patcher.boxes
    .map((entry) => entry.box)
    .find((candidate) => candidate.text === text);
}

function boxId(device: AmxdDevice, text: string): string {
  const box = boxByText(device, text);
  assert.ok(box, `expected box ${text}`);
  return box.id;
}

function hasLine(
  device: AmxdDevice,
  sourceId: string,
  sourceOutlet: number,
  destinationId: string,
  destinationInlet: number,
): boolean {
  return device.patcher.lines.some(({ patchline }) =>
    patchline.source[0] === sourceId &&
    patchline.source[1] === sourceOutlet &&
    patchline.destination[0] === destinationId &&
    patchline.destination[1] === destinationInlet,
  );
}

test("poryaaaa.amxd sends wsstatus only after the node script reports ready", () => {
  const device = loadPoryaaaaDevice();
  const readyRoute = boxId(device, "route ready bank path voicegroup wsstatus");
  const startupDelay = boxId(device, "delay 700");
  const wsstatusTrigger = boxId(device, "t b b");
  const startPolling = boxId(device, "1");

  assert.equal(
    hasLine(device, startupDelay, 0, wsstatusTrigger, 0),
    false,
    "startup delay must not send wsstatus before node.script is ready",
  );
  assert.equal(
    hasLine(device, startupDelay, 0, startPolling, 0),
    false,
    "startup delay must not start wsstatus polling before node.script is ready",
  );
  assert.equal(
    hasLine(device, readyRoute, 0, wsstatusTrigger, 0),
    true,
    "ready route should request immediate websocket status after handler registration",
  );
  assert.equal(
    hasLine(device, readyRoute, 0, startPolling, 0),
    true,
    "ready route should start wsstatus polling after handler registration",
  );
});

test("poryaaaa.amxd does not ask node.script to restore saved projects.json state", () => {
  const device = loadPoryaaaaDevice();
  assert.equal(
    boxByText(device, "restore"),
    undefined,
    "device must not contain a restore message that can load projects.json",
  );
});
