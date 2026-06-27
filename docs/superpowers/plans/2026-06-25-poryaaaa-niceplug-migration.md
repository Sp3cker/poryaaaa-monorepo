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

Progress verified: `cargo build` from `packages/poryaaaa/plugin`; `_clap_entry` present in `target/debug/libporyaaaa_clap_plugin.dylib`.

### Task 2: Port New Rust Parameters And State

- [x] Add `packages/poryaaaa/plugin/src/params.rs` with 16 stepped `IntParam`s for channel programs 0..127, defaults 0..15, using new stable NicePlug string ids.
- [x] Keep committed project root and voicegroup/bank as NicePlug `#[persist]` fields on `PoryaaaaParams`. Do not add `state.rs` just to store these values.
- [x] Ensure committed root/bank fields are set only after the editor-facing `voicegroup-core` Load interface succeeds. Failed validation must leave committed selection unchanged.
- [x] Add tests for default params, program range clamping, committed root/bank state round trip, and failed-commit preservation of the previous committed root/bank.
- [x] Verify:

```bash
(cd packages/poryaaaa/plugin && cargo test params)
(cd packages/poryaaaa/plugin && cargo check)
```

### Task 3: Add The Voicegroup-Core Load Interface And projects.json Emitter

- [x] Add `packages/voicegroup-core/src/projects_json.rs` as the only module that emits the shared `projects.json` file. It must emit the existing ccomidi-compatible top-level shape: `root`, `bank`, and `slots`; each slot has `program`, `name`, `typeCode`, and optional `drumset`.
- [x] Add one future-facing editor Load interface in `voicegroup-core`, in `packages/voicegroup-core/src/plugin_load.rs`: input draft root/bank plus output `projects.json` path; validate project root, bank source syntax, macro/catalog structure, and loadable bank records; emit `projects.json` only on success; return `Ok(())` or one human-readable error string. The plugin commits the accepted draft root/bank after `Ok(())`.
- [x] Shape that interface so it can later return the file-path and organization handoff record that the audio engine consumes. The Rust plugin must not build sample-path organization or recursive handoff plans itself.
- [x] Keep this interface structural. It must not read sample bytes, decode samples, or prove that the audio engine can consume every sample path.
- [x] Add tests that prove a valid bank emits the ccomidi-compatible JSON shape, including `typeCode` and `drumset` metadata.
- [x] Add tests that prove validation failure leaves any existing `projects.json` untouched and returns one editor-facing error string.
- [x] Add a test that proves a structurally valid bank with a known sample symbol is accepted without checking the sample file bytes.
- [x] Verify:

```bash
(cd packages/voicegroup-core && cargo test)
```

### Task 4: Wire The Rust Plugin Load Transaction And General Tab

- [x] Add `packages/poryaaaa/plugin/src/config.rs` to load first-pass `poryaaaa.cfg` keys: `project_root`, `voicegroup`, `reverb`, `volume`. Tolerate but do not implement the old `log` key; plugin logging will be refactored later.
- [x] Remove the current editor probe path that calls `ProjectIndex::load()`/`load_program_bank()` through `packages/poryaaaa/plugin/src/voicegroup.rs`. If any helper remains, it must be renamed and routed through the Load transaction so the editor cannot call `voicegroup-core` before Load is clicked.
- [ ] If the remaining helper is only the shared `projects.json` default-path policy, rename `packages/poryaaaa/plugin/src/voicegroup.rs` to a path-specific module such as `shared_projects_json.rs`, update `mod` wiring in the same change, and do not leave `VoicegroupLoadStatus` in that path-only module.
- [x] Add one associated `PoryaaaaPlugin` Load function that takes draft root/bank and committed params, calls the single `voicegroup-core` Load interface, sets committed `#[persist]` root/bank only after success, and returns success/error status to the editor. Do not add a plugin-local transaction struct just for this function.
- [x] On NicePlug/CLAP initialize, if restored committed `#[persist]` project root and bank are non-empty, call the same voicegroup-core Load interface to validate and publish `projects.json` before processing starts.
- [x] Store draft project root and draft voicegroup/bank in `GuiState`, initialized from committed `#[persist]` root/bank when the editor opens. Text edits and Browse must not mutate committed params.
- [x] On Load failure, display the single `voicegroup-core` error string and leave committed root/bank unchanged. On Load success, committed root/bank have been set, `projects.json` has been emitted by `voicegroup-core`, and the editor path calls the NicePlug/CLAP restart request.
- [x] Do not add Voices, Recorder, C FFI, native C link changes, sample decoding, `voicegroup_load`, `LoadedVoiceGroup`, `ToneData`, or `m4a_engine_set_voicegroup()` in this Rust-only slice.
- [x] Verify:

