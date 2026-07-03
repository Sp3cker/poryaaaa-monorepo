import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync, existsSync } from "node:fs";

interface PackageJson {
  scripts?: Record<string, string>;
  dependencies?: Record<string, string>;
}

test("build:node builds voicegroup-core-node before bundling Node scripts", () => {
  const pkg = JSON.parse(readFileSync("package.json", "utf8")) as PackageJson;
  const scripts = pkg.scripts ?? {};

  assert.equal(pkg.dependencies?.["@poryaaaa/voicegroup-core-node"], "file:../voicegroup-core-node");
  assert.equal(scripts["build:napi"], "npm --prefix ../voicegroup-core-node run build");
  assert.equal(scripts["build:node"], "run-s --silent build:napi check:node bundle:node");
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
  assert.equal(scripts["build:externals"], "run-s --silent build:voicegroup-core-static build:externals:cmake");
  assert.match(scripts["build:externals:cmake"] ?? "", /--log-level=warning/i);
  assert.match(scripts["build:externals:cmake"] ?? "", /-G Ninja\b/);
  assert.match(scripts["build:externals:cmake"] ?? "", /\bbuild-ninja\b/);
  assert.doesNotMatch(scripts["build:externals:cmake"] ?? "", /\bXcode\b/);
});

test("aggregate M4L scripts use run-s --silent", () => {
  const pkg = JSON.parse(readFileSync("package.json", "utf8")) as PackageJson;
  const scripts = pkg.scripts ?? {};

  const aggregateScripts = ["build", "build:js", "build:v8", "build:node", "build:externals", "check"];
  for (const name of aggregateScripts) {
    assert.match(
      scripts[name] ?? "",
      /^run-s --silent /,
      `Script '${name}' should start with run-s --silent`
    );
  }
});

test("root justfile m4l case delegates quiet build to poryaaaa-m4l without invoking voicegroup-core-node directly", () => {
  const justfilePath = existsSync("../../justfile") ? "../../justfile" : "justfile";
  const justfile = readFileSync(justfilePath, "utf8");

  const m4lMatch = justfile.match(/m4l\)([\s\S]*?);;/);
  assert.ok(m4lMatch, "Should find the m4l target block in justfile");

  const m4lBlock = m4lMatch[1];
  assert.doesNotMatch(
    m4lBlock,
    /voicegroup-core-node/,
    "m4l target should not run voicegroup-core-node npm script directly"
  );
  assert.match(
    m4lBlock,
    /npm\s+[\s\S]*?run\s+build\s+[\s\S]*?--silent/,
    "m4l target should delegate with --silent near npm run build"
  );
});
