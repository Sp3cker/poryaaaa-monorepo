# poryaaaa-rs Volume and Reverb Dials Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Volume and Reverb rotary controls to `poryaaaa-rs` that drive the C m4a engine the same way the C++ `poryaaaa.clap` GUI does.

**Architecture:** Treat Volume and Reverb as first-class nice-plug integer parameters, not editor-local state. The egui editor renders those parameters as rotary dials, and the plugin bridge applies their current 0..127 values to `M4aEngine::set_volume()` and `M4aEngine::set_reverb_amount()` whenever the host, state restore, config defaults, or the GUI changes them. This mirrors C++ `poryaaaa.clap`, where GUI edits set byte-sized `volume`/`reverbAmount` fields and immediately call `m4a_engine_set_volume()` / `m4a_engine_set_reverb_amount()` on the active engine.

**Tech Stack:** Rust 2021, nice-plug, nice-plug-egui, egui 0.34.1, existing C m4a engine FFI (`m4a_engine_set_volume`, `m4a_engine_set_reverb_amount`), Cargo tests under `packages/poryaaaa/plugin`.

## Global Constraints

- Package boundary: all implementation is inside `packages/poryaaaa/plugin`; no C++ `poryaaaa.clap` behavior change is required.
- Match C++ behavior exactly for values: Volume is integer `0..127`, default `127`; Reverb is integer `0..127`, default `0`.
- Preserve existing `poryaaaa.cfg` keys: `volume=<0..127>` and `reverb=<0..127>` remain startup defaults when no DAW state overrides them.
- Persist DAW state through nice-plug parameters, not a new custom state format.
- Do not add dependencies; implement the dial using egui primitives already available through `egui = 0.34.1`.
- Do not smooth these controls unless the C++ plugin does; C++ applies the raw byte immediately.
- Stable nice-plug parameter IDs must be short and never renamed after release. Use `vol` and `rev`.
- Verification for this feature must include `cargo test` from `packages/poryaaaa/plugin` and `just build poryaaaa-rs` from the repo root.

## Implementation Details Raised

1. **These should be host parameters, not only GUI state.** The Rust plugin already exposes per-channel programs as `IntParam`s. Volume and Reverb should follow that model so host automation, host generic UI, and plugin-state persistence all work. This is stronger than the current C++ GUI-only path while still driving the same C engine setters.
2. **Config defaults seed parameters once.** C++ stores config-loaded bytes in plugin data before activation. In Rust, load `poryaaaa.cfg` into `PluginConfig`, create `PoryaaaaParams` with those defaults, then let DAW state restore override the nice-plug params if present.
3. **Runtime application must happen after state restore and during processing.** Initial activation should read `params.audio_settings()` so restored DAW values win over defaults. Processing should cheaply compare current param values to the last values applied to the runtime and call the C setters only on change.
4. **There is no existing dial widget.** Implement a small rotary param widget in `editor.rs` using egui painting and `ParamSetter`; do not bring in a knob dependency.
5. **Reverb behavior is engine-side.** The plan only wires the existing `m4a_engine_set_reverb_amount()` path. It does not change how the engine computes reverb.

---

## File Structure

- `packages/poryaaaa/plugin/src/params.rs`
  - Owns the host-facing Volume and Reverb params, defaults, range constants, and a small `AudioSettings` value object.
- `packages/poryaaaa/plugin/src/plugin.rs`
  - Seeds params from `poryaaaa.cfg`, reads params for runtime initialization, reapplies audio settings on reset, and applies changed param values during process blocks.
- `packages/poryaaaa/plugin/src/editor.rs`
  - Renders two rotary dials bound to the new params through `ParamSetter`.
- `packages/poryaaaa/plugin/tests/skeleton.rs`
  - Extends public parameter shape coverage for defaults, ranges, and stable IDs where available through the nice-plug public interface.
- `packages/poryaaaa/plugin/src/plugin.rs` test module
  - Adds behavior coverage for config seeding and runtime audio-control application.

