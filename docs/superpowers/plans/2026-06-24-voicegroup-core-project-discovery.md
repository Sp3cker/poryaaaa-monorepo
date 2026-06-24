# Voicegroup Core Project Discovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move voicegroup source reading, discovery, tokenization, argument parsing, source indexing, and symbol validation out of `packages/poryaaaa` and into `packages/voicegroup-core`, while keeping poryaaaa responsible for runtime `ToneData`/`WaveData` materialization.

**Architecture:** `voicegroup-core` becomes the project-aware C++ voicegroup package: it takes a project root and requested bank name, discovers relevant source files, reads them, indexes symbols, parses/analyzes the selected bank, and returns ordered slot records with project-relative asset paths. `poryaaaa` keeps its C public loader API but replaces its internal voicegroup parser/discovery/indexing code with a thin C++ materialization bridge that turns core records into `LoadedVoiceGroup`.

**Tech Stack:** C++20 for `packages/voicegroup-core` and the poryaaaa bridge, C public ABI for `voicegroup_loader.h`, existing poryaaaa C runtime types, CMake, poryaaaa unit tests.

---

## Scope Decision

This plan supersedes the earlier direction in `docs/superpowers/plans/2026-06-23-voice-macro-grammar-source.md` where poryaaaa's C macro catalog stayed the source of truth. The source of truth now lives in `packages/voicegroup-core`.

`packages/poryaaaa` will no longer store package-local code for:

- voicegroup file reading
- voicegroup section/bank discovery
- voice macro tokenization
- voice macro argument parsing
- DirectSound, programmable-wave, and keysplit source indexing
- voicegroup symbol validation

`packages/poryaaaa` will still store package-local code for:

- `ToneData` and `LoadedVoiceGroup` ownership
- `WaveData` decoding/loading
- DirectSound `.bin` declaration to runtime `.wav` loading behavior
- cry `.bin` loading behavior
- programmable-wave runtime loading
- keysplit table allocation/copying
- sub-voicegroup runtime materialization
- CLAP/plugin UI state and sample-swap overrides

## File Structure

Create `packages/voicegroup-core` as the reusable C++ package:

- Create: `packages/voicegroup-core/CMakeLists.txt`
- Create: `packages/voicegroup-core/include/voicegroup_core/diagnostic.hpp`
- Create: `packages/voicegroup-core/include/voicegroup_core/macro_catalog.hpp`
- Create: `packages/voicegroup-core/include/voicegroup_core/project_index.hpp`
- Create: `packages/voicegroup-core/include/voicegroup_core/voicegroup_document.hpp`
- Create: `packages/voicegroup-core/src/macro_catalog.cpp`
- Create: `packages/voicegroup-core/src/project_index.cpp`
- Create: `packages/voicegroup-core/src/voicegroup_parser.cpp`
- Create: `packages/voicegroup-core/test/voicegroup_core_tests.cpp`

Keep poryaaaa's public C API, but replace internals with a C++ bridge:

- Modify: `packages/poryaaaa/CMakeLists.txt`
- Modify: `packages/poryaaaa/plugin/voicegroup/CMakeLists.txt`
- Modify: `packages/poryaaaa/plugin/voicegroup/voicegroup_loader.h`
- Replace: `packages/poryaaaa/plugin/voicegroup/voicegroup_loader.c` with `packages/poryaaaa/plugin/voicegroup/voicegroup_loader.cpp`
- Replace: `packages/poryaaaa/plugin/voicegroup/vg_parser.c` and `vg_parser.h` with `packages/poryaaaa/plugin/voicegroup/vg_materialize.cpp` and `vg_materialize.hpp`
- Modify: `packages/poryaaaa/plugin/voicegroup/project_asset_index.c`
- Modify: `packages/poryaaaa/plugin/voicegroup/voicegroup_project_state.c`
- Modify: `packages/poryaaaa/test/test_voicegroup_loader.c`

Delete poryaaaa source/discovery/parser modules after replacement tests pass:

- Delete: `packages/poryaaaa/plugin/voicegroup/vg_discovery.c`
- Delete: `packages/poryaaaa/plugin/voicegroup/vg_discovery.h`
- Delete: `packages/poryaaaa/plugin/voicegroup/vg_source.c`
- Delete: `packages/poryaaaa/plugin/voicegroup/vg_source.h`
- Delete: `packages/poryaaaa/plugin/voicegroup/vg_symbols.c`
- Delete: `packages/poryaaaa/plugin/voicegroup/vg_symbols.h`
- Delete: `packages/poryaaaa/plugin/voicegroup/vg_voice_macro.h`
- Delete: `packages/poryaaaa/plugin/voicegroup/vg_keysplit.c`
- Delete: `packages/poryaaaa/plugin/voicegroup/vg_keysplit.h`

