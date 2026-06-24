# Context Map

Domain contexts modeled so far. Grown lazily as terms get resolved — absence of a package here means it hasn't been modeled yet, not that it has no domain.

## Contexts

- [Voicegroup Core](./packages/voicegroup-core/CONTEXT.md) — planned package; the I/O-free voicegroup macro grammar + structural analysis shared by the engine loader and the LSP.

## Relationships

- **Engine loader → Voicegroup Core**: `poryaaaa/plugin/voicegroup/` links Voicegroup Core and satisfies its symbol-resolution port with a sample-resolver adapter; keeps sample loading and lifecycle.
- **Language service → Voicegroup Core**: `voicegroup-lsp` (C++ after the Swift port) links Voicegroup Core and satisfies the port with a project-index adapter; keeps editor concerns.
- **TS UI / JSON state → Voicegroup Core**: `poryaaaa-m4l` and `ccomidi` derive from generated artifacts (NAPI, generated type-code map) rather than linking.