```bash
(cd packages/poryaaaa/plugin && cargo test config && cargo test plugin_load && cargo test default_path_policy)
(cd packages/poryaaaa/plugin && cargo test editor)
(cd packages/poryaaaa/plugin && cargo test)
(cd packages/poryaaaa/plugin && cargo check)
```

Earlier config/probe progress verified before this revision: `cargo test config`, `cargo test voicegroup`, full `cargo test`, full `cargo build`, `_clap_entry`, `cmake --build packages/poryaaaa/build --target poryaaaa_unit_tests`, and `packages/poryaaaa/build/poryaaaa_unit_tests` after building `packages/voicegroup-core` release staticlib and reconfiguring CMake. The old probe path is now marked for replacement by the Load transaction above.

### Task 5: Later Add A Narrow C Engine FFI

Defer this task until the Rust-only Load path validates draft root/bank, emits `projects.json`, sets committed `#[persist]` root/bank, propagates validation errors to egui, and requests restart without touching C.

- [ ] Add `packages/poryaaaa/plugin/src/ffi.rs` with C declarations only for lifecycle, settings, MIDI, tempo, voicegroup handle, and render calls already exposed by `m4a_engine.h`.
- [ ] Mirror `M4AEngine` layout only in `ffi.rs`, with size/alignment checks. Keep `M4AEngine` effectively opaque from every normal Rust module.
- [ ] Add a small safe wrapper type that owns init/destroy and exposes first-pass methods: `reset`, `set_voicegroup`, `set_volume`, `set_reverb_amount`, `set_tempo_bpm`, `note_on`, `note_off`, `program_change`, `cc`, `pitch_bend`, `all_sound_off`, `process`.
- [ ] Implement `Plugin::initialize`/`Plugin::deactivate` so activate recreates owned engine state and deactivate tears it down like C `plugin_deactivate`/`m4a_engine_destroy`; `stop_processing` is not the engine lifetime boundary.
- [ ] Implement `Plugin::reset` as `m4a_engine_reset` plus program and voicegroup re-sync when an engine exists.
- [ ] Add the audio-engine path that receives the file-path handoff shape from `voicegroup-core` and then performs sample decoding, `LoadedVoiceGroup`/`ToneData` construction, and engine application.
- [ ] Do not expose `m4a_engine_driver()`, `m4a_engine_hw_audio()`, or voice-editing internals to Rust plugin code.
- [ ] When adding `build.rs` native linking, match CMake's single `voicegroup-core` staticlib path and link order instead of linking both `poryaaaa_engine`/`voicegroup_loader` and a Rust `voicegroup-core` rlib path that can duplicate `voicegroup_core` symbols.
- [ ] Verify:

```bash
(cd packages/poryaaaa/plugin && cargo test)
(cd packages/poryaaaa/plugin && cargo build)
(cd packages/poryaaaa/plugin && nm -u target/debug/libporyaaaa_clap_plugin.dylib)
cmake --build packages/poryaaaa/build --target poryaaaa_unit_tests
packages/poryaaaa/build/poryaaaa_unit_tests
```

### Task 6: Later Port The Process Adapter

- [ ] Add `packages/poryaaaa/plugin/src/process.rs` for NicePlug event translation and render chunking.
- [ ] Translate `NoteEvent::NoteOn`, `NoteEvent::NoteOff`, `NoteEvent::Choke`, `NoteEvent::MidiProgramChange`, `NoteEvent::MidiCC`, and `NoteEvent::MidiPitchBend` to existing C engine calls.
- [ ] Program changes are consumed only as engine input. Do not emit program-change events, do not mirror them into GUI state, and do not preserve the old C plugin's param echo behavior.
- [ ] Add `process.rs` tests proving incoming `MidiProgramChange` calls only the safe engine wrapper's `program_change`/`m4a_engine_program_change` path and does not update NicePlug program params or GUI-visible program state.
- [ ] Apply host tempo through `m4a_engine_set_tempo_bpm()` when NicePlug transport provides tempo. Recorder beat mapping is deferred.
- [ ] Preserve chunking to `M4A_ENGINE_MAX_PROCESS_FRAMES`.
- [ ] Preserve sample-accurate event/render interleaving from `m4a_plugin.c`: advance an outer `frame_pos`, apply all NicePlug events whose `NoteEvent::timing() <= frame_pos`, render only until the next event time through chunked `m4a_engine_process`, then advance.
- [ ] Add a pure-Rust process-order test with a mock engine that records `(timing, action)` and proves events and renders interleave, for example note at 0, render `N`, note at `N`; do not accept an implementation that drains every event before rendering the whole block.
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