---

### Task 1: Promote Volume and Reverb to nice-plug params

**Files:**
- Modify: `packages/poryaaaa/plugin/src/params.rs`
- Test: `packages/poryaaaa/plugin/tests/skeleton.rs`

**Interfaces:**
- Consumes: existing `PoryaaaaParams`, `IntParam`, `IntRange`, `PROGRAM_COUNT`.
- Produces:
  - `pub const DEFAULT_VOLUME: u8 = 127`
  - `pub const DEFAULT_REVERB: u8 = 0`
  - `pub(crate) struct AudioSettings { pub volume: u8, pub reverb: u8 }`
  - `PoryaaaaParams::with_audio_defaults(volume: u8, reverb: u8) -> Self`
  - `PoryaaaaParams::audio_settings(&self) -> AudioSettings`
  - public fields `volume: IntParam` and `reverb: IntParam` with stable ids `vol` and `rev`

- [ ] **Step 1: Add the failing public parameter test**

Add this test to `packages/poryaaaa/plugin/tests/skeleton.rs` after `params_default_channel_programs_match_channel_numbers`:

```rust
#[test]
fn params_default_audio_controls_match_cpp_plugin() {
    let params = PoryaaaaParams::default();

    assert_eq!(params.volume.value(), 127);
    assert!(matches!(
        params.volume.range(),
        IntRange::Linear { min: 0, max: 127 }
    ));

    assert_eq!(params.reverb.value(), 0);
    assert!(matches!(
        params.reverb.range(),
        IntRange::Linear { min: 0, max: 127 }
    ));
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run from `packages/poryaaaa/plugin`:

```bash
cargo test params_default_audio_controls_match_cpp_plugin --test skeleton
```

Expected: compile failure mentioning missing fields `volume` and `reverb` on `PoryaaaaParams`.

- [ ] **Step 3: Add constants, `AudioSettings`, and param fields**

In `packages/poryaaaa/plugin/src/params.rs`, replace the top constants with:

```rust
pub const PROGRAM_COUNT: usize = 16;
pub const DEFAULT_VOLUME: u8 = 127;
pub const DEFAULT_REVERB: u8 = 0;
pub(crate) const DEFAULT_EDITOR_WIDTH: u32 = 525;
pub(crate) const DEFAULT_EDITOR_HEIGHT: u32 = 325;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) struct AudioSettings {
    pub volume: u8,
    pub reverb: u8,
}
```

Add the new params after `midi_activity` and before `program_00` in `PoryaaaaParams`:

```rust
    #[id = "vol"]
    pub volume: IntParam,
    #[id = "rev"]
    pub reverb: IntParam,
```

- [ ] **Step 4: Replace `Default` construction with audio-aware construction**

Replace `impl Default for PoryaaaaParams` with:

```rust
impl Default for PoryaaaaParams {
    fn default() -> Self {
        Self::with_audio_defaults(DEFAULT_VOLUME, DEFAULT_REVERB)
    }
}
```

Add this constructor at the top of `impl PoryaaaaParams`:

```rust
    /// Builds params with config-seeded audio defaults before host state restore can override them.
    pub(crate) fn with_audio_defaults(volume: u8, reverb: u8) -> Self {
        Self {
            editor_state: EguiState::from_size(DEFAULT_EDITOR_WIDTH, DEFAULT_EDITOR_HEIGHT),
            project_root: Arc::new(RwLock::new(String::new())),
            voicegroup: Arc::new(RwLock::new(String::new())),
            runtime_voicegroup_status: Arc::new(RwLock::new(None)),
            host_restart_pending: Arc::new(AtomicBool::new(false)),
            midi_activity: Arc::new(MidiActivity::default()),
            volume: audio_control_param("Volume", volume),
            reverb: audio_control_param("Reverb", reverb),
            program_00: channel_program_param(0),
            program_01: channel_program_param(1),
            program_02: channel_program_param(2),
            program_03: channel_program_param(3),
            program_04: channel_program_param(4),
            program_05: channel_program_param(5),
            program_06: channel_program_param(6),
            program_07: channel_program_param(7),
            program_08: channel_program_param(8),
            program_09: channel_program_param(9),
            program_10: channel_program_param(10),
            program_11: channel_program_param(11),
            program_12: channel_program_param(12),
            program_13: channel_program_param(13),
            program_14: channel_program_param(14),
            program_15: channel_program_param(15),
        }
    }

    /// Reads the host-facing global audio settings as m4a byte values.
    pub(crate) fn audio_settings(&self) -> AudioSettings {
        AudioSettings {
            volume: self.volume.value().clamp(0, 127) as u8,
            reverb: self.reverb.value().clamp(0, 127) as u8,
        }
    }
