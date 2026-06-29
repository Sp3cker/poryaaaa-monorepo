# Poryaaaa NicePlug Migration Implementation Plan

**Goal:** Add a new Rust NicePlug + egui CLAP plugin for poryaaaa while keeping the m4a audio engine in C. The existing C/C++ CLAP plugin remains available until the Rust plugin can scan and render audio.

**Architecture:** Rust owns the host-facing plugin: CLAP identity, lifecycle, params, new-state format, event translation, transport mapping, and GUI. `voicegroup-core` owns voicegroup validation, `projects.json` emission, and the later audio-engine handoff shape. C remains the audio-engine seam for `M4AEngine`, `m4a_driver`, `hw_audio`, and anything after file paths are handed to the audio engine. Later recorder core stays behind explicit FFI until a later decision says otherwise.

**Tech Stack:** Rust nightly, NicePlug, nice-plug-egui, egui, `voicegroup-core`, and later C FFI/CMake native linking for Rust-to-C engine objects.

---

## Assumptions

- The Rust plugin is a new plugin with a new CLAP name/id. DAW state and automation from the existing C/C++ plugin are intentionally incompatible.
- The migration target is the scaffold in `packages/poryaaaa/plugin/Cargo.toml` and `packages/poryaaaa/plugin/src/`.
- The C audio engine behavior is not rewritten as part of this plan.
- Later Rust FFI mirrors the current C `M4AEngine` layout only inside `ffi.rs`; normal Rust modules use a safe wrapper and never rely on C layout details.
- Recorder functionality, external MIDI realtime clock parity, plugin logging, voice editing, sample overrides, and final bundle replacement are not first-pass requirements.
- Rust Cargo commands for this plugin should run from `packages/poryaaaa/plugin` so the local `rust-toolchain.toml` selects nightly.

## Current Responsibility Map

- CLAP identity/lifecycle/factory/export: `packages/poryaaaa/plugin/m4a_plugin.c`
- CLAP params: `packages/poryaaaa/plugin/m4a_params.c`, `packages/poryaaaa/plugin/m4a_params.h`
- Plugin state and `poryaaaa.cfg`: `packages/poryaaaa/plugin/m4a_plugin.c`
- Host note/MIDI/transport adapter: `packages/poryaaaa/plugin/m4a_plugin.c`
- GUI extension and timer pump: `packages/poryaaaa/plugin/m4a_plugin.c`
- Dear ImGui/Pugl UI: `packages/poryaaaa/plugin/m4a_gui.cpp`, `packages/poryaaaa/plugin/m4a_gui.h`
- Engine FFI target: `packages/poryaaaa/plugin/m4a_engine.h`
- Recorder FFI target: `packages/poryaaaa/plugin/m4a_engine_recorder.h`
- Voicegroup loader FFI target: `packages/poryaaaa/plugin/voicegroup/voicegroup_loader.h`
- Existing bundle/install wiring: `packages/poryaaaa/CMakeLists.txt`, `packages/poryaaaa/plugin/Info.plist`

## Migration Shape

- NicePlug `Plugin` replaces manual CLAP lifecycle, audio/note ports, process callback, activate/deactivate, and reset.
- NicePlug `ClapPlugin` plus `nice_export_clap!` replaces the manual descriptor, factory, and `clap_entry` for the new Rust plugin identity.
- `#[derive(Params)]` replaces the manual `CLAP_EXT_PARAMS` table for new Rust-plugin params. Old numeric CLAP param ids do not need to be preserved.
- `nice_plug_egui::create_egui_editor` and `EguiState` replace the CLAP GUI extension, timer callback, Pugl shell, and ImGui state.
- Later Rust process code reads incoming NicePlug `NoteEvent`s, translates them to C `m4a_engine_*` calls, and chunks `m4a_engine_process()` to `M4A_ENGINE_MAX_PROCESS_FRAMES`.
- Later incoming MIDI program-change messages are input-only: they call `m4a_engine_program_change()` so m4a channels choose voices.
- Rust GUI first uses General-tab snapshots plus explicit commands. It must not pass a raw plugin-data pointer into UI code like `m4a_gui_set_plugin_data()` does today.

## Voicegroup Load/Commit Decisions From 2026-06-26 Grilling

- Keep three concepts separate:
  - **Draft UI strings:** text currently typed in the editor.
  - **Committed host-visible selection:** the plugin instance's committed `project_root` and `voicegroup`/bank strings, stored in NicePlug `#[persist]` fields for the host to save as plugin state. Do not describe setting these fields as a disk write.
  - **Published project voicegroup state:** the shared `projects.json` consumed by ccomidi.
