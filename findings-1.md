 Findings

  1. Parsed and resolved models are already tangled.
     packages/voicegroup-core/include/voicegroup_core/voicegroup_document.hpp:25 makes ParsedProgram carry parse
     fields plus resolution/materialization fields: asset, voicegroupSymbol, keysplitTable. 
1.A Addressed by creating `std::variants` for each Macro 

  2. The parser lost argument ranges, which undercuts the LSP goal.
     packages/voicegroup-core/include/voicegroup_core/voicegroup_document.hpp:29 stores rawArguments as
     std::vector<std::string>, and packages/voicegroup-core/src/voicegroup_parser.cpp:198 reports all argument
     diagnostics against the whole macro range. The Swift core already had argument ranges for diagnostics/hover.
     Rebuilding this later will churn parser, analyzer, and tests.
     Fix: introduce ParsedArgument { std::string text; SourceRange range; } now. It is the canonical language model,
     not LSP-specific complexity.

  3. loadBank() hardcodes the first-section/per-file happy path in the public shape.
     packages/voicegroup-core/src/project_index.cpp:147 assumes sound/voicegroups/<bank>.inc, then packages/
     voicegroup-core/src/project_index.cpp:168 loads document.sections.front(). That works for the first fixture but
     bakes in the exact behavior that core is supposed to abstract away: bank discovery and section selection.
     Fix: add a small internal BankSource { relativePath, sectionLabel } discovery result and make loadBank() call
     findBankSource(bankName). Even if it only supports per-file today, the boundary becomes correct.

  4. Tests rely on assert, so a Release/NDEBUG test binary can become a no-op.
     packages/voicegroup-core/test/voicegroup_core_tests.cpp:47 uses raw assert() throughout. If this target is built
     with NDEBUG, the suite can report green while checking nothing. That is not acceptable for the package becoming
     the syntax source of truth.
     Fix: use a tiny local require(condition, message) helper that prints and exits, or a real test framework later.
     No dependency needed.

  5. ProjectIndex exposes copy-heavy collections and false-return stubs as public behavior.
     packages/voicegroup-core/include/voicegroup_core/project_index.hpp:20 exposes lookup methods, but keysplit/
     voicegroup lookups currently always return false at packages/voicegroup-core/src/project_index.cpp:190. Also
     directSoundAssets() returns a copied vector at packages/voicegroup-core/src/project_index.cpp:200.
     Fix: keep unfinished lookup APIs private or model them as absent until implemented. For assets, return
     std::span<const ResolvedAsset> unless callers need ownership.


     Gaps vs the plan, by severity

  1. Task 3 stubbed — and it produces false positives (highest concern). hasKeysplitSymbol and hasVoicegroup are hardcoded return
  false (project_index.cpp:190-198). The index never indexes keysplit tables or voicegroup symbols. Consequence for the LSP
  purpose: every voice_keysplit / voice_keysplit_all line will light up with unknown-voicegroup-symbol + unknown-keysplit-symbol
  against any real project. And ParsedProgram.keysplitTable is never populated, so Task 6's keysplit-materialization path would
  have nothing to copy. This isn't silent absence — it's wrong output on the most structurally interesting macros.

  2. Task 2 stubbed — no discovery, only hardcoded paths. load reads exactly two fixed files; loadBank hardcodes
  sound/voicegroups/<bank>.inc (project_index.cpp:142-151). The required pokefirered monolithic sound/voice_groups.inc layout
  (real — vg_discovery.c:214) is unsupported, and there's no keysplit-table-file discovery. Related latent bug: loadBank reads
  document.sections.front() only (:168), which silently breaks the instant a monolithic many-section file is supported.

  3. ProjectConfig dropped. Plan API was load(root, config) with extraSoundDataPaths/extraVoicegroupPaths; header is load(root)
  only. Possibly a fine simplification — but flag it as a decision, not a drift.

  Minor

  - Tests are assert()-based → vacuous under NDEBUG/Release. They pass now because the default build is Debug; a Release build
  compiles them to nothing and "passes."
  - Plan's referential-transparency test is missing. Task 4 asked for "call twice → identical." The no-file/in-memory path is
  exercised (analyze with a default-constructed index, test at :111-112), but the twice-identical assertion isn't there.
  - C++23 with no C++23 feature in use. Everything is ≤C++20 (starts_with/ends_with, span, from_chars). cxx_std_20 is the safer
  floor for the Windows cross-compat constraint — a one-line change.
  - Parser warns on every unrecognized line (:135-143). Need to confirm real voicegroup .inc files contain nothing but
  label/voice_group/macros — if they carry .align/.section, that's LSP noise. poryaaaa's vg_parser.c had explicit skips
  (:625,:632) worth diffing against.
  - ResolvedAsset.displayName (basename) was added beyond the plan API — reasonable, it pre-serves Task 6's voiceSampleNames.
  Just noting it's an addition.