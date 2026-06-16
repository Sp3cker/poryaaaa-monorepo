import { unlinkSync } from "node:fs";

type MaxApi = {
  addHandler: (name: string, handler: (...args: unknown[]) => void) => void;
  post: (msg: string) => void;
};

const maxApi = require("max-api") as MaxApi;

maxApi.addHandler("unlink", (...args) => {
  const path = args.map((arg) => String(arg)).join(" ").trim();
  unlinkTempFile(path, {
    unlinkSync,
    post: (msg) => maxApi.post(msg),
  });
});

maxApi.post("cleanup: ready\n");
export interface CleanupServiceDeps {
  unlinkSync: (path: string) => void;
  post: (msg: string) => void;
}

export function unlinkTempFile(path: string, deps: CleanupServiceDeps): boolean {
  if (!path) {
    deps.post("cleanup: unlink requires a path\n");
    return false;
  }

  try {
    deps.unlinkSync(path);
    return true;
  } catch (err) {
    deps.post(`cleanup: failed to unlink ${path}: ${String(err)}\n`);
    return false;
  }
}