import { copyFileSync, existsSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const packageRoot = dirname(fileURLToPath(import.meta.url));

const cargoTarget = (() => {
  if (process.platform !== "darwin") return null;
  switch (process.arch) {
    case "arm64":
      return "aarch64-apple-darwin";
    case "x64":
      return "x86_64-apple-darwin";
    default:
      return null;
  }
})();

const cargoArgs = ["build", "--release"];
if (cargoTarget) cargoArgs.push("--target", cargoTarget);
const cargo = spawnSync("cargo", cargoArgs, {
  cwd: packageRoot,
  stdio: "inherit",
});

if (cargo.status !== 0) {
  process.exit(cargo.status ?? 1);
}

const targetDir = process.env.CARGO_TARGET_DIR
  ? join(process.env.CARGO_TARGET_DIR, ...(cargoTarget ? [cargoTarget] : []), "release")
  : join(packageRoot, "target", ...(cargoTarget ? [cargoTarget] : []), "release");

const platformLibrary = (() => {
  switch (process.platform) {
    case "darwin":
      return "libvoicegroup_core_node.dylib";
    case "linux":
      return "libvoicegroup_core_node.so";
    case "win32":
      return "voicegroup_core_node.dll";
    default:
      throw new Error(`Unsupported platform for voicegroup-core-node: ${process.platform}`);
  }
})();

const source = join(targetDir, platformLibrary);
if (!existsSync(source)) {
  throw new Error(`Cargo finished but native library is missing: ${source}`);
}

const destination = join(packageRoot, "voicegroup_core_node.node");
mkdirSync(dirname(destination), { recursive: true });
copyFileSync(source, destination);
console.log(`Copied ${source} -> ${destination}`);