- The editor Load button is the save/commit boundary. Typing in the text fields and using Browse only mutate UI draft strings. Do not call `voicegroup-core` until Load is clicked.
- The Load transaction should be one function on `PoryaaaaPlugin`, not a new plugin-local transaction module. That function takes the draft root/bank, calls one editor-facing `voicegroup-core` interface, sets committed `#[persist]` fields only on success, and then calls the CLAP/NicePlug restart request path.
- The Rust plugin must not port `voicegroup_project_state_collect()`, `voicegroup_project_state_write()`, or any `projects.json` slot serialization. `projects.json` emission is handled by a dedicated module inside `packages/voicegroup-core`.
- `voicegroup-core` validates the draft project root and bank structurally: project discovery succeeds, selected voicegroup source parses, macro/catalog checks pass, referenced voicegroup/keysplit/source symbols resolve, and a loadable bank record can be built. It must not read sample bytes or prove that the audio engine can consume every sample path.
- `voicegroup-core` emits `projects.json` only after validation succeeds. The file must keep the current ccomidi-compatible top-level shape: `root`, `bank`, and `slots`, where each slot has `program`, `name`, `typeCode`, and optional `drumset` entries.
- The Load transaction calls one future-facing `voicegroup-core` interface. For the immediate Rust-only slice, the plugin needs the committed root/bank and one human-readable error string on failure. Later, the same interface grows or returns the file-path and organization handoff shape that `voicegroup-core` compiles for the audio engine; the plugin must not need a second core call or build that handoff shape itself.
- On validation failure, poryaaaa emits no world-visible change: committed `#[persist]` root/bank stay unchanged, `projects.json` is not replaced, no restart is requested, and the egui editor displays the `voicegroup-core` error.
- On validation success, poryaaaa sets the committed `#[persist]` root/bank, `projects.json` has already been emitted by `voicegroup-core`, and poryaaaa requests a host restart. This first slice does not need to render audio and should avoid touching C.
- Sample decoding, `LoadedVoiceGroup`, `ToneData`, and `m4a_engine_set_voicegroup()` are later audio-runtime work after `voicegroup-core` has handed file paths and organization to the audio engine. If that later audio-engine work fails after `voicegroup-core` accepts the bank, treat it as an implementation bug in either the audio-engine path or `voicegroup-core`, not as an ordinary user-facing Load outcome.
- The current Rust `voicegroup-core` probe in `plugin/src/voicegroup.rs` is diagnostics/status only and must be removed from the editor Load path or renamed into the new Load transaction. The editor must not keep a separate probe path that calls `voicegroup-core` before Load is clicked.


## Files

Immediate Rust-only Load/publish slice:
- Create: `packages/voicegroup-core/src/projects_json.rs`
- Create: `packages/voicegroup-core/src/plugin_load.rs`
- Modify: `packages/voicegroup-core/src/lib.rs`
- Modify: `packages/voicegroup-core/src/project_index.rs`
- Create: `packages/voicegroup-core/tests/plugin_load.rs`
- Modify: `packages/poryaaaa/plugin/src/plugin.rs`
- Modify: `packages/poryaaaa/plugin/src/editor.rs`
- Rename/modify: `packages/poryaaaa/plugin/src/voicegroup.rs` to `packages/poryaaaa/plugin/src/shared_projects_json.rs` if only the default `projects.json` path helper remains; keep a one-line module doc that `voicegroup-core` emits the file and the plugin only resolves the shared default path
- Modify: `packages/poryaaaa/plugin/src/lib.rs`
- Move `VoicegroupLoadStatus` out of any path-only module into the plugin/editor load surface during that rename
- Modify: `packages/poryaaaa/plugin/src/params.rs`
- Modify: `packages/poryaaaa/plugin/Cargo.toml` only if the new interface requires feature/dependency wiring

Later C/audio slice:
- Later create: `packages/poryaaaa/plugin/src/ffi.rs`
- Later create: `packages/poryaaaa/plugin/src/process.rs`
- Later modify: `packages/poryaaaa/CMakeLists.txt`
- Later inspect/remove from plugin target: `packages/poryaaaa/plugin/m4a_plugin.c`, `packages/poryaaaa/plugin/m4a_params.c`, `packages/poryaaaa/plugin/m4a_gui.cpp`

## Tasks

### Task 1: Export A Rust CLAP Skeleton

Done. Rust NicePlug CLAP skeleton builds from `packages/poryaaaa/plugin`; `_clap_entry` exists in `target/debug/libporyaaaa_clap_plugin.dylib`.

### Task 2: Port New Rust Parameters And State

