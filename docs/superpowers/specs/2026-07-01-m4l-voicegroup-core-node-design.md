# M4L Voicegroup-Core Node Integration Design

## Context

`just build m4l` currently fails because the M4L build path still asks CMake for the obsolete `poryaaaa_napi` target:

```text
cmake --build packages/poryaaaa/build --config Release --target poryaaaa_napi
```

The M4L Node transport still uses `packages/poryaaaa-m4l/code-src/poryaaaa-node/voicegroup-parser.ts` as its local voicegroup module. That adapter manually scans `sound/voicegroups/*.inc` and loads `poryaaaa_voicegroup.node` from the old poryaaaa CMake NAPI build. The intended replacement already exists as `packages/voicegroup-core-node`, published locally as `@poryaaaa/voicegroup-core-node`, and exposes the exact Node/M4L interface needed by M4L:

```ts
scanVoicegroupBanks(root: string): string[]
parseVoicegroup(root: string, bank: string): VoicegroupParseResult
```

## Goal

Make poryaaaa-m4l use voicegroup-core through the existing `@poryaaaa/voicegroup-core-node` NAPI wrapper so poryaaaa.amxd can load voicegroup names and slot metadata for ccomidi consumption without depending on the removed `poryaaaa_napi` CMake target.

## Non-goals

- Do not link M4L directly to the Rust `voicegroup-core` C ABI.
- Do not reimplement voicegroup parsing or project indexing in TypeScript.
- Do not keep a fallback to the old `poryaaaa_voicegroup.node` path.
- Do not change the poryaaaa/ccomidi WebSocket message contract except as required to preserve existing state delivery.
- Do not hand-edit `.amxd` device binaries unless implementation reveals a necessary patcher routing change.

## Chosen approach

Use the existing `@poryaaaa/voicegroup-core-node` package as the implementation behind the current M4L-local `voicegroup-parser.ts` module.

The M4L-facing module interface stays:

```ts
scanVoicegroupBanks(root: string): string[]
parseVoicegroup(root: string, bank: string): VoicegroupParseResult
```

The implementation changes from:

```text
manual directory scan + old poryaaaa_voicegroup.node
```

to:

```text
@poryaaaa/voicegroup-core-node
```

## Runtime/module-loading constraint

`@poryaaaa/voicegroup-core-node/index.js` loads `voicegroup_core_node.node` with `__dirname`. If esbuild bundles that package into `poryaaaa_voicegroup_server.js`, `__dirname` can point at the M4L `javascript/` output directory and the native addon may not be found.

Implementation must therefore choose one explicit runtime strategy:

1. Mark `@poryaaaa/voicegroup-core-node` external in the Node bundle and ensure the dependency is installed in `packages/poryaaaa-m4l/node_modules` for Max runtime; or
2. Copy the built `.node` addon next to the bundled M4L JS and require it through a stable local path.

The preferred implementation is option 1 because it preserves normal Node package resolution and keeps the NAPI package self-contained. If Max's Node runtime cannot reliably resolve package dependencies from the M4L package directory, implementation may switch to option 2 and document that choice in the final report.

## Data flow

```text
poryaaaa.amxd
  -> node.script poryaaaa_voicegroup_server.js
    -> poryaaaa-node/voicegroup-parser.ts
      -> @poryaaaa/voicegroup-core-node
        -> voicegroup-core Rust project index
          -> sound/voice_groups.inc
          -> declared sound/voicegroups/<bank>.inc files
```

The existing broadcast flow remains:

```text
PoryaaaaVoicegroupService snapshot
  -> ws://127.0.0.1:17777
    -> ccomidi_voicegroup_client.js
      -> state <encoded>
        -> ccomidi_voices.js
          -> ccomidi dropdown
```

## Build design

Keep `just build m4l` as the full M4L build/install workflow, but replace the obsolete `poryaaaa_napi` pre-step with the local voicegroup-core-node build:

```text
npm --prefix packages/voicegroup-core-node run build
cd packages/poryaaaa-m4l
npm run build
```

Inside `packages/poryaaaa-m4l/package.json`, `build:napi` should likewise build the local `voicegroup-core-node` package instead of CMake's removed `poryaaaa_napi` target. This ensures package-local M4L commands and the root `just build m4l` path agree.

## Test design

Update M4L parser tests so they match voicegroup-core semantics:

- `scanVoicegroupBanks()` lists banks declared by `sound/voice_groups.inc`, not every `.inc` under `sound/voicegroups`.
- `parseVoicegroup()` returns 128 M4L-compatible slots with `{ name, typeCode }` and null holes.
- parse failures return M4L-friendly diagnostic strings through the existing `VoicegroupParseResult` union.

Verify both touched packages:

```text
packages/voicegroup-core-node: npm test
packages/poryaaaa-m4l: npm run check
packages/poryaaaa-m4l: npm test
packages/poryaaaa-m4l: npm run build:node
repo root: just build m4l
```

## Acceptance criteria

- `just build m4l` no longer references the missing `poryaaaa_napi` target.
- M4L parser code uses `@poryaaaa/voicegroup-core-node` as its voicegroup source.
- M4L runtime can load the native addon from the built Node bundle context.
- M4L parser tests demonstrate voicegroup-core project-index behavior.
- The ccomidi WebSocket dropdown receives the same slot shape as before: `{ name, typeCode } | null` across 128 slots.
- Both `voicegroup-core-node` and `poryaaaa-m4l` focused checks pass, or any environmental failure is reported with exact output.