```

Add this helper near `channel_program_param`:

```rust
/// Builds one automatable global audio-control parameter with m4a's 0..127 range.
fn audio_control_param(name: &'static str, default: u8) -> IntParam {
    IntParam::new(
        name,
        default.min(127) as i32,
        IntRange::Linear { min: 0, max: 127 },
    )
}
```

- [ ] **Step 5: Run the parameter test to verify it passes**

Run from `packages/poryaaaa/plugin`:

```bash
cargo test params_default_audio_controls_match_cpp_plugin --test skeleton
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add packages/poryaaaa/plugin/src/params.rs packages/poryaaaa/plugin/tests/skeleton.rs
git commit -m "feat(poryaaaa-rs): expose volume and reverb params"
```

---

### Task 2: Apply audio params to the C runtime like C++ poryaaaa.clap

**Files:**
- Modify: `packages/poryaaaa/plugin/src/plugin.rs`
- Test: `packages/poryaaaa/plugin/src/plugin.rs`

**Interfaces:**
- Consumes: `PoryaaaaParams::with_audio_defaults`, `PoryaaaaParams::audio_settings`, `AudioSettings`, existing `M4aEngine::set_volume`, existing `M4aEngine::set_reverb_amount`.
- Produces:
  - `PoryaaaaPlugin` no longer needs to store `PluginConfig` after construction.
  - `PoryaaaaPlugin::apply_audio_settings_to_runtime(&mut self, force: bool)` applies changed settings to the active runtime.
  - Activation, reset, and processing all converge on the same audio-control application path.

- [ ] **Step 1: Add failing tests for config seeding and runtime application**

In the `#[cfg(test)] mod tests` block in `packages/poryaaaa/plugin/src/plugin.rs`, add these tests after `remember_host_tempo_keeps_only_finite_positive_values`:

```rust
    #[test]
    fn audio_param_config_seeds_volume_and_reverb_params() {
        let config_dir = temp_project("audio-param-config-dir");
        write_file(
            &config_dir,
            "poryaaaa.cfg",
            "\
volume=64
reverb=23
",
        );
        let _env_lock = TEST_ENV_LOCK.lock().expect("test env lock");
        crate::config::set_default_config_dir_for_test(Some(config_dir.clone()));

        let plugin = PoryaaaaPlugin::default();

        assert_eq!(plugin.params_for_test().volume.value(), 64);
        assert_eq!(plugin.params_for_test().reverb.value(), 23);

        crate::config::set_default_config_dir_for_test(None);
        fs::remove_dir_all(config_dir).expect("remove temp config dir");
    }

    #[test]
    fn audio_param_changes_are_applied_to_active_runtime() {
        let root = temp_project("audio-param-runtime-project");
        write_file(
            &root,
            "sound/voice_groups.inc",
            "\
                \t.align 2
                voicegroup000::
                \tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3
            ",
        );
        let mut plugin = PoryaaaaPlugin::default();
        plugin
            .params_for_test()
            .commit_voicegroup_selection(&root.to_string_lossy(), "voicegroup000");
        let mut init_context = TestInitContext;
        assert!(plugin.initialize(
            &PoryaaaaPlugin::AUDIO_IO_LAYOUTS[0],
            &test_buffer_config(),
            &mut init_context,
        ));
        assert!(plugin.runtime_has_loaded_voicegroup());

        let mut context = TestProcessContext::with_events(vec![NoteEvent::NoteOn {
            timing: 0,
            voice_id: None,
            channel: 0,
            note: 60,
            velocity: 1.0,
        }]);
        let mut audible_peak = 0.0f32;
        for _ in 0..8 {
            let mut output = vec![vec![0.0; 512], vec![0.0; 512]];
            assert_eq!(
                process_with_output(&mut plugin, &mut output, &mut context),
                ProcessStatus::KeepAlive,
            );
            audible_peak = output
                .iter()
                .flatten()
                .fold(audible_peak, |peak, sample| peak.max(sample.abs()));
        }
        assert!(audible_peak > 0.0001);

        unsafe {
            use nice_plug::params::InternalParamMut;
            plugin.params_for_test().volume._internal_set_plain_value(0);
            plugin.params_for_test().reverb._internal_set_plain_value(0);
        }

        let mut muted_peak = 0.0f32;
        for _ in 0..4 {
            let mut output = vec![vec![0.0; 512], vec![0.0; 512]];
            assert_eq!(
                process_with_output(&mut plugin, &mut output, &mut context),
                ProcessStatus::KeepAlive,
            );
            muted_peak = output
                .iter()
                .flatten()
                .fold(muted_peak, |peak, sample| peak.max(sample.abs()));
        }
        assert!(muted_peak < audible_peak * 0.10);

        plugin.deactivate();
        fs::remove_dir_all(root).expect("remove temp project");
    }
```

If `nice_plug::params::InternalParamMut` is not exported at that path, use `nice_plug_core::params::InternalParamMut` only if the crate is already available through existing dependencies. Do not add a dependency solely for tests; instead add this private test helper to `impl PoryaaaaPlugin`:

```rust
    #[cfg(test)]
    pub(crate) fn set_audio_params_for_test(&self, volume: i32, reverb: i32) {
        unsafe {
            use nice_plug::params::InternalParamMut;
            self.params.volume._internal_set_plain_value(volume);
            self.params.reverb._internal_set_plain_value(reverb);
        }
    }
```

Then call `plugin.set_audio_params_for_test(0, 0)` in the test.

- [ ] **Step 2: Run tests to verify they fail**

Run from `packages/poryaaaa/plugin`:

```bash
cargo test audio_param --lib
```

Expected: compile failure or assertion failure because config values are still stored in `PluginConfig`, not in params, and process does not apply changed params to the runtime.

- [ ] **Step 3: Update plugin construction to seed params from config**

In `packages/poryaaaa/plugin/src/plugin.rs`, remove `PluginConfig` from the stored fields.

Change the imports from:

```rust
    config::PluginConfig,
```

to no `PluginConfig` import.

Change `PoryaaaaPlugin` from:

```rust
pub struct PoryaaaaPlugin {
    params: Arc<PoryaaaaParams>,
    runtime: Arc<Mutex<Option<M4aEngine>>>,
    last_host_tempo_bpm: Option<f64>,
    config: PluginConfig,
}
```

to:

```rust
pub struct PoryaaaaPlugin {
    params: Arc<PoryaaaaParams>,
    runtime: Arc<Mutex<Option<M4aEngine>>>,
    last_host_tempo_bpm: Option<f64>,
    last_applied_audio_settings: Option<crate::params::AudioSettings>,
}
```

Change `Default` to:

```rust
impl Default for PoryaaaaPlugin {
    fn default() -> Self {
        let config = crate::config::load_default_config();
        let params = PoryaaaaParams::with_audio_defaults(config.volume, config.reverb);
        Self::apply_config_defaults_to_params(&params, &config);
        Self {
            params: Arc::new(params),
            runtime: Arc::new(Mutex::new(None)),
            last_host_tempo_bpm: None,
            last_applied_audio_settings: None,
        }
    }
}
```

