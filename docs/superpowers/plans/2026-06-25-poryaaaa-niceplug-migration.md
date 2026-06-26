# Poryaaaa NicePlug Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a new Rust NicePlug + egui CLAP plugin for poryaaaa while keeping the m4a audio engine in C. The existing C/C++ CLAP plugin remains available until the Rust plugin can scan and render audio.

**Architecture:** Rust owns the host-facing plugin: CLAP identity, lifecycle, params, new-state format, event translation, transport mapping, and GUI. C remains the audio-engine seam: `M4AEngine`, `m4a_driver`, `hw_audio`, voicegroup loading/materialization, and later recorder core stay behind explicit FFI until a later decision says otherwise.

**Tech Stack:** Rust nightly, NicePlug, nice-plug-egui, egui, C FFI, CMake for existing C engine/test targets, and an explicit native-link step for Rust-to-C engine objects.

---

## Assumptions

- The Rust plugin is a new plugin with a new CLAP name/id. DAW state and automation from the existing C/C++ plugin are intentionally incompatible.
- The migration target is the scaffold in `packages/poryaaaa/plugin/Cargo.toml` and `packages/poryaaaa/plugin/src/`.
- The C audio engine behavior is not rewritten as part of this plan.
- Rust mirrors the current C `M4AEngine` layout only inside `ffi.rs`; normal Rust modules use a safe wrapper and never rely on C layout details.
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
- Rust process code reads incoming NicePlug `NoteEvent`s, translates them to C `m4a_engine_*` calls, and chunks `m4a_engine_process()` to `M4A_ENGINE_MAX_PROCESS_FRAMES`.
- Incoming MIDI program-change messages are input-only: they call `m4a_engine_program_change()` so m4a channels choose voices. The Rust plugin does not need to emit PC changes, mirror them to GUI state, or preserve the old C plugin's host-visible param echo.
- Rust GUI first uses General-tab snapshots plus explicit commands. It must not pass a raw plugin-data pointer into UI code like `m4a_gui_set_plugin_data()` does today.

## Files

- Create: `packages/poryaaaa/plugin/src/plugin.rs`
- Create: `packages/poryaaaa/plugin/src/params.rs`
- Create: `packages/poryaaaa/plugin/src/ffi.rs`
- Create: `packages/poryaaaa/plugin/src/process.rs`
- Create: `packages/poryaaaa/plugin/src/state.rs`
- Create: `packages/poryaaaa/plugin/src/editor.rs` (initial editor shell exists; General-tab command/status wiring still pending)
- Create: `packages/poryaaaa/plugin/src/config.rs`
- Create: `packages/poryaaaa/plugin/src/voicegroup.rs`
- Modify: `packages/poryaaaa/plugin/src/lib.rs`
- Modify: `packages/poryaaaa/plugin/Cargo.toml`
- Later modify: `packages/poryaaaa/CMakeLists.txt`
- Later inspect/remove from plugin target: `packages/poryaaaa/plugin/m4a_plugin.c`, `packages/poryaaaa/plugin/m4a_params.c`, `packages/poryaaaa/plugin/m4a_gui.cpp`

## Tasks

### Task 1: Export A Rust CLAP Skeleton

- [x] Move the current `plugin_scaffold_version()` stub behind a real `PoryaaaaPlugin` type in `packages/poryaaaa/plugin/src/plugin.rs`.
- [x] Declare stereo output and MIDI input with NicePlug:

```rust
impl Plugin for PoryaaaaPlugin {
    const NAME: &'static str = "poryaaaa-rs";
    const VENDOR: &'static str = "sp3cker";
    const URL: &'static str = "";
    const EMAIL: &'static str = "";
    const VERSION: &'static str = env!("CARGO_PKG_VERSION");
    const AUDIO_IO_LAYOUTS: &'static [AudioIOLayout] = &[AudioIOLayout {
        main_output_channels: NonZeroU32::new(2),
        ..AudioIOLayout::const_default()
    }];
    const MIDI_INPUT: MidiConfig = MidiConfig::MidiCCs;
    type SysExMessage = ();
    type BackgroundTask = ();
}
```

