# Voicegroup Core

Planned package: the **project-aware** source of truth for voicegroup source syntax, macro definitions, symbol indexing, structural analysis, and loadable voicegroup records. The boundary is what makes it shareable: **core reads `.inc` text, discovers the project layout, and indexes symbols to project-relative paths — but never decodes a sample into memory.** Sample/`WaveData` decoding, runtime allocation, and voicegroup lifetime stay in the poryaaaa engine loader, not here.

## Language

**Voice macro**:
A single `voice_*` or `cry` directive on one line of a voicegroup `.inc` file (e.g. `voice_directsound`, `voice_square_1`, `voice_keysplit`).
_Avoid_: voice, instrument, opcode.

**Macro catalog**:
The one canonical table mapping a **voice macro** name ⇄ **type code** ⇄ argument schema (count, kinds, ranges). Every consumer derives from this; none re-declares it.
_Avoid_: grammar table, macro list, opcode table.

**Parsed document**:
The source-level model produced from in-memory `.inc` text: labels, `voice_group` declarations, voice macro calls, parsed arguments with exact source ranges, trailing comments, and syntax diagnostics. It preserves source shape for tooling but does not resolve symbols or construct engine voices.
_Avoid_: AST (when it hides the source-range contract), raw lines, parser output.

**Voicegroup section**:
A normal voicegroup bank section introduced by `voice_group name` or `voice_group name, start_slot`. This is distinct from an assembly label such as `label::`; assembly labels may appear in symbol-declaring files, but they are not the normal voicegroup bank declaration shape.
_Avoid_: label, assembly section.

**Voicegroup file**:
A project-relative `.inc` file under `sound/voicegroups/`. It is a source file that may contain one or more **voicegroup sections** and may or may not be included from `sound/voice_groups.inc` yet. Use this term for file discovery and include completions.
_Avoid_: candidate, voicegroup.


**Type code**:
The GBA voice-type byte the **macro catalog** owns (e.g. `0x00` DirectSound). Travels to non-linking consumers (TS UI, JSON state) as a derived map, not a hand-copied switch.
_Avoid_: voice type id, kind.

**Project index**:
The materialized, **project-relative** symbol table core builds by discovering the project layout and reading its symbol-declaring `.inc` files — DirectSound symbols, programmable-wave symbols, keysplit tables — each as `{symbol, relativePath}` (or parsed table bytes). This is core's one disk-touching tier; it replaces the old injected resolver. Both consumers get the same index because both link core.
_Avoid_: symbol-resolution port, resolver, adapter, workspace.

**Structural analysis**:
Validation over a *parsed document plus a **project index*** — argument count/kind/range, slot bounds, duplicate slots, and contextual symbol resolution (does this DirectSound / keysplit / sub-voicegroup symbol exist in the index?). Pure: it runs on **in-memory document text**, so tooling can analyze unsaved buffers and the poryaaaa loader can analyze a bank read from disk. Same rule, same code, both consumers.
_Avoid_: validation, linting.

**Loadable voicegroup bank**:
The checked, slot-ordered result poryaaaa consumes after **structural analysis**: voice records with macro identity, type code, normalized arguments, and project-relative asset references. It is not a parser model and contains no editor-only trivia.
_Avoid_: parsed bank, runtime bank, engine voice array.

**Neutral diagnostic**:
A transport-agnostic finding — `{range, severity, code, message}`. Each consumer maps it to its own reporting channel. Editor protocols, plugin status messages, and engine logging formats never enter this package.
_Avoid_: protocol diagnostic, error, warning.

**Sample materialization** (what stays *out* of core):
Decoding a resolved `relativePath` into a runtime `WaveData`/PCM buffer, allocating it, and owning its lifetime through `voicegroup_free`. This is the engine loader's job alone — core hands over a relative path and stops there.
_Avoid_: load, resolve (when it means reading a `.bin`).

## Consumers

- **poryaaaa engine loader** (`poryaaaa/plugin/voicegroup/`) — consumes a **loadable voicegroup bank** plus **neutral diagnostics** from voicegroup-core; then **materializes** samples (`.bin`→PCM `WaveData`), allocates, owns lifetime, and recurses sub-voicegroups. Core does parsing, indexing, structural analysis, and loadable record construction; the loader does decoding and ownership.
- **Rust/plugin tooling** — consumes **parsed documents**, **structural analysis**, and **neutral diagnostics** directly when it needs source ranges, editor-like feedback, or project-aware inspection.
- **C/Poryaaaa adapter** — exposes only stable handles and C-shaped data for poryaaaa. It must not expose Rust `Vec`, Rust `String`, parser trivia, or internal module layout.
- **TS UI / JSON state** (`poryaaaa-m4l`, `ccomidi`) — derive from generated artifacts (NAPI, a generated **type code** map), not by linking.

## Flagged ambiguities

- **"Parser" / "loader"**: core *parses, discovers, indexes, analyzes, and constructs a loadable bank*; the engine **loader** *materializes* (decodes samples, allocates, owns memory). Reserve "loader" for the runtime materialization step; say **parsed document** / **macro catalog** / **project index** / **structural analysis** / **loadable voicegroup bank** for the shared, in-core work.
- **"Resolve"** is overloaded: core resolves a symbol to a *relative path* (in-index lookup, no disk read of the asset); the loader resolves a relative path to a *loaded PCM buffer* (disk read + decode). Name the tier when it matters.

## Example dialogue

**Dev:** If I add `voice_pcm_hybrid`, where does it go?
**Expert:** One edit — the **macro catalog**: its name, **type code**, and argument schema. Structural analysis and loadable bank construction both pick it up because they share voicegroup-core.

**Dev:** And checking that its sample symbol actually exists?
**Expert:** That's a contextual check inside **structural analysis**, answered against the **project index** core already built by reading the project. No injected adapter — core read the symbols, so core resolves them.

**Dev:** So tooling can analyze a file I haven't saved?
**Expert:** Yes. **Structural analysis** runs on the **in-memory document text** plus the project index. The index comes from disk; the document being checked can be a live buffer.

**Dev:** So core never reads a `.bin` sample?
**Expert:** Never. Core resolves the symbol to a **relative path** and stops. Turning that path into a decoded `WaveData` is **sample materialization** — the loader's job.
