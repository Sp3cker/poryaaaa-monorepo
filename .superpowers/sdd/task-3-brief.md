### Task 3: Verify runtime bundle and full M4L build

**Files:**
- Verify generated artifacts under `packages/poryaaaa-m4l/javascript/` only through build output; do not hand-edit them.
- No source file edits expected unless a verification command exposes a concrete issue.

**Interfaces:**
- Consumes: completed Task 1 and Task 2 outputs.
- Produces: rebuilt JS bundles that leave `@poryaaaa/voicegroup-core-node` as a runtime external and a successful full M4L build path.

- [ ] **Step 1: Verify voicegroup-core-node package tests**

Run from `packages/voicegroup-core-node`:

```bash
npm test
```

Expected: PASS. This also runs the package `pretest`, building `voicegroup_core_node.node` first.

- [ ] **Step 2: Verify M4L typecheck**

Run from `packages/poryaaaa-m4l`:

```bash
npm run check
```

Expected: PASS with no TypeScript errors.

- [ ] **Step 3: Verify M4L tests**

Run from `packages/poryaaaa-m4l`:

```bash
npm test
```

Expected: PASS for all `code-src/test/*.test.ts` tests.

- [ ] **Step 4: Verify Node bundle builds with externalized NAPI package**

Run from `packages/poryaaaa-m4l`:

```bash
npm run build:node
```

Expected: PASS. The generated `javascript/poryaaaa_voicegroup_server.js` should not inline `voicegroup-core-node/index.js`; it should contain a runtime require for `@poryaaaa/voicegroup-core-node`.

- [ ] **Step 5: Verify full root M4L build**

Run from repo root:

```bash
just build m4l
```

Expected: PASS. The command must not print or execute `--target poryaaaa_napi`.

- [ ] **Step 6: Inspect the generated Node bundle contract**

Search the generated server bundle content with the built-in search tool, not shell grep:

Pattern: `@poryaaaa/voicegroup-core-node`
Path: `packages/poryaaaa-m4l/javascript/poryaaaa_voicegroup_server.js`

Expected: at least one reference remains in the generated bundle, proving esbuild externalized the package instead of bundling the native-loader wrapper.

- [ ] **Step 7: Final report**

Report:

- changed files
- test commands run and pass/fail evidence
- whether `just build m4l` completed
- whether any Max runtime caveat remains, especially if Node package resolution requires `npm install` in `packages/poryaaaa-m4l`
```
