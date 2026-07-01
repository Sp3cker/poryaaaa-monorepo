import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";

interface PackageJson {
  scripts?: Record<string, string>;
  dependencies?: Record<string, string>;
}

test("build:node builds voicegroup-core-node before bundling Node scripts", () => {
  const pkg = JSON.parse(readFileSync("package.json", "utf8")) as PackageJson;
  const scripts = pkg.scripts ?? {};

  assert.equal(pkg.dependencies?.["@poryaaaa/voicegroup-core-node"], "file:../voicegroup-core-node");
  assert.equal(scripts["build:napi"], "npm --prefix ../voicegroup-core-node run build");
  assert.equal(scripts["build:node"], "run-s build:napi check:node bundle:node");
  assert.match(scripts["bundle:node"] ?? "", /--external:@poryaaaa\/voicegroup-core-node/);
  assert.match(scripts["watch:node"] ?? "", /--external:@poryaaaa\/voicegroup-core-node/);
  assert.doesNotMatch(scripts["build:napi"] ?? "", /poryaaaa_napi/);
});

test("pretest builds voicegroup-core-node before M4L tests import it", () => {
  const pkg = JSON.parse(readFileSync("package.json", "utf8")) as PackageJson;
  const scripts = pkg.scripts ?? {};

  assert.equal(scripts["pretest"], "npm run build:napi");
});

test("build:externals builds voicegroup-core static C ABI before CMake", () => {
  const pkg = JSON.parse(readFileSync("package.json", "utf8")) as PackageJson;
  const scripts = pkg.scripts ?? {};

  assert.equal(scripts["build:voicegroup-core-static"], "bash scripts/build_voicegroup_core_static.sh");
  assert.equal(scripts["build:externals"], "run-s build:voicegroup-core-static build:externals:cmake");
  assert.match(scripts["build:externals:cmake"] ?? "", /\bcmake\b[\s\S]*\bcmake\s+--build\b/);
});