Keep `PluginConfig` referenced in the method signature by using its full path:

```rust
    fn apply_config_defaults_to_params(params: &PoryaaaaParams, config: &crate::config::PluginConfig) {
```

- [ ] **Step 4: Read audio settings from params during initialization**

In `initialize`, replace:

```rust
        let config = EngineConfig {
            sample_rate: buffer_config.sample_rate,
            volume: self.config.volume,
            reverb: self.config.reverb,
        };
```

with:

```rust
        let audio_settings = self.params.audio_settings();
        let config = EngineConfig {
            sample_rate: buffer_config.sample_rate,
            volume: audio_settings.volume,
            reverb: audio_settings.reverb,
        };
        self.last_applied_audio_settings = Some(audio_settings);
```

After storing `runtime`, keep the existing call to `self.reapply_host_state_to_runtime();`; Task 2 Step 5 updates that method to force the same settings into the runtime after voicegroup/program replay.

- [ ] **Step 5: Apply audio settings during reapply and process**

Add this method to `impl PoryaaaaPlugin` near `reapply_host_state_to_runtime`:

```rust
    /// Pushes changed global audio controls into the active C runtime.
    fn apply_audio_settings_to_runtime(&mut self, force: bool) {
        let settings = self.params.audio_settings();
        if !force && self.last_applied_audio_settings == Some(settings) {
            return;
        }

        let mut runtime = self.runtime.lock().expect("runtime lock");
        let Some(runtime) = runtime.as_mut() else {
            self.last_applied_audio_settings = None;
            return;
        };

        runtime.set_volume(settings.volume);
        runtime.set_reverb_amount(settings.reverb);
        self.last_applied_audio_settings = Some(settings);
    }
```

Change `reapply_host_state_to_runtime` so it applies audio settings before programs:

```rust
    /// Reapplies restored/default host state after engine reinit/reset.
    fn reapply_host_state_to_runtime(&mut self) {
        self.apply_audio_settings_to_runtime(true);
        let programs = read_program_params(self.params.as_ref());
        let mut runtime = self.runtime.lock().expect("runtime lock");
        let Some(runtime) = runtime.as_mut() else {
            return;
        };
        for (channel, &program) in programs.iter().enumerate() {
            runtime.program_change(channel as i32, program);
        }
        if let Some(bpm) = self.last_host_tempo_bpm {
            runtime.set_tempo_bpm(bpm);
        }
    }
```

In `process`, call the method just before `process::process_stereo(...)`:

```rust
        self.apply_audio_settings_to_runtime(false);
        let mut runtime = self.runtime.lock().expect("runtime lock");
        let Some(runtime) = runtime.as_mut() else {
            process::clear_stereo(left, right);
            process::drain_midi_activity(self.params.midi_activity.as_ref(), || {
                context.next_event()
            });
            return ProcessStatus::Normal;
        };
```

Because this changes lock ordering, implement it carefully: do not keep the existing `runtime` mutex guard alive while calling `apply_audio_settings_to_runtime()`. The final `process` shape should be:

```rust
        self.apply_audio_settings_to_runtime(false);
        let mut runtime = self.runtime.lock().expect("runtime lock");
        let Some(runtime) = runtime.as_mut() else { ... };
        if !runtime.is_ready() { ... }
        process::process_stereo(...);
```

- [ ] **Step 6: Reset audio-settings cache on deactivation**

Change `deactivate` to:

```rust
    fn deactivate(&mut self) {
        *self.runtime.lock().expect("runtime lock") = None;
        self.last_applied_audio_settings = None;
    }
```

- [ ] **Step 7: Run focused tests**

Run from `packages/poryaaaa/plugin`:

```bash
cargo test audio_param --lib
```

Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add packages/poryaaaa/plugin/src/plugin.rs
git commit -m "feat(poryaaaa-rs): apply volume and reverb params"
```

---

### Task 3: Render Volume and Reverb as egui dials

**Files:**
- Modify: `packages/poryaaaa/plugin/src/editor.rs`
- Test: `packages/poryaaaa/plugin/src/editor.rs` if a pure helper is added; otherwise compile verification in Task 4 covers the widget.

**Interfaces:**
- Consumes: `PoryaaaaParams.volume`, `PoryaaaaParams.reverb`, `ParamSetter`, `IntParam`.
- Produces:
  - A small local dial widget that calls `ParamSetter::begin_set_parameter`, `set_parameter`, and `end_set_parameter` so the host observes automation gestures.
  - A UI section under MIDI activity and above project-root selection with two dials labelled `Volume` and `Reverb`.

- [ ] **Step 1: Add dial layout constants**

Near the existing editor constants in `packages/poryaaaa/plugin/src/editor.rs`, add:

```rust
const AUDIO_DIAL_SIZE: f32 = 64.0;
const AUDIO_DIAL_VALUE_STEP: f32 = 127.0 / 180.0;
```

- [ ] **Step 2: Add the dial widget helper**

Add this helper after `show_activity_light`:

```rust
/// Shows one automatable 0..127 integer parameter as a rotary dial.
fn show_audio_dial(ui: &mut egui::Ui, setter: &ParamSetter, param: &IntParam) {
    ui.vertical_centered(|ui| {
        ui.label(param.name());
        let (rect, mut response) = ui.allocate_exact_size(
            Vec2::splat(AUDIO_DIAL_SIZE),
            egui::Sense::click_and_drag(),
        );

        if response.is_pointer_button_down_on() {
            setter.begin_set_parameter(param);
            let delta = response.drag_delta().y * -AUDIO_DIAL_VALUE_STEP;
            if delta.abs() >= 1.0 {
                let next = (param.value() as f32 + delta).round().clamp(0.0, 127.0) as i32;
                setter.set_parameter(param, next);
                response.mark_changed();
            }
        }
        if response.double_clicked() {
            setter.set_parameter(param, param.default_plain_value());
            response.mark_changed();
        }
        if response.drag_stopped() {
            setter.end_set_parameter(param);
        }

        let painter = ui.painter_at(rect);
        let center = rect.center();
        let radius = rect.width().min(rect.height()) * 0.42;
        let normalized = param.modulated_normalized_value().clamp(0.0, 1.0);
        let start_angle = std::f32::consts::TAU * 0.625;
        let end_angle = std::f32::consts::TAU * 0.875 + std::f32::consts::TAU;
        let angle = start_angle + normalized * (end_angle - start_angle);
        let stroke = egui::Stroke::new(2.0, ui.visuals().widgets.inactive.fg_stroke.color);
        let active_stroke = egui::Stroke::new(3.0, ui.visuals().selection.stroke.color);

        painter.circle_stroke(center, radius, stroke);
        painter.line_segment(
            [
                center,
                center + egui::vec2(angle.cos(), angle.sin()) * (radius * 0.78),
            ],
            active_stroke,
        );
        painter.circle_filled(center, 3.0, ui.visuals().selection.stroke.color);

        ui.label(param.to_string());
    });
}
```

If `Response::is_pointer_button_down_on()` begins a new automation gesture on every repaint while held, replace the direct calls with a tiny `egui::Id`-backed memory flag modelled after `nice_plug_egui::widgets::param_slider`. Keep that state local to `show_audio_dial`; do not add fields to `GuiState` for drag mechanics.

- [ ] **Step 3: Add the audio controls section to the editor**

Inside the editor closure, after the MIDI activity separator and before `ui.label("Project root");`, insert:

```rust
                    ui.horizontal(|ui| {
                        show_audio_dial(ui, setter, &params.volume);
                        ui.add_space(16.0);
                        show_audio_dial(ui, setter, &params.reverb);
                    });
                    ui.separator();
