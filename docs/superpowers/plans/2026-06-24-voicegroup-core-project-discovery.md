# Voicegroup Core Rust Rewrite Plan

> **For agentic workers:** Use superpowers:subagent-driven-development or superpowers:executing-plans. Keep steps task-by-task, test-first, and package-local.

**Goal:** Make `packages/voicegroup-core` the Rust source of truth for voicegroup syntax, macro definitions, project indexing, structural analysis, and checked loadable bank records.

**Architecture:** Core owns parsing, indexing, analysis, and loadable records. Poryaaaa consumes loadable records through a C-shaped adapter and still owns `ToneData`, `WaveData`, sample decoding, allocation, lifetime, recursion materialization, plugin state, and audio behavior. NAPI consumes the same core records for JS snapshots and diagnostics, not audio materialization.

**Tech Stack:** Rust 2021, pest, Cargo tests, Rust FFI/C adapter for poryaaaa, N-API adapter for JS snapshots.

---

## Current State

Done:
- Rust crate scaffold, `.gitignore`, `Cargo.toml`, `Cargo.lock`.
- Pest parser over in-memory text.
- Source-preserving parsed model with ranges and diagnostics.
- Normal `voice_group` sections split from assembly `label::` symbols.
- Macro catalog with typed argument schemas and poryaaaa type codes.
- Pure analyzer with typed symbol namespaces and parser-diagnostic preservation.
- Poryaaaa-facing `ProgramBank` model with typed program payloads and build diagnostics.
- `ProjectIndex` with standard project discovery, symbol indexing, keysplit indexing, and selected-bank loading.
- C-shaped adapter with opaque index/result handles, diagnostic ranges/severity, slot kinds, type codes, relative paths, child-bank/table symbols, and typed payload accessors.
- Parser/analyzer fixture tests.
- Removed stale previous `voicegroup-core` files.
- Thermo review cleanup: removed unused NAPI dependencies until the adapter exists, made C accessors borrow bank records, and removed fabricated combined-bank fallback source.
- Verified: `cargo fmt --check`, `cargo clippy --all-targets -- -D warnings`, `cargo test`, `cargo check`.

Not done:
- NAPI adapter for JS snapshots.
- Poryaaaa loader migration.

## Core Model

Keep parser output separate from checked records:

```text
parse_document(text) -> ParsedDocument
analyze_document(parsed, &ProjectIndex::analysis_context()) -> Vec<Diagnostic>
ProjectIndex::load_program_bank(name) -> ProgramBankLoadResult
```

`ParsedDocument` is source-shaped: declarations, labels, raw arguments, comments, ranges, syntax diagnostics.

`ProgramBank` is consumer-shaped:

```rust
pub struct ProgramBank {
    pub name: String,
    pub source_relative_path: String,
    pub programs: [Option<ProgramRecord>; 128],
}

pub struct ProgramBankBuildResult {
    pub bank: ProgramBank,
    pub diagnostics: Vec<Diagnostic>,
}

pub struct ProgramBankLoadResult {
    pub bank: Option<ProgramBank>,
    pub diagnostics: Vec<Diagnostic>,
}

pub struct ProgramRecord {
    pub slot: usize,
    pub macro_name: String,
    pub type_code: VoiceType,
    pub trailing_comment: Option<String>,
    pub source_range: SourceRange,
    pub data: ProgramData,
}

pub enum ProgramData {
    DirectSound(DirectSoundProgram),
    ProgrammableWave(ProgrammableWaveProgram),
    Square1(Square1Program),
    Square2(Square2Program),
    Noise(NoiseProgram),
    Keysplit(KeysplitProgram),
    KeysplitAll(KeysplitAllProgram),
    Cry(CryProgram),
}
```

Payloads hold typed numeric values plus resolved project-relative assets or keysplit data. Do not add optional resolved fields to parser structs.

## Project Index