Keep poryaaaa runtime helpers:

- Keep: `packages/poryaaaa/plugin/voicegroup/vg_wav.c`
- Keep: `packages/poryaaaa/plugin/voicegroup/vg_wav.h`
- Keep: `packages/poryaaaa/plugin/voicegroup/vg_paths.c`
- Keep: `packages/poryaaaa/plugin/voicegroup/vg_paths.h`
- Keep: `packages/poryaaaa/plugin/voicegroup/vg_log.c`
- Keep: `packages/poryaaaa/plugin/voicegroup/vg_log.h`
- Keep: `packages/poryaaaa/plugin/voicegroup/vg_alloc.h`

## Core API Shape

`voicegroup-core` should expose a small C++ API like this:

```cpp
namespace voicegroup {

enum class MacroKind {
    DirectSound,
    DirectSoundAlt,
    DirectSoundNoResample,
    Square1,
    Square2,
    ProgrammableWave,
    Noise,
    Keysplit,
    KeysplitAll,
    Cry,
};

enum class ArgumentKind {
    Integer,
    DirectSoundSymbol,
    ProgrammableWaveSymbol,
    VoicegroupSymbol,
    KeysplitSymbol,
};

struct MacroDefinition {
    std::string_view name;
    std::uint8_t typeCode;
    MacroKind kind;
    std::span<const ArgumentKind> arguments;
};

struct ResolvedAsset {
    std::string symbol;
    std::string relativePath;
};

struct ParsedProgram {
    int slot = 0;
    const MacroDefinition* macro = nullptr;
    std::vector<std::string> rawArguments;
    std::optional<ResolvedAsset> directSound;
    std::optional<ResolvedAsset> programmableWave;
    std::optional<std::string> voicegroupSymbol;
    std::optional<std::vector<std::uint8_t>> keysplitTable;
    std::optional<std::string> trailingComment;
    SourceRange range;
};

struct LoadedBank {
    std::string name;
    std::string sourceRelativePath;
    std::array<std::optional<ParsedProgram>, 128> programs;
    std::vector<Diagnostic> diagnostics;
};

// Result of pure tokenization of in-memory document text. No disk, no index.
// The resolution-only fields on ParsedProgram (directSound, programmableWave,
// keysplitTable) stay empty here; only loadBank() fills them against the index.
struct ParsedDocument {
    std::vector<ParsedProgram> programs;
    std::vector<Diagnostic> diagnostics;   // syntax-level only
};

struct ProjectConfig {
    std::vector<std::string> extraSoundDataPaths;
    std::vector<std::string> extraVoicegroupPaths;
};

class ProjectIndex {
public:
    static ProjectIndex load(std::filesystem::path projectRoot, ProjectConfig config = {});
    LoadedBank loadBank(std::string_view bankName) const;
    std::vector<ResolvedAsset> directSoundAssets() const;
    std::vector<ResolvedAsset> programmableWaveAssets() const;
};

// --- Pure analysis entry points (the LSP's live-diagnostics path) ---------
// Both are referentially transparent: same inputs -> same output, no I/O, no
// shared state. Safe to call on the editor's unsaved buffer every keystroke.
ParsedDocument          parseDocument(std::string_view text);
std::vector<Diagnostic> analyze(const ParsedDocument& document, const ProjectIndex& index);

// loadBank() is built on these: read file -> parseDocument -> analyze ->
// resolve assets. The loader uses loadBank(); the LSP calls analyze(
// parseDocument(buffer), index) directly so its parser/analyzer collapse into core.

} // namespace voicegroup
```

The exact container choices can change during implementation, but the boundary cannot: core returns ordered bank records plus relative paths and diagnostics; poryaaaa does not parse voicegroup source files. Two consumption paths exist on purpose — the loader's `loadBank()` (disk in, materialization records out) and the LSP's pure `parseDocument()` + `analyze()` (caller-supplied text in, diagnostics out). `loadBank()` is implemented in terms of the pure pair, so there is one parser and one analyzer, not two.

## Tasks

### Task 1: Establish `voicegroup-core` Build And Macro Catalog