```

The surrounding section should become:

```rust
                    show_midi_activity(ui, gui_state, params.midi_activity.snapshot());
                    ui.separator();

                    ui.horizontal(|ui| {
                        show_audio_dial(ui, setter, &params.volume);
                        ui.add_space(16.0);
                        show_audio_dial(ui, setter, &params.reverb);
                    });
                    ui.separator();

                    ui.label("Project root");
```

- [ ] **Step 4: Compile-check editor code**

Run from `packages/poryaaaa/plugin`:

```bash
cargo test --test skeleton
```

Expected: PASS. This catches missing imports, method names, and type errors in the editor module.

- [ ] **Step 5: Commit**

```bash
git add packages/poryaaaa/plugin/src/editor.rs
git commit -m "feat(poryaaaa-rs): add volume and reverb dials"
```

---

### Task 4: Final verification and manual host smoke test

**Files:**
- Modify only files already touched if verification exposes a bug.

**Interfaces:**
- Consumes: Tasks 1-3 completed.
- Produces: Verified `poryaaaa-rs.clap` bundle installed to the user CLAP directory by the existing `just build poryaaaa-rs` recipe.

- [ ] **Step 1: Run all Rust plugin tests**

Run from `packages/poryaaaa/plugin`:

```bash
cargo test
```

Expected: all tests pass.

- [ ] **Step 2: Build and install the Rust CLAP bundle**

Run from repo root:

```bash
just build poryaaaa-rs
```

Expected: command succeeds and writes:

```text
$HOME/Library/Audio/Plug-Ins/CLAP/poryaaaa-rs.clap/Contents/MacOS/poryaaaa-rs
$HOME/Library/Audio/Plug-Ins/CLAP/poryaaaa-rs.clap/Contents/Info.plist
```

- [ ] **Step 3: Confirm installed bundle files exist**

Run from repo root:

```bash
test -x "$HOME/Library/Audio/Plug-Ins/CLAP/poryaaaa-rs.clap/Contents/MacOS/poryaaaa-rs"
test -f "$HOME/Library/Audio/Plug-Ins/CLAP/poryaaaa-rs.clap/Contents/Info.plist"
```

Expected: both commands exit 0.

- [ ] **Step 4: Manual DAW smoke test**

Open a CLAP host that can load `poryaaaa-rs`, then verify:

1. The editor shows two rotary dials labelled `Volume` and `Reverb`.
2. Loading the same project root and voicegroup as before still works.
3. While a note is sounding, turning `Volume` down reduces the output and `Volume = 0` mutes or nearly mutes it.
4. Turning `Volume` back to `127` restores the output.
5. Turning `Reverb` changes the engine reverb amount without requiring a voicegroup reload or plugin restart.
6. Closing and reopening the plugin preserves the two control values through host state.
7. The host generic parameter UI lists Volume and Reverb as automatable params.

- [ ] **Step 5: Commit any verification fixes**

If verification required fixes, commit only those touched files:

```bash
git add packages/poryaaaa/plugin/src/params.rs packages/poryaaaa/plugin/src/plugin.rs packages/poryaaaa/plugin/src/editor.rs packages/poryaaaa/plugin/tests/skeleton.rs
git commit -m "fix(poryaaaa-rs): stabilize volume reverb controls"
```

If no fixes were needed, do not create an empty commit.

---

## Self-Review

- **Spec coverage:** The plan covers the requested Volume and Reverb dials, matches the C++ value ranges/defaults, drives the same C engine setters, preserves `poryaaaa.cfg`, handles host automation/state, and includes build/test/manual verification.
- **Placeholder scan:** No placeholder tasks are left. Every task has concrete files, code snippets, commands, and expected results.
- **Type consistency:** The plan consistently uses `AudioSettings { volume: u8, reverb: u8 }`, `PoryaaaaParams::with_audio_defaults`, `PoryaaaaParams::audio_settings`, and stable param IDs `vol`/`rev`.
- **Scope check:** This is one package-local feature in `packages/poryaaaa/plugin`; no cross-package plan is needed.