Done. `params.rs` owns 16 channel program `IntParam`s, committed `project_root` + `voicegroup` `#[persist]` fields, state round-trip/default/clamp tests, and failed-commit preservation. Verified with `cargo test params` and `cargo check`.

### Task 3: Add The Voicegroup-Core Load Interface And projects.json Emitter

Done. `voicegroup-core` owns `projects_json.rs` and `plugin_load.rs`; Load validates root/bank structure, emits ccomidi-compatible `projects.json` only on success, preserves old JSON on failure, returns one editor-facing error, and does not read/decode sample bytes. Verified with `cargo test` in `packages/voicegroup-core`.

### Task 4: Wire The Rust Plugin Load Transaction And General Tab

Done except path-helper cleanup if still applicable. Rust plugin Load commits params only after `voicegroup-core` succeeds, editor drafts stay separate from committed params, Load errors display without world-visible changes, Load success emits `projects.json` and requests restart, initialize republishes restored committed state before processing, and this Rust-only slice avoids C FFI/audio runtime work. If `packages/poryaaaa/plugin/src/voicegroup.rs` is now only shared `projects.json` default-path policy, rename it to a path-specific module such as `shared_projects_json.rs` and keep `VoicegroupLoadStatus` out of that path-only module.

Prior verification included plugin config/load/editor/default-path tests, full plugin `cargo test`, `cargo check`, `_clap_entry`, `poryaaaa_unit_tests`, and voicegroup-core release staticlib/CMake reconfigure.

### Task 5: Later Add A Narrow C Engine FFI

Contract: keep C engine types opaque in Rust. Add `ffi.rs`, C lifecycle helpers `m4a_engine_create/free`, tiny accessor `voicegroup_loaded_voices`, and a safe `CPluginRuntime` cradle owning `M4AEngine*` plus `LoadedVoiceGroup*`. Rust may use existing `voicegroup_load/free` for the fast audio slice; do not duplicate voicegroup parsing/materialization or expose driver/hw/voice-editing internals.

Lifecycle: `initialize` creates runtime; `deactivate` destroys engine before freeing loaded voicegroup; `reset` calls `m4a_engine_reset` and replays program/voicegroup state only when engine remains valid. If committed root+bank are valid but C materialization fails, initialize returns true, preserves committed params, exposes error, clears runtime voicegroup binding, and renders silence.

Replacement/build: user/state voicegroup replacement while running is transactional: validate/load new bank first, commit/swap only on success, preserve previous loaded voicegroup on failure. Add Cargo-owned `build.rs` using `cc` for only fast-audio native closure; no stale CMake `.a` dependency; keep Rust plugin using Rust `voicegroup-core` directly for Load/projects.json even if C loader links the C ABI/staticlib too.

Task 5 verification baseline:

```bash
(cd packages/poryaaaa/plugin && cargo test)
(cd packages/poryaaaa/plugin && cargo build)
(cd packages/poryaaaa/plugin && nm -u target/debug/libporyaaaa_clap_plugin.dylib)
(cd packages/poryaaaa/plugin && nm target/debug/libporyaaaa_clap_plugin.dylib | grep voicegroup_core_project_index_load)
(cd packages/poryaaaa/plugin && otool -l target/debug/libporyaaaa_clap_plugin.dylib)
cmake --build packages/poryaaaa/build --target poryaaaa_unit_tests
packages/poryaaaa/build/poryaaaa_unit_tests
```

### Task 5.1: Close Runtime Ownership Review Gaps Before Continuing

Run this task after the narrow C engine FFI exists and before treating Task 6, Task 7, or Task 8 as mergeable. This task captures deterministic review fixes that do not need another product/design decision. Agents should implement them directly, using existing repo patterns; ask only if a tool proves an API boundary is impossible.

If the worktree already contains `packages/poryaaaa/plugin/src/process.rs` or a `Plugin::process` path that renders through `CPluginRuntime`, treat the branch as Task 5 + Task 6 in progress. Do not leave a half-ported process adapter in a "Task 5 only" merge. Either remove the process adapter from the Task 5 branch, or, by default for the fast audio slice, finish the Task 6 acceptance items listed below in this task and in Task 6.

