# Voicegroup Core

Planned package I/O-free source of truth for the voicegroup macro grammar and its structural analysis, linked by both the poryaaaa engine loader and the voicegroup LSP. The boundary rule is the package's whole point: **nothing here touches disk or a project** — sample resolution, WaveData loading, and voicegroup lifecycle stay in the engine loader, not here.

## Language

**Voice macro**:
A single `voice_*` or `cry` directive on one line of a voicegroup `.inc` file (e.g. `voice_directsound`, `voice_square_1`, `voice_keysplit`).
_Avoid_: voice, instrument, opcode.

**Macro catalog**:
The one canonical table mapping a **voice macro** name ⇄ **type code** ⇄ argument schema (count, types, ranges). Every consumer derives from this; none re-declares it.
_Avoid_: grammar table, macro list, opcode table.

**Type code**:
The GBA voice-type byte the **macro catalog** owns (e.g. `0x00` DirectSound). Travels to non-linking consumers (TS UI, JSON state) as a derived map, not a hand-copied switch.
_Avoid_: voice type id, kind.

**Structural analysis**:
Validation that needs only the parsed document plus the **macro catalog** — argument count/type/range, slot bounds, duplicate slots. Pure; shared by both consumers.
_Avoid_: validation, linting.

**Symbol-resolution port**:
The interface the analyzer calls to answer "does this symbol resolve?" (a sample symbol, a referenced keysplit table). Defined here, satisfied by a consumer **adapter** — keeps contextual checks in the package while the I/O stays injected.
_Avoid_: resolver, lookup, provider.

**Neutral diagnostic**:
A transport-agnostic finding — `{range, severity, code, message}`. Each consumer maps it: the LSP to protocol diagnostics, the loader to `vg_log`. The LSP protocol never enters this package.
_Avoid_: LSP diagnostic, error, warning.

## Consumers (adapters on the seam)

- **Engine loader** (`poryaaaa/plugin/voicegroup/`) — links voicegroup-core; supplies the sample-resolver **adapter** for the **symbol-resolution port**; keeps its own sample loading, WaveData, and sub-voicegroup recursion.
- **Language service** (`voicegroup-lsp`, C++ after the port) — links voicegroup-core; supplies a project/document-index **adapter**; keeps editor concerns (completion, hover, transport).
- **textedit** — consumes the language service, not voicegroup-core directly.
- **TS UI / JSON state** (`poryaaaa-m4l`, `ccomidi`) — derive from generated artifacts (NAPI, a generated **type code** map), not by linking.

## Flagged ambiguities

- **"Parser"** is ambiguous: the engine **loader** parses *and loads*; the package only tokenizes + analyzes. Reserve "loader" for the runtime; say **macro catalog** / **structural analysis** for the shared, I/O-free work.

## Example dialogue

**Dev:** If I add `voice_pcm_hybrid`, where does it go?
**Expert:** One edit — the **macro catalog**: its name, **type code**, and argument schema. The loader and the LSP both pick it up because they link voicegroup-core.
**Dev:** And the LSP checking that its sample symbol exists?
**Expert:** That's a contextual check, so it goes through the **symbol-resolution port**. The LSP's **adapter** answers from the project index; the loader's adapter answers from its sample resolver. The rule the analyzer runs is shared; the lookup is injected.
**Dev:** So the package never reads a `.bin`?
**Expert:** Never. The moment it needs disk, it's the loader's job, not voicegroup-core's.
