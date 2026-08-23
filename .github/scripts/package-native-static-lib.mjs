import { execFileSync } from "node:child_process";
import { createHash } from "node:crypto";
import { cpSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, join } from "node:path";

const [target, sourceLibrary, runner] = process.argv.slice(2);
if (!target || !sourceLibrary || !runner) {
  throw new Error(
    "usage: package-native-static-lib.mjs <target> <library> <runner>",
  );
}
const commit = process.env.GITHUB_SHA;
if (!commit || !/^[0-9a-f]{40}$/.test(commit)) {
  throw new Error(
    "GITHUB_SHA must contain the exact 40-character source commit",
  );
}
const runnerArch = process.env.RUNNER_ARCH ?? process.arch;
const expectedArch = target.startsWith("aarch64-") ? "ARM64" : "X64";
if (runnerArch.toUpperCase() !== expectedArch) {
  throw new Error(`runner architecture ${runnerArch} does not match ${target}`);
}

const crateRoot = "packages/voicegroup-core";
const artifactRoot = join("native-static-lib", target);
const includeDir = join(artifactRoot, "include");
const libraryDir = join(artifactRoot, "lib", target);
mkdirSync(includeDir, { recursive: true });
mkdirSync(libraryDir, { recursive: true });

const headerSource = join(crateRoot, "include", "voicegroup_core.h");
const headerText = readFileSync(headerSource, "utf8");
const abiMatch = headerText.match(
  /^#define VOICEGROUP_CORE_ABI_VERSION ([1-9][0-9]*)$/m,
);
if (!abiMatch) {
  throw new Error("voicegroup_core.h does not define a valid ABI version");
}
const abiVersion = Number(abiMatch[1]);
const headerDestination = join(includeDir, "voicegroup_core.h");
const libraryDestination = join(libraryDir, basename(sourceLibrary));
cpSync(headerSource, headerDestination);
cpSync(sourceLibrary, libraryDestination);

const sha256 = (path) =>
  createHash("sha256").update(readFileSync(path)).digest("hex");
const metadata = JSON.parse(
  execFileSync("cargo", [
    "metadata",
    "--format-version",
    "1",
    "--manifest-path",
    join(crateRoot, "Cargo.toml"),
  ], {
    encoding: "utf8",
  }),
);
const crate = metadata.packages.find(({ name }) => name === "voicegroup-core");
if (!crate) {
  throw new Error("cargo metadata did not contain voicegroup-core");
}
const rustc = execFileSync("rustc", ["-vV"], { encoding: "utf8" }).trim();
const licenses = metadata.packages
  .map(({ name, version, license, source }) => ({
    name,
    version,
    license: license ?? "NOASSERTION",
    source: source ?? "workspace",
  }))
  .sort((left, right) =>
    left.name.localeCompare(right.name) ||
    left.version.localeCompare(right.version)
  );
writeFileSync(
  join(artifactRoot, "licenses.json"),
  `${JSON.stringify(licenses, null, 2)}\n`,
);

const librarySha256 = sha256(libraryDestination);
const headerSha256 = sha256(headerDestination);
const workflowArtifactName = `voicegroup-core-${target}`;
const archiveName = basename(sourceLibrary);
const manifest = {
  schema_version: 1,
  source_commit: commit,
  crate_version: crate.version,
  abi_version: abiVersion,
  wave_cache: "unbounded-per-load-absolute-path",
  target_triple: target,
  architecture: target.split("-")[0],
  runner,
  runner_arch: runnerArch,
  artifact_name: archiveName,
  artifact_sha256: librarySha256,
  workflow_artifact_name: workflowArtifactName,
  rustc,
  profile: "release",
  library: `lib/${target}/${basename(sourceLibrary)}`,
  lib_sha256: librarySha256,
  header: "include/voicegroup_core.h",
  header_sha256: headerSha256,
  license_inventory: "licenses.json",
};
writeFileSync(
  join(artifactRoot, "manifest.json"),
  `${JSON.stringify(manifest, null, 2)}\n`,
);
writeFileSync(
  join(artifactRoot, "SHA256SUMS"),
  `${manifest.lib_sha256}  ${manifest.library}\n${manifest.header_sha256}  ${manifest.header}\n`,
);
