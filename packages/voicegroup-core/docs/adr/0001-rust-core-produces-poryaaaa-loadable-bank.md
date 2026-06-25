# Rust core produces a poryaaaa-loadable bank

VoiceGroupCore is being rewritten as the canonical Rust module for voicegroup source syntax, macro definitions, project indexing, structural analysis, and checked loadable voicegroup records. The parser stays source-oriented and produces a parsed document with ranges and diagnostics; later stages turn that into a loadable voicegroup bank for poryaaaa. The poryaaaa audio engine consumes that checked result through a narrow C-shaped adapter, while sample decoding, `WaveData` allocation, voicegroup lifetime, and other engine ownership remain in poryaaaa.

## Consequences

VoiceGroupCore must be designed for both rich Rust/tooling consumers and poryaaaa's compact runtime needs. Parser internals, raw arguments, Rust collection layouts, editor protocol details, and plugin publication side effects must not leak into the poryaaaa adapter.