- [x] Implement `ClapPlugin` with a new id/name, `com.sp3cker.poryaaaa-rs` / `poryaaaa-rs`, and features `Instrument`, `Synthesizer`, `Sampler`, `Stereo`.
- [x] Export only CLAP with `nice_export_clap!(PoryaaaaPlugin);`.
- [x] Verify:

```bash
cd packages/poryaaaa/plugin
cargo build
nm -g target/debug/libporyaaaa_clap_plugin.dylib | grep clap_entry
```

Progress verified: `cargo build` from `packages/poryaaaa/plugin`; `_clap_entry` present in `target/debug/libporyaaaa_clap_plugin.dylib`.

### Task 2: Add A Narrow C Engine FFI

- [ ] Add `packages/poryaaaa/plugin/src/ffi.rs` with C declarations only for lifecycle, settings, MIDI, tempo, voicegroup handle, and render calls already exposed by `m4a_engine.h`.
- [ ] Mirror `M4AEngine` layout only in `ffi.rs`, with size/alignment checks. Keep `M4AEngine` effectively opaque from every normal Rust module.
- [ ] Add a small safe wrapper type that owns init/destroy and exposes first-pass methods: `reset`, `set_voicegroup`, `set_volume`, `set_reverb_amount`, `set_tempo_bpm`, `note_on`, `note_off`, `program_change`, `cc`, `pitch_bend`, `all_sound_off`, `process`.
- [ ] Do not expose `m4a_engine_driver()`, `m4a_engine_hw_audio()`, or voice-editing internals to Rust plugin code.
- [ ] Verify:

```bash
(cd packages/poryaaaa/plugin && cargo test)
cmake --build packages/poryaaaa/build --target poryaaaa_unit_tests
packages/poryaaaa/build/poryaaaa_unit_tests
```

### Task 3: Port New Rust Parameters And State

- [x] Add `packages/poryaaaa/plugin/src/params.rs` with 16 stepped `IntParam`s for channel programs 0..127, defaults 0..15, using new stable NicePlug string ids.
- [ ] Persist first-pass non-automated plugin settings separately: project root, voicegroup name, volume, reverb, and egui window state. Recorder path/armed state comes later with the Recorder tab.
- [ ] Add `packages/poryaaaa/plugin/src/state.rs` for the new Rust plugin state format only. Do not parse or migrate `M4A_PLUGIN_STATE_VERSION == 2`; old C plugin state compatibility is an explicit break because the Rust plugin has a new CLAP identity.
- [ ] Add tests for default params, program range clamping, and new-state round trip. Default params and range shape are covered; state round trip remains pending.
- [ ] Verify:

```bash
(cd packages/poryaaaa/plugin && cargo test params)
(cd packages/poryaaaa/plugin && cargo test state)
(cd packages/poryaaaa/plugin && cargo check)
```

### Task 4: Port The Process Adapter

- [ ] Add `packages/poryaaaa/plugin/src/process.rs` for NicePlug event translation and render chunking.
- [ ] Translate `NoteEvent::NoteOn`, `NoteEvent::NoteOff`, `NoteEvent::Choke`, `NoteEvent::MidiProgramChange`, `NoteEvent::MidiCC`, and `NoteEvent::MidiPitchBend` to existing C engine calls.
- [ ] Program changes are consumed only as engine input. Do not emit program-change events, do not mirror them into GUI state, and do not preserve the old C plugin's param echo behavior.
- [ ] Apply host tempo through `m4a_engine_set_tempo_bpm()` when NicePlug transport provides tempo. Recorder beat mapping is deferred.
- [ ] Preserve chunking to `M4A_ENGINE_MAX_PROCESS_FRAMES`.
- [ ] Verify:

```bash
(cd packages/poryaaaa/plugin && cargo test process)
cmake --build packages/poryaaaa/build --target poryaaaa_unit_tests poryaaaa_engine_lifecycle
packages/poryaaaa/build/poryaaaa_unit_tests
packages/poryaaaa/build/poryaaaa_engine_lifecycle 1
```