- [x] Keep explicit user/state voicegroup replacement transactional while running. The editor does not receive a raw plugin/runtime pointer; it dispatches a `LoadVoicegroup` background task, validates/publishes through `voicegroup-core`, then swaps the active `CPluginRuntime` voicegroup with `CPluginRuntime::load_voicegroup`. Failed replacement leaves the previous loaded runtime voicegroup intact and surfaces the error.
- [ ] If `process.rs` remains in this branch, complete the Task 6 program-state seam now: process through a plugin-owned adapter or equivalent state owner so incoming MIDI Program Change updates the private runtime program mirror and C engine, never NicePlug host params or GUI-visible draft state. Engine reset and voicegroup reload must replay that mirror.
- [ ] If `process.rs` remains in this branch, add the Task 6 process proofs now: loaded-runtime process path produces nonzero strict-stereo overwritten output and `ProcessStatus::KeepAlive`; no-runtime and no-loaded-voicegroup paths clear stereo and return `ProcessStatus::Normal`; render chunking is tested above `M4A_ENGINE_MAX_PROCESS_FRAMES`.
- [ ] Update this plan's checkboxes to match observed implementation and verification. Check only items that are implemented and covered; leave remaining Task 6 proof items unchecked until their tests exist and pass.

Verify:

```bash
(cd packages/poryaaaa/plugin && cargo test)
(cd packages/poryaaaa/plugin && cargo build)
(cd packages/poryaaaa/plugin && cargo test process)
cmake --build packages/poryaaaa/build --target poryaaaa_unit_tests poryaaaa_engine_lifecycle
packages/poryaaaa/build/poryaaaa_unit_tests
packages/poryaaaa/build/poryaaaa_engine_lifecycle 1
```

### Task 6: Later Port The Process Adapter

- [ ] Add `packages/poryaaaa/plugin/src/process.rs` for NicePlug event translation and render chunking.
- [ ] Translate `NoteEvent::NoteOn`, `NoteEvent::NoteOff`, `NoteEvent::Choke`, `NoteEvent::MidiProgramChange`, `NoteEvent::MidiCC`, and `NoteEvent::MidiPitchBend` to existing C engine calls.
- [ ] Program changes are consumed only as engine input. Do not emit program-change events, do not mirror them into GUI state, and do not preserve the old C plugin's param echo behavior.
- [ ] Treat host program `IntParam`s as restored/default state and keep a private per-channel runtime program mirror for the current engine state. MIDI Program Change updates the runtime mirror and engine only; host param changes update the runtime mirror when observed. Engine reset/voicegroup reload replays the runtime mirror.
- [ ] Add `process.rs` tests proving incoming `MidiProgramChange` calls only the safe engine wrapper's `program_change`/`m4a_engine_program_change` path and does not update NicePlug program params or GUI-visible program state.
- [ ] Apply host tempo only for the fast audio slice: when `context.transport().tempo` is finite and positive, call `m4a_engine_set_tempo_bpm()` before consuming timed events or rendering that NicePlug process callback/sub-block. Cache `last_host_tempo_bpm` for reapply after engine reset/reinit. Defer external MIDI clock (`0xF8`/`0xFA`/`0xFB`/`0xFC`) and recorder beat mapping.
- [ ] Preserve chunking to `M4A_ENGINE_MAX_PROCESS_FRAMES`.
- [ ] Preserve sample-accurate event/render interleaving from `m4a_plugin.c`: advance an outer `frame_pos`, apply all NicePlug events whose `NoteEvent::timing() <= frame_pos`, render only until the next event time through chunked `m4a_engine_process`, then advance.
- [ ] Treat NicePlug `Buffer` as a strict stereo planar overwrite target matching the declared stereo output layout: split channel 0/1 into left/right, pass their slice pointers directly to `m4a_engine_process`, and let C overwrite/pad rendered samples. Do not pre-clear before successful render, do not accumulate, and do not add mono/surround policy. Fill silence only on explicit no-runtime/no-loaded-voicegroup fallback paths.
- [ ] Return `ProcessStatus::Normal` for no-runtime/no-loaded-voicegroup silent fallback paths; return `ProcessStatus::KeepAlive` only after a successful process block with a loaded C runtime/voicegroup. Do not use `ProcessStatus::Tail(_)` until the engine can report a real finite tail length.
- [ ] Add a pure-Rust process-order test with a mock engine that records `(timing, action)` and proves events and renders interleave, for example note at 0, render `N`, note at `N`; do not accept an implementation that drains every event before rendering the whole block.
- [ ] Add a Rust process-path audio proof: drive the process path through a loaded-runtime seam and assert a canned note/program setup produces nonzero strict-stereo overwritten output and `ProcessStatus::KeepAlive`; assert no-runtime/no-loaded-voicegroup fallback writes silence and returns `ProcessStatus::Normal`. Keep `poryaaaa_engine_lifecycle 1` as the real C backend audibility smoke; do not require a host/CLAP scan or WAV artifact comparison for this fast slice.
- [ ] Verify:

