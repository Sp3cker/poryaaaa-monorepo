# Voicegroup Core

Planned package: the **project-aware** source of truth for the voicegroup macro grammar, its discovery, symbol indexing, and structural analysis — linked by both the poryaaaa engine loader and the voicegroup LSP. The boundary is what makes it shareable: **core reads `.inc` text, discovers the project layout, and indexes symbols to project-relative paths — but never decodes a sample into memory.** Sample/`WaveData` decoding, runtime allocation, and voicegroup lifetime stay in the engine loader, not here.

## Language

**Voice macro**:
A single `voice_*` or `cry` directive on one line of a voicegroup `.inc` file (e.g. `voice_directsound`, `voice_square_1`, `voice_keysplit`).
_Avoid_: voice, instrument, opcode.

**Macro catalog**:
The one canonical table mapping a **voice macro** name ⇄ **type code** ⇄ argument schema (count, kinds, ranges). Every consumer derives from this; none re-declares it.
_Avoid_: grammar table, macro list, opcode table.

**Type code**:
The GBA voice-type byte the **macro catalog** owns (e.g. `0x00` DirectSound). Travels to non-linking consumers (TS UI, JSON state) as a derived map, not a hand-copied switch.
_Avoid_: voice type id, kind.

**Project index**:
The materialized, **project-relative** symbol table core builds by discovering the project layout and reading its symbol-declaring `.inc` files — DirectSound symbols, programmable-wave symbols, keysplit tables — each as `{symbol, relativePath}` (or parsed table bytes). This is core's one disk-touching tier; it replaces the old injected resolver. Both consumers get the same index because both link core.
_Avoid_: symbol-resolution port, resolver, adapter, workspace.

**Structural analysis**:
Validation over a *parsed document plus a **project index*** — argument count/kind/range, slot bounds, duplicate slots, and contextual symbol resolution (does this DirectSound / keysplit / sub-voicegroup symbol exist in the index?). Pure: it runs on **in-memory document text**, so the LSP can analyze an unsaved buffer on every keystroke, and the loader can analyze a bank read from disk. Same rule, same code, both consumers.
_Avoid_: validation, linting.

**Neutral diagnostic**:
A transport-agnostic finding — `{range, severity, code, message}`. Each consumer maps it: the LSP to protocol diagnostics, the loader to `vg_log`. The LSP protocol never enters this package.
_Avoid_: LSP diagnostic, error, warning.

**Sample materialization** (what stays *out* of core):
Decoding a resolved `relativePath` into a runtime `WaveData`/PCM buffer, allocating it, and owning its lifetime through `voicegroup_free`. This is the engine loader's job alone — core hands over a relative path and stops there.
_Avoid_: load, resolve (when it means reading a `.bin`).

## Consumers

- **Engine loader** (`poryaaaa/plugin/voicegroup/`) — links voicegroup-core; calls it with a project root + bank name to get ordered slot records with project-relative paths and diagnostics; then **materializes** samples (`.bin`→PCM `WaveData`), allocates, owns lifetime, and recurses sub-voicegroups. Core does the parsing/indexing; the loader does the decoding and ownership.
- **Language service** (`voicegroup-lsp`, C++ after the port) — links voicegroup-core; loads the **project index** from the workspace, then runs **structural analysis** on the **in-memory document** each keystroke for diagnostics, plus completion/hover from the **macro catalog**. Keeps transport and editor concerns.
- **textedit** — consumes the language service, not voicegroup-core directly.
- **TS UI / JSON state** (`poryaaaa-m4l`, `ccomidi`) — derive from generated artifacts (NAPI, a generated **type code** map), not by linking.

## Flagged ambiguities

- **"Parser" / "loader"**: core *parses, discovers, indexes, and analyzes*; the engine **loader** *materializes* (decodes samples, allocates, owns memory). Reserve "loader" for the runtime materialization step; say **macro catalog** / **project index** / **structural analysis** for the shared, in-core work.
- **"Resolve"** is overloaded: core resolves a symbol to a *relative path* (in-index lookup, no disk read of the asset); the loader resolves a relative path to a *loaded PCM buffer* (disk read + decode). Name the tier when it matters.

## Example dialogue

**Dev:** If I add `voice_pcm_hybrid`, where does it go?
**Expert:** One edit — the **macro catalog**: its name, **type code**, and argument schema. The loader and the LSP both pick it up because they link voicegroup-core.

**Dev:** And checking that its sample symbol actually exists?
**Expert:** That's a contextual check inside **structural analysis**, answered against the **project index** core already built by reading the project. No injected adapter — core read the symbols, so core resolves them.

**Dev:** So the LSP analyzes a file I haven't saved?
**Expert:** Yes. **Structural analysis** runs on the **in-memory document text** plus the project index. The index comes from disk; the document being checked is the editor's live buffer.

**Dev:** So core never reads a `.bin` sample?
**Expert:** Never. Core resolves the symbol to a **relative path** and stops. Turning that path into a decoded `WaveData` is **sample materialization** — the loader's job.
