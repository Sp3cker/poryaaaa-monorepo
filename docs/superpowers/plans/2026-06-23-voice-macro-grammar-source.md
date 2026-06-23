# Voice Macro Grammar Source Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the C voice-macro catalog the source of truth for macro names, type codes, and argument schema used by the parser, M4L Node bridge, and editor tooling.

**Architecture:** Do not introduce a new grammar DSL. Deepen the existing C module around `vg_voice_macro.h`/`vg_parser.c`, expose catalog data through small native interfaces, and make JavaScript consume NAPI results instead of re-parsing voicegroup source.

**Tech Stack:** C, NAPI, TypeScript, Swift `VoicegroupCore`, existing voicegroup parser tests.

---

## My Take

This is real duplication, but the lazy path is not codegen. The existing C voicegroup parser is already the runtime truth; make it queryable and stop copying its tables into every consumer.

## Files

- Modify: `packages/poryaaaa/plugin/voicegroup/vg_voice_macro.h`
- Modify: `packages/poryaaaa/plugin/voicegroup/vg_parser.c`
- Modify: `packages/poryaaaa/plugin/voicegroup/voicegroup_project_state.c`
- Modify: `packages/poryaaaa-m4l/source/audio/poryaaaa~/CMakeLists.txt`
- Modify: `packages/poryaaaa-m4l/code-src/poryaaaa-node/voicegroup-parser.ts`
- Modify: `packages/poryaaaa-m4l/code-src/test/poryaaaa_node_voicegroup_parser.test.ts`
- Modify later: `packages/voicegroup-lsp/Sources/VoicegroupCore/MacroCatalog.swift`

## Tasks

### Task 1: Freeze Current Catalog Behavior

- [ ] Add or extend a poryaaaa voicegroup test that checks every macro keyword in `vg_voice_macros` is recognized.
- [ ] Include `voice_directsound_alt`, `voice_directsound_no_resample`, `voice_square_1_alt`, `voice_keysplit_all`, `cry`, and `cry_reverse` in the fixture.
- [ ] Run the focused poryaaaa voicegroup test and record the exact command in the commit message.

### Task 2: Add Catalog Metadata To The C Module

- [ ] Extend `VoicegroupMacro` with argument count and argument kind data needed by consumers.
- [ ] Keep match order in `vg_voice_macros`; more specific keywords stay before shorter base names.
- [ ] Add a focused C test that fails if the catalog has duplicate keywords or duplicate incompatible type-code entries.
- [ ] Run the focused poryaaaa voicegroup test.

### Task 3: Expose Catalog Through NAPI

- [ ] Add `getVoiceMacroCatalog()` to `poryaaaa_voicegroup.node`.
- [ ] Return plain data: `keyword`, `typeCode`, `kind`, and `args`.
- [ ] Update `voicegroup-parser.ts` to call native parsing for file content and native catalog data for UI/tooling metadata.
- [ ] Keep `scanVoicegroupBanks()` as file discovery only; it may read directory names, not parse voice macros.
- [ ] Remove `maxApi.post(res)` from `voicegroup-parser.ts`; parser modules should not log as a side effect.
- [ ] Run `npm test` or the nearest focused poryaaaa-m4l parser tests.

### Task 4: Collapse Swift Catalog Drift

- [ ] Replace hard-coded macro names in `MacroCatalog.swift` with data loaded from the same native catalog seam or a generated fixture committed beside Swift tests.
- [ ] Keep Swift-only editor behavior in Swift; only macro facts move to the shared source.
- [ ] Add a Swift test that compares Swift-visible macro names against the C catalog output.
- [ ] Run the focused `voicegroup-lsp` Swift test target.

## Verification

- `cmake --build packages/poryaaaa/build --config Release --target poryaaaa_unit_tests`
- `npm test` from `packages/poryaaaa-m4l` or the focused parser test command if narrower.
- Focused Swift tests for `VoicegroupCore`.