- [ ] Create `packages/voicegroup-core/CMakeLists.txt` with a static library target named `voicegroup_core` and a test executable named `voicegroup_core_tests`.
- [ ] Add `macro_catalog.hpp`/`.cpp` with the canonical voice macro table, including macro name, poryaaaa-compatible type code, macro kind, and argument schema.
- [ ] Port the current macro list from `packages/poryaaaa/plugin/voicegroup/vg_voice_macro.h` and the richer argument schema from `packages/voicegroup-lsp/Sources/VoicegroupCore/MacroCatalog.swift`.
- [ ] Add a `voicegroup_core_tests` case that checks the catalog contains `voice_directsound_no_resample`, `voice_directsound_alt`, `voice_directsound`, `voice_keysplit_all`, `voice_keysplit`, `cry_reverse`, and `cry`.
- [ ] Run `cmake --build packages/poryaaaa/build --target voicegroup_core_tests` after the poryaaaa build includes the core subdirectory, or run the package-local core test target if implemented first.

### Task 2: Add Core Project Discovery And Source Reading

- [ ] Implement `ProjectIndex::load(projectRoot, config)` in `project_index.cpp`.
- [ ] Move the behavior from poryaaaa's `vg_discovery.c` into core: discover direct-sound data files, programmable-wave data files, keysplit table files, voicegroup directories, and monolithic voicegroup files.
- [ ] Preserve poryaaaa's two required layouts: pokeemerald per-file voicegroups and pokefirered monolithic `voice_groups.inc`.
- [ ] Keep paths returned by core project-relative. Do not expose absolute paths in `ResolvedAsset`.
- [ ] Add tests using temporary fixture directories for both per-file and monolithic layouts.

### Task 3: Add Core Symbol Indexing

- [ ] Move DirectSound and programmable-wave source indexing from poryaaaa's `vg_symbols.c` into `ProjectIndex`.
- [ ] Move keysplit table parsing from poryaaaa's `vg_keysplit.c` into `ProjectIndex`.
- [ ] Store DirectSound and programmable-wave assets as `{symbol, relativePath}`.
- [ ] Store keysplit tables as parsed 128-byte data associated with their source symbol.
- [ ] Add tests that prove `ProjectIndex::directSoundAssets()` and `ProjectIndex::programmableWaveAssets()` return the same symbols and relative paths poryaaaa currently exposes through `voicegroup_loader_collect_project_assets`.

### Task 4: Add Core Bank Parsing And Analysis

- [ ] Implement `parseDocument(text)` as a pure tokenizer over in-memory text: labels, `voice_group` declarations, voice macro lines, comma-separated arguments, and trailing `@` comments. No disk access, no index. Emit syntax-level diagnostics only.
- [ ] Implement `analyze(document, index)` as a pure function: validate unknown macros, wrong argument count, invalid integers, range violations, slot out of range, duplicate populated slots, missing DirectSound symbols, missing programmable-wave symbols, missing keysplit tables, and missing sub-voicegroup references (contextual checks resolved against the passed `ProjectIndex`). No disk access, no shared state.
- [ ] Implement `ProjectIndex::loadBank(bankName)` **in terms of** `parseDocument` + `analyze`: read the bank file, parse it, analyze it, then resolve DirectSound/programmable-wave arguments to project-relative asset paths and keysplit arguments to parsed table bytes + sub-voicegroup symbols, populating `LoadedBank.programs[slot]`. There must be exactly one parser and one analyzer; `loadBank` adds only file reading and asset resolution.
- [ ] Add tests for `loadBank`: directsound, CGB square/noise, programmable wave, keysplit, keysplit_all, cry, duplicate slot, and missing symbol diagnostics.
- [ ] Add a test for the LSP path: call `analyze(parseDocument(text), index)` on in-memory text (no file on disk) and assert it returns the same contextual diagnostics; assert calling it twice on the same inputs yields identical results (referential transparency).

### Task 5: Link poryaaaa To `voicegroup-core`

- [ ] Add `add_subdirectory("${PORYAAAA_MONOREPO_ROOT}/packages/voicegroup-core" "${CMAKE_CURRENT_BINARY_DIR}/voicegroup-core")` from `packages/poryaaaa/CMakeLists.txt` or another single poryaaaa CMake entry point.
- [ ] Link `voicegroup_loader` against `voicegroup_core`.
- [ ] Convert `voicegroup_loader.c` to `voicegroup_loader.cpp` so it can create a `voicegroup::ProjectIndex`.
- [ ] Keep `voicegroup_loader.h` C-callable by wrapping declarations with `extern "C"` when compiled as C++.
- [ ] Run `cmake --build build --target poryaaaa_unit_tests` from `packages/poryaaaa`.

