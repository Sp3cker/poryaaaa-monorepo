import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";

type PackageJson = {
  scripts?: Record<string, string>;
};

test("build:node builds the native voicegroup addon before bundling Node scripts", () => {
  const pkg = JSON.parse(readFileSync("package.json", "utf8")) as PackageJson;
  const scripts = pkg.scripts ?? {};

  assert.match(scripts["build:napi"] ?? "", /--target poryaaaa_napi/);
  assert.match(scripts["build:napi"] ?? "", /\.\.\/poryaaaa/);
  assert.equal(scripts["build:node"], "run-s build:napi check:node bundle:node");
});