`ProjectIndex::load(root, config)` is the only disk-touching core tier.

It must:
- Discover per-file `sound/voicegroups/<name>.inc`.
- Discover monolithic `sound/voice_groups.inc`.
- Discover DirectSound, programmable-wave, and keysplit source files.
- Store all paths project-relative.
- Build `AnalysisContext` from indexed symbols.
- Expose directsound/prog-wave asset lists for poryaaaa asset APIs and NAPI.

Tests:
- Temporary fixture for per-file voicegroups.
- Temporary fixture for monolithic voice groups.
- DirectSound symbol -> relative sample path.
- Programmable-wave symbol -> relative sample path.
- Keysplit symbol -> `[u8; 128]`.
- `analysis_context()` produces the same diagnostics as hand-built context tests.

## Project Bank Loading

`ProjectIndex::load_program_bank(bank_name)` must:
- Read the selected source file.
- Call `parse_document`.
- Call `analyze_document`.
- If diagnostics include errors, return diagnostics and any safely built records only if unambiguous.
- Resolve symbol arguments through the index.
- Convert catalog-checked raw arguments into typed `ProgramData`.
- Populate `[Option<ProgramRecord>; 128]`.

Tests:
- DirectSound, DirectSound alt, no-resample.
- Square1, Square2, programmable wave, noise.
- Keysplit, keysplit_all, cry, cry_reverse.
- Start slot handling.
- Duplicate slot diagnostics.
- Missing symbol diagnostics.
- Referential transparency for parse/analyze path.

## Poryaaaa Consumption

Poryaaaa should see a narrow C-shaped adapter, not Rust internals:
- Stable handles for index/bank lifetimes.
- C structs/enums for diagnostics, slots, program kind, type code, relative path, and typed arguments.
- No Rust `Vec`, `String`, parser trivia, or pest details across the seam.

Poryaaaa materializer then:
- Converts `ProgramBank` slots to `LoadedVoiceGroup`.
- Decodes DirectSound/cry/prog-wave assets.
- Allocates/copies keysplit tables.
- Recurses sub-voicegroups.
- Preserves `voiceSampleNames` from asset display names.
- Keeps `voicegroup_free()` ownership correct.

Keep plugin-specific publication separate: generic load stays pure; any `projects.json` update belongs in a poryaaaa wrapper.

## NAPI Consumption

NAPI should call the same core path and return JS metadata:

```ts
{
  parsed: boolean,
  diagnostics: Diagnostic[],
  slots: Array<null | {
    slot: number,
    macroName: string,
    typeCode: number,
    kind: string,
    symbol?: string,
    relativePath?: string,
    displayName?: string,
    trailingComment?: string
  }>
}
```

NAPI must not decode samples and must not implement a second parser/catalog.

## Poryaaaa Migration

After Rust core has `ProjectIndex` and project-level bank loading:
- Link poryaaaa to the Rust adapter.
- Replace poryaaaa source discovery/parser/indexing calls with core calls.
- Keep runtime helpers: `vg_wav`, `vg_paths`, `vg_log`, `vg_alloc`.
- Delete old poryaaaa discovery/source/symbol/macro/keysplit/parser modules after tests pass.
- Update project asset and project state readers to use core results.

## Verification

Core:

```bash
cd packages/voicegroup-core
cargo fmt --check
cargo clippy --all-targets -- -D warnings
cargo test
cargo check
```

Poryaaaa after adapter work:

```bash
cd packages/poryaaaa
cmake --build build --target poryaaaa_unit_tests
./build/poryaaaa_unit_tests
cmake --build build --target poryaaaa
```

Source checks:

```bash
rg "vg_discovery|vg_source|vg_symbols|vg_voice_macro|vg_keysplit|vg_parse_voicegroup" packages/poryaaaa
rg "voicegroup-core|ProjectIndex|ProgramBank" packages/poryaaaa packages/voicegroup-core
```