```bash
(cd packages/poryaaaa/plugin && cargo test process)
cmake --build packages/poryaaaa/build --target poryaaaa_unit_tests poryaaaa_engine_lifecycle
packages/poryaaaa/build/poryaaaa_unit_tests
packages/poryaaaa/build/poryaaaa_engine_lifecycle 1
```

### Task 7: Later Port Voices And Recorder Tabs Without Raw Plugin Pointers

- [ ] Defer this task until the Rust plugin can scan, render audio, receive a `voicegroup-core` file-path handoff, build the runtime voicegroup from that handoff, and open the General tab.
- [ ] Port Voices tab as display-only before adding edits. Editing voices, ADSR edits, CGB key/duty/period edits, sample override requests, and restore-original commands are not first-pass scope.
- [ ] Port Recorder tab through recorder FFI and Rust-owned UI state after the core plugin works. Temporary loss of recorder functionality is acceptable during the first Rust rewrite.
- [ ] Do not pass a raw `PoryaaaaPlugin*` or `M4APluginData*` equivalent into the editor.
- [ ] Verify when this later task is in scope:

```bash
(cd packages/poryaaaa/plugin && cargo test editor)
(cd packages/poryaaaa/plugin && cargo test recorder)
cmake --build packages/poryaaaa/build --target poryaaaa_unit_tests
packages/poryaaaa/build/poryaaaa_unit_tests
```

Manual verification after bundling exists: record notes, clear buffer, save SMF with host tempo, and confirm recorder indicators update.

### Task 8: Later Replace The Shipped CLAP Bundle

- [ ] Start this task only after the Rust plugin scans, receives a `voicegroup-core` file-path handoff, builds the runtime voicegroup from that handoff, renders audio from incoming MIDI notes/program changes, and opens the General tab.
- [ ] Add a bundling path that builds the Rust `cdylib` and packages it as `poryaaaa-rs.clap` or another deliberate new bundle name.
- [ ] Reuse `packages/poryaaaa/plugin/Info.plist` only if its executable name, bundle id, display name, and version are updated for the Rust artifact.
- [ ] Preserve the existing C/C++ plugin target and install loop until the Rust bundle passes scan, validation/publish, runtime voicegroup build, render, and editor checks.
- [ ] Verify:

```bash
(cd packages/poryaaaa/plugin && cargo build)
cmake --build packages/poryaaaa/build --target poryaaaa
```

Manual verification: installed Rust bundle timestamp is fresh, host scans the new Rust CLAP id, MIDI input renders sound, incoming program changes select voices, and editor opens.

## Risks

- The Rust plugin intentionally breaks old C plugin state and automation compatibility by using a new CLAP name/id and new parameter ids.
- Cargo/Rust native linkage must deliberately choose how to link the C engine, C++ recorder if/when used, voicegroup loader, `m4a_driver`, `hw_audio`, and `voicegroup-core` without duplicate static Rust symbols.
- The current GUI mutates live `ToneData*`. Rust should use snapshots and commands so GUI concerns do not leak into engine internals again; voice editing is deferred until after the first Rust plugin works.
- The existing CMake target auto-installs the C/C++ CLAP bundle. The Rust workflow must not replace that developer loop until Rust scan, validation/publish, runtime voicegroup build, render, and editor checks pass.
- External MIDI realtime clock support and recorder parity are deferred. Do not fork or patch NicePlug for `0xF8`, `0xFA`, `0xFB`, or `0xFC` in the first rewrite.
- Project-level `projects.json` emission must remain inside `voicegroup-core`; duplicating legacy C project-state serialization in the Rust plugin would create a second source of truth.
- `voicegroup-core` must not turn validation into sample decoding or runtime voice construction. Checking sample file contents or decoding sample bytes belongs to later audio-engine work.

## Stop Conditions

- Stop before deleting old C plugin files unless the Rust CLAP bundle scans, validates/publishes voicegroup state, builds the runtime voicegroup from the audio-engine handoff, renders audio, opens the editor, and passes focused tests.
- Stop if Rust-to-C native linkage requires changing C engine semantics. That belongs in a separate engine-seam decision.
- Stop if a first-pass FFI addition would expose engine internals outside `ffi.rs` or the safe engine wrapper.
- Stop if implementing General-tab Load requires the Rust plugin to serialize or merge project voicegroup files itself. Add/finish the missing `voicegroup-core` API instead.
- Stop if the immediate Rust-only Load slice requires sample decoding, runtime voicegroup construction, `LoadedVoiceGroup`, `ToneData`, or a native C link change. Those belong to the later audio-engine handoff slice.
