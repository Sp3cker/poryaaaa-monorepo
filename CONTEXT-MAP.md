# Context Map

Domain contexts modeled so far. Grown lazily as terms get resolved — absence of a package here means it hasn't been modeled yet, not that it has no domain.

## Contexts

- [Voicegroup Core](./packages/voicegroup-core/CONTEXT.md) — planned package; the project-aware voicegroup macro grammar, discovery, symbol indexing, and structural analysis shared by the engine loader and the LSP. Reads `.inc` text and resolves symbols to project-relative paths; never decodes a sample.

## Relationships

- **Engine loader → Voicegroup Core**: `poryaaaa/plugin/voicegroup/` links Voicegroup Core, calls it with a project root + bank name for ordered slot records and relative paths, then materializes samples (`.bin`→PCM `WaveData`) and owns their lifetime.
- **Language service → Voicegroup Core**: `voicegroup-lsp` (C++ after the Swift port) links Voicegroup Core, loads the project index from the workspace, and runs structural analysis on the in-memory document each keystroke; keeps editor concerns (transport, completion, hover).
- **TS UI / JSON state → Voicegroup Core**: `poryaaaa-m4l` and `ccomidi` derive from generated artifacts (NAPI, generated type-code map) rather than linking.