### Task 6: Replace poryaaaa Parsing With Runtime Materialization

- [ ] Create `vg_materialize.cpp` to convert `voicegroup::LoadedBank` into `LoadedVoiceGroup`.
- [ ] For DirectSound programs, use the core-provided relative path and preserve poryaaaa's existing runtime `.bin` to `.wav` loading behavior.
- [ ] For cry programs, use the core-provided relative path and preserve poryaaaa's existing `.bin` loading behavior.
- [ ] For programmable-wave programs, use the core-provided relative path and preserve poryaaaa's existing prog-wave loading behavior.
- [ ] For keysplit programs, copy the core-provided keysplit table bytes into poryaaaa-owned memory and recursively materialize the referenced sub-voicegroup through core.
- [ ] Preserve `voiceSampleNames[slot]` by using the basename of the core-provided relative path for DirectSound, programmable-wave, and cry programs.
- [ ] Run `./build/poryaaaa_unit_tests` from `packages/poryaaaa`.

### Task 7: Move Project Asset And Project State Readers To Core Results

- [ ] Update `voicegroup_loader_collect_project_assets()` to call `ProjectIndex::directSoundAssets()` and `ProjectIndex::programmableWaveAssets()`.
- [ ] Keep `project_asset_index.c` as the plugin's override store, but stop using poryaaaa-local source indexing to rebuild it.
- [ ] Update `voicegroup_project_state_collect()` to use `ProjectIndex::loadBank()` and the returned program records instead of `vg_read_voicegroup_lines()` and `vg_voice_macro_match()`.
- [ ] Preserve the existing JSON state format unless a separate task explicitly changes the format.
- [ ] Run `./build/poryaaaa_unit_tests` from `packages/poryaaaa`.

### Task 8: Delete poryaaaa Source/Discovery/Parser Code

- [ ] Remove `vg_discovery.c`, `vg_discovery.h`, `vg_source.c`, `vg_source.h`, `vg_symbols.c`, `vg_symbols.h`, `vg_voice_macro.h`, `vg_keysplit.c`, `vg_keysplit.h`, `vg_parser.c`, and `vg_parser.h` from the poryaaaa source tree.
- [ ] Remove those files from `packages/poryaaaa/plugin/voicegroup/CMakeLists.txt`.
- [ ] Remove tests that include `vg_voice_macro.h` directly; replace them with core catalog tests or poryaaaa loader behavior tests.
- [ ] Run `rg "vg_discovery|vg_source|vg_symbols|vg_voice_macro|vg_keysplit|vg_parse_voicegroup" packages/poryaaaa` and verify there are no remaining references.
- [ ] Run `cmake --build build --target poryaaaa_unit_tests` and `./build/poryaaaa_unit_tests` from `packages/poryaaaa`.

### Task 9: Build Plugin Artifact After Loader Migration

- [ ] Run `cmake --build build --target poryaaaa` from `packages/poryaaaa`.
- [ ] Verify the installed CLAP bundle under `~/Library/Audio/Plug-Ins/CLAP/poryaaaa.clap` was refreshed by the build.
- [ ] Report any platform caveat caused by linking C++ into the formerly pure-C `voicegroup_loader` target.

## Verification

Required checks before calling the migration complete:

```bash
cd packages/poryaaaa
cmake --build build --target poryaaaa_unit_tests
./build/poryaaaa_unit_tests
cmake --build build --target poryaaaa
```

Required source checks:

```bash
rg "vg_discovery|vg_source|vg_symbols|vg_voice_macro|vg_keysplit|vg_parse_voicegroup" packages/poryaaaa
rg "voicegroup-core|voicegroup_core|ProjectIndex|LoadedBank" packages/poryaaaa packages/voicegroup-core
```

The first `rg` command should only return historical references in docs or no references at all. The second should show poryaaaa using `voicegroup-core` through the bridge and core owning the project-aware voicegroup implementation.

## Risks

- The C public loader API must remain stable because the plugin, renderer, and tests already consume it.
- `voicegroup_loader` becoming a C++-linked target may expose missing `extern "C"` declarations or CMake link-order issues.
- Recursive keysplit materialization must keep poryaaaa ownership rules intact so `voicegroup_free()` still frees every loaded sample, prog wave, subgroup, and keysplit table.
- Core must return project-relative paths. Absolute paths would couple core output to one machine and make poryaaaa config/state harder to reason about.