### Task 5: Bridge Config And Voicegroup Loading

- [x] Add `packages/poryaaaa/plugin/src/config.rs` to load first-pass `poryaaaa.cfg` keys: `project_root`, `voicegroup`, `reverb`, `volume`. Tolerate but do not implement the old `log` key; plugin logging will be refactored later.
- [x] Add a Rust `voicegroup-core` probe in `packages/poryaaaa/plugin/src/voicegroup.rs` that loads `ProjectIndex`, calls `load_program_bank`, and returns typed diagnostics plus load status for the General tab.
- [ ] Add a C materialization bridge that calls `voicegroup_load`, stores the loaded voicegroup handle, applies it to the engine, and clears the engine's voicegroup reference before freeing the loaded voicegroup.
- [ ] Keep project-state publication plugin-specific, but do not port project asset indexes, sample overrides, or voice editing in this slice.
- [ ] Rebuild/reload voicegroup data only on project-root or voicegroup changes.
- [ ] Verify:

```bash
(cd packages/poryaaaa/plugin && cargo test config && cargo test voicegroup)
cmake --build packages/poryaaaa/build --target poryaaaa_unit_tests
packages/poryaaaa/build/poryaaaa_unit_tests
```

Progress verified: `cargo test config`, `cargo test voicegroup`, full `cargo test`, full `cargo build`, `_clap_entry`, `cmake --build packages/poryaaaa/build --target poryaaaa_unit_tests`, and `packages/poryaaaa/build/poryaaaa_unit_tests` after building `packages/voicegroup-core` release staticlib and reconfiguring CMake.

### Task 6: Port The egui General Tab

- [x] Add `packages/poryaaaa/plugin/src/editor.rs` using `create_egui_editor`.
- [ ] Implement only General tab first: project root field, voicegroup field, reload command, loaded/error status, volume, and reverb.
- [ ] Use explicit command messages for reload/config changes. Use `ParamSetter` only for values that are actually modeled as automatable params.
- [ ] Replace the current editor-local project root / voicegroup strings with the real snapshot/command seam before treating Task 6 as complete.
- [ ] Do not add Voices or Recorder tabs in this slice.
- [ ] Verify:

```bash
(cd packages/poryaaaa/plugin && cargo check)
(cd packages/poryaaaa/plugin && cargo test editor)
```

Manual verification after bundling exists: host opens editor, reloads a voicegroup, and saves/restores project root, voicegroup, volume, and reverb.

### Task 7: Later Port Voices And Recorder Tabs Without Raw Plugin Pointers

- [ ] Defer this task until the Rust plugin can scan, render audio, load a voicegroup, and open the General tab.
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

- [ ] Start this task only after the Rust plugin scans, loads a voicegroup, renders audio from incoming MIDI notes/program changes, and opens the General tab.
- [ ] Add a bundling path that builds the Rust `cdylib` and packages it as `poryaaaa-rs.clap` or another deliberate new bundle name.
- [ ] Reuse `packages/poryaaaa/plugin/Info.plist` only if its executable name, bundle id, display name, and version are updated for the Rust artifact.
- [ ] Preserve the existing C/C++ plugin target and install loop until the Rust bundle passes scan/load/render/editor checks.
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
- The existing CMake target auto-installs the C/C++ CLAP bundle. The Rust workflow must not replace that developer loop until Rust scan/load/render/editor checks pass.
- External MIDI realtime clock support and recorder parity are deferred. Do not fork or patch NicePlug for `0xF8`, `0xFA`, `0xFB`, or `0xFC` in the first rewrite.

## Stop Conditions

- Stop before deleting old C plugin files unless the Rust CLAP bundle scans, renders audio, opens the editor, loads voicegroups, and passes focused tests.
- Stop if Rust-to-C native linkage requires changing C engine semantics. That belongs in a separate engine-seam decision.
- Stop if a first-pass FFI addition would expose engine internals outside `ffi.rs` or the safe engine wrapper.
