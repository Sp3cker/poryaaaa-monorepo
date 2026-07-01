# Task 3 Verification Report

## Summary

Status: DONE.

The M4L runtime bundle and build path now verify end to end in the isolated worktree. The generated Node bundle externalizes `@poryaaaa/voicegroup-core-node`, the M4L package tests and typechecks pass, the C++ externals build succeeds, and `just build m4l` completes.

During verification, the full build exposed two additional M4L externals gaps that had to be fixed for `just build m4l` to pass:

1. `source/audio/poryaaaa~/CMakeLists.txt` compiled `packages/poryaaaa/plugin/voicegroup/*.c` directly but did not include or link `packages/voicegroup-core` even though `voicegroup_loader.c` now includes `voicegroup_core.h` and calls its C ABI.
2. `source/audio/poryaaaa~/poryaaaa~.cpp` still called stale loader APIs: `voicegroup_load(..., NULL)` and the removed/undefined `voicegroup_export_channel_remap`.

The fix builds a universal macOS `voicegroup-core` static C ABI for the fat Xcode external build, links it into `poryaaaa~`, updates `voicegroup_load` to the current 2-argument API, and keeps the public `exportvoicegroup` selector while returning an explicit `voicegroupexportfailed unsupported` status until a real replacement exporter exists.

## Commands Run

### 1. `npm test`

Directory: `packages/voicegroup-core-node`

Result: PASS

Evidence from Task 3 verifier:

```text
3 tests passed, 0 failed
```

### 2. `npm run check`

Directory: `packages/poryaaaa-m4l`

Result: PASS

Evidence:

```text
> run-s check:v8 check:node check:test
> tsc -p code-src/tsconfig.v8.json --noEmit
> tsc -p code-src/tsconfig.node.json --noEmit
> tsc -p code-src/tsconfig.test.json --noEmit
```

### 3. `npm test`

Directory: `packages/poryaaaa-m4l`

Result: PASS

Evidence:

```text
> poryaaaa-m4l@0.0.0 pretest
> npm run build:napi
ℹ tests 252
ℹ pass 252
ℹ fail 0
```

The pretest hook built `@poryaaaa/voicegroup-core-node` before the M4L tests imported the parser.

### 4. `npm run build:node`

Directory: `packages/poryaaaa-m4l`

Result: PASS

Evidence from Task 3 verifier:

```text
build:napi, check:node, and bundle:node completed.
```

Bundle contract: PASS. `packages/poryaaaa-m4l/javascript/poryaaaa_voicegroup_server.js` contains a runtime `require("@poryaaaa/voicegroup-core-node")` and does not inline the native addon.

### 5. `npm run build:externals`

Directory: `packages/poryaaaa-m4l`

Result: PASS

Evidence:

```text
> run-s build:voicegroup-core-static build:externals:cmake
> bash scripts/build_voicegroup_core_static.sh
Finished `release` profile [optimized] target(s) ... (aarch64 + x86_64)
> cmake -G Xcode -B build -DM4A_DRIVER_V2=ON -DHW_AUDIO_V2=ON && cmake --build build --config  Release
** BUILD SUCCEEDED **
```

Built external artifacts confirmed under `packages/poryaaaa-m4l/externals/`:

- `poryaaaa~.mxo/Contents/MacOS/poryaaaa~`
- `porya.reverb~.mxo/Contents/MacOS/porya.reverb~`
- `ccomidi.mxo/Contents/MacOS/ccomidi`
- `ccomidi.legatobend.mxo/Contents/MacOS/ccomidi.legatobend`

### 6. `just build m4l`

Directory: repo root of `.worktrees/m4l-voicegroup-core-node`

Result: PASS

Evidence:

```text
> @poryaaaa/voicegroup-core-node@0.1.0 build
Copied .../libvoicegroup_core_node.dylib -> .../voicegroup_core_node.node
> poryaaaa-m4l@0.0.0 build
> run-s build:js build:externals install:max-package
> poryaaaa-m4l@0.0.0 build:externals
> run-s build:voicegroup-core-static build:externals:cmake
> poryaaaa-m4l@0.0.0 install:max-package
```

The command exited with code 0. Xcode printed the existing local CoreSimulator warning, but the macOS external build succeeded.

## Changed Files

Task 3 / verification follow-up changed:

- `packages/poryaaaa-m4l/package.json`
  - Adds `@poryaaaa/voicegroup-core-node` dependency.
  - Builds `../voicegroup-core-node` before Node bundling and before `npm test`.
  - Externalizes `@poryaaaa/voicegroup-core-node` in `bundle:node` and `watch:node`.
  - Adds `build:voicegroup-core-static` and runs it before CMake externals.
- `packages/poryaaaa-m4l/package-lock.json`
  - Records the local `file:../voicegroup-core-node` dependency.
- `packages/poryaaaa-m4l/code-src/poryaaaa-node/voicegroup-parser.ts`
  - Delegates bank scanning/parsing to `@poryaaaa/voicegroup-core-node`.
- `packages/poryaaaa-m4l/code-src/test/poryaaaa_node_voicegroup_parser.test.ts`
  - Updates parser tests to cover project-index/core-node semantics.
- `packages/poryaaaa-m4l/code-src/test/poryaaaa_node_build_contract.test.ts`
  - Asserts Node bundling externalizes the local package, `pretest` builds the native addon before tests import it, and externals build runs the static C ABI step before CMake.
- `packages/poryaaaa-m4l/scripts/build_voicegroup_core_static.sh`
  - Builds the Rust staticlib for both `aarch64-apple-darwin` and `x86_64-apple-darwin`, combines them with `lipo`, and regenerates `include/voicegroup_core.h`.
- `packages/poryaaaa-m4l/source/audio/poryaaaa~/CMakeLists.txt`
  - Imports and links the correct `voicegroup-core` static C ABI for the active/fat macOS architecture set, with a voicegroup-core path independent of `PORYA_ROOT` overrides.
- `packages/poryaaaa-m4l/source/audio/poryaaaa~/poryaaaa~.cpp`
  - Updates `voicegroup_load` to the current 2-argument API.
  - Keeps `exportvoicegroup` registered but reports `voicegroupexportfailed unsupported` because the previous exporter function no longer exists.
- `justfile`
  - `just build m4l` no longer runs the obsolete `poryaaaa_napi` CMake target; the package build owns the local Node/native build steps.

## Runtime Caveat

Max/Node package resolution still depends on the Max package install symlink pointing at this built package, which `npm run build` / `just build m4l` performs via `scripts/install_max_package.sh`.

If a clean machine lacks the Rust x86 target needed for Max's fat Xcode build, install it once with:

```bash
rustup target add x86_64-apple-darwin
```

The build script reports this prerequisite explicitly if it is missing.
