import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";

import {
  ProjectStore,
  resolveProjectsJsonPath,
} from "../poryaaaa-node/project-store";

function tempFile(): string {
  return join(mkdtempSync(join(tmpdir(), "poryaaaa-store-")), "nested", "projects.json");
}

function seedStateFile(statePath: string, contents: string): void {
  mkdirSync(dirname(statePath), { recursive: true });
  writeFileSync(statePath, contents);
}

test("missing projects.json returns no voicegroup state", () => {
  const store = new ProjectStore({ statePath: tempFile() });

  assert.equal(store.readVoicegroupState(), null);
});

test("writeVoicegroupState creates parent directory and writes global root/bank", () => {
  const statePath = tempFile();
  const store = new ProjectStore({ statePath });

  store.writeVoicegroupState({ root: "/p", bank: "alpha" });

  assert.equal(existsSync(dirname(statePath)), true);
  assert.deepEqual(JSON.parse(readFileSync(statePath, "utf8")), {
    root: "/p",
    bank: "alpha",
  });
  assert.deepEqual(store.readVoicegroupState(), {
    root: "/p",
    bank: "alpha",
  });
});

test("writeVoicegroupState preserves project-keyed recorder entries", () => {
  const statePath = tempFile();
  const store = new ProjectStore({ statePath });
  seedStateFile(statePath, JSON.stringify({
    "/sets/Test.als": {
      root: "/old",
      bank: "oldbank",
      recorderFilename: "take.mid",
      recorderStart: "1.1.1",
      recorderLength: "4",
      recorderLoopStart: "2.1.1",
      recorderLoopEnd: "3.1.1",
    },
  }));

  store.writeVoicegroupState({ root: "/new", bank: "newbank" });

  assert.deepEqual(JSON.parse(readFileSync(statePath, "utf8")), {
    root: "/new",
    bank: "newbank",
    "/sets/Test.als": {
      root: "/old",
      bank: "oldbank",
      recorderFilename: "take.mid",
      recorderStart: "1.1.1",
      recorderLength: "4",
      recorderLoopStart: "2.1.1",
      recorderLoopEnd: "3.1.1",
    },
  });
});

test("readVoicegroupState falls back to latest matching legacy project bank when global bank is blank", () => {
  const statePath = tempFile();
  const store = new ProjectStore({ statePath });
  seedStateFile(statePath, JSON.stringify({
    "/sets/Old.als": { root: "/p", bank: "oldbank" },
    "/sets/Current.als": { root: "/p", bank: "currentbank" },
    root: "/p",
    bank: "",
  }));

  assert.deepEqual(store.readVoicegroupState(), {
    root: "/p",
    bank: "currentbank",
  });
});

test("readVoicegroupState falls back to latest legacy project when no global state exists", () => {
  const statePath = tempFile();
  const store = new ProjectStore({ statePath });
  seedStateFile(statePath, JSON.stringify({
    "/sets/Old.als": { root: "/old", bank: "oldbank" },
    "/sets/Current.als": { root: "/current", bank: "currentbank" },
  }));

  assert.deepEqual(store.readVoicegroupState(), {
    root: "/current",
    bank: "currentbank",
  });
});

test("malformed projects.json is preserved and backed up instead of reset on read", () => {
  const statePath = tempFile();
  const store = new ProjectStore({ statePath });
  seedStateFile(statePath, "{ not json");

  assert.equal(store.readVoicegroupState(), null);
  assert.equal(readFileSync(statePath, "utf8"), "{ not json");
  assert.equal(readFileSync(`${statePath}.bad`, "utf8"), "{ not json");
});

test("writeVoicegroupState can recover from malformed projects.json without losing the backup", () => {
  const statePath = tempFile();
  const store = new ProjectStore({ statePath });
  seedStateFile(statePath, "{ not json");

  store.writeVoicegroupState({ root: "/p", bank: "alpha" });

  assert.deepEqual(JSON.parse(readFileSync(statePath, "utf8")), {
    root: "/p",
    bank: "alpha",
  });
  assert.equal(readFileSync(`${statePath}.bad`, "utf8"), "{ not json");
});

test("resolveProjectsJsonPath preserves macOS path and updates Windows/Linux paths", () => {
  assert.equal(
    resolveProjectsJsonPath({
      platform: "darwin",
      env: {},
      homeDir: "/Users/me",
    }),
    "/Users/me/Library/Application Support/poryaaaa/projects.json",
  );

  assert.equal(
    resolveProjectsJsonPath({
      platform: "win32",
      env: { APPDATA: "C:\\Users\\me\\AppData\\Roaming" },
      homeDir: "C:\\Users\\me",
    }),
    "C:\\Users\\me\\AppData\\Roaming/poryaaaa/projects.json",
  );

  assert.equal(
    resolveProjectsJsonPath({
      platform: "linux",
      env: { XDG_CONFIG_HOME: "/home/me/.config-xdg" },
      homeDir: "/home/me",
    }),
    "/home/me/.config-xdg/poryaaaa/projects.json",
  );

  assert.equal(
    resolveProjectsJsonPath({
      platform: "linux",
      env: {},
      homeDir: "/home/me",
    }),
    "/home/me/.config/poryaaaa/projects.json",
  );
});
