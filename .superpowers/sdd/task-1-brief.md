### Task 1: Replace the M4L native-addon build contract

**Files:**
- Modify: `packages/poryaaaa-m4l/code-src/test/poryaaaa_node_build_contract.test.ts`
- Modify: `packages/poryaaaa-m4l/package.json`
- Modify: `packages/poryaaaa-m4l/package-lock.json`
- Modify: `justfile`

**Interfaces:**
- Consumes: existing npm script names `build:napi`, `build:node`, `bundle:node`.
- Produces: `build:napi` builds `../voicegroup-core-node`; `bundle:node` treats `@poryaaaa/voicegroup-core-node` as external; `just build m4l` builds `packages/voicegroup-core-node` before the full M4L build.

- [ ] **Step 1: Update the failing build-contract test**

Replace `packages/poryaaaa-m4l/code-src/test/poryaaaa_node_build_contract.test.ts` with:

```ts
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
  assert.doesNotMatch(scripts["build:napi"] ?? "", /poryaaaa_napi/);
});
```

- [ ] **Step 2: Run the focused test and verify it fails**

Run from `packages/poryaaaa-m4l`:

```bash
npm test -- code-src/test/poryaaaa_node_build_contract.test.ts
```

Expected: FAIL because `package.json` still lacks the local dependency, `build:napi` still references CMake/`poryaaaa_napi`, and `bundle:node` does not externalize `@poryaaaa/voicegroup-core-node`.

- [ ] **Step 3: Update `packages/poryaaaa-m4l/package.json` minimally**

Apply these changes in `packages/poryaaaa-m4l/package.json`:

```json
{
  "scripts": {
    "build:node": "run-s build:napi check:node bundle:node",
    "build:napi": "npm --prefix ../voicegroup-core-node run build",
    "bundle:node": "esbuild code-src/poryaaaa_voicegroup_server.ts code-src/ccomidi_voicegroup_client.ts code-src/cleanup.ts --bundle --outdir=javascript --format=cjs --platform=node --target=node24 --external:max-api --external:@poryaaaa/voicegroup-core-node --log-level=warning"
  },
  "dependencies": {
    "@poryaaaa/voicegroup-core-node": "file:../voicegroup-core-node",
    "midi-writer-js": "^3.2.1",
    "ws": "^8.21.0"
  }
}
```

Preserve the existing unrelated fields and dependency versions. Only add the local dependency and replace the two script bodies.

- [ ] **Step 4: Update `package-lock.json` through npm**

Run from `packages/poryaaaa-m4l`:

```bash
npm install
```

Expected: exit 0. `package-lock.json` gains `node_modules/@poryaaaa/voicegroup-core-node` as a local file dependency.

- [ ] **Step 5: Update `justfile` M4L build pre-step**

In the `m4l)` branch, replace:

```bash
cmake -S packages/poryaaaa -B packages/poryaaaa/build -DCMAKE_BUILD_TYPE=Release
cmake --build packages/poryaaaa/build --config Release --target poryaaaa_napi
cd packages/poryaaaa-m4l
npm run build
```

with:

```bash
npm --prefix packages/voicegroup-core-node run build
cd packages/poryaaaa-m4l
npm run build
```

Keep the full M4L `npm run build` behavior unchanged.

- [ ] **Step 6: Run the focused test and verify it passes**

Run from `packages/poryaaaa-m4l`:

```bash
npm test -- code-src/test/poryaaaa_node_build_contract.test.ts
```

Expected: PASS for the build-contract test.

---

