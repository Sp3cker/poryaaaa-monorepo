# M4aEngine Hybrid Facade Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the CPluginRuntime wrapper with a clean, safe, and deep `M4aEngine` facade, enforcing drop ordering invariants and programmatic error handling while preserving the existing event-interleaving rendering seam.

**Architecture:** `M4aEngine` wraps the unsafe C FFI. It implements `ProcessRuntime` so that the event-interleaving loop `process_stereo` in `process.rs` remains unchanged. It encapsulates safety invariants like unbind-before-free inside its custom `Drop` logic and uses typed `EngineError` variants.

**Tech Stack:** Rust nightly, NicePlug, C FFI.

## Global Constraints
- Target package: `packages/poryaaaa/plugin`
- Do not modify C-side engine files or engine FFI signatures (`packages/poryaaaa/plugin/src/ffi.rs` is read-only for this plan).
- Enforce the unbind-before-free drop order: `m4a_engine_set_voicegroup` must be called with a null pointer before freeing the engine or voicegroup.
- Return programmatic `EngineError` variants instead of unstructured strings.

---

## File Structure

- Modify: `packages/poryaaaa/plugin/src/runtime.rs`
  - Defines `EngineError`, `EngineConfig`, and `M4aEngine`.
  - Implements the safe facade methods, `ProcessRuntime` trait, and custom `Drop`.
- Modify: `packages/poryaaaa/plugin/src/plugin.rs`
  - Switches `CPluginRuntime` with `M4aEngine`.
  - Simplifies activation/reset/deactivation/processing logic.
- Modify: `packages/poryaaaa/plugin/src/lib.rs`
  - Updates integration tests to verify the new facade behaves correctly.

---

## Verifiable End-Goals
- **EG1:** The `poryaaaa-plugin` package compiles without errors.
- **EG2:** `cargo test -p poryaaaa-plugin` passes successfully, executing all tests in `runtime.rs` and `lib.rs`.
- **EG3:** Implicit drop-order reliance is eliminated; tests verify that the engine is safely unbound when dropped or when a reset fails.

---

### Task 1: Implement the M4aEngine Facade and Safety Invariants

**Files:**
- Modify: `packages/poryaaaa/plugin/src/runtime.rs`

**Interfaces:**
- Consumes: Raw FFI functions in `packages/poryaaaa/plugin/src/ffi.rs`.
- Produces:
  ```rust
  #[derive(Debug, PartialEq, Eq)]
  pub enum EngineError {
      EngineCreateFailed { sample_rate: f32 },
      InvalidCString { field: &'static str },
      VoicegroupLoadFailed(String),
      VoicegroupHasNoVoices,
      ResetFailed,
  }

  #[derive(Clone, Copy, PartialEq)]
  pub struct EngineConfig {
      pub sample_rate: f32,
      pub volume: u8,
      pub reverb: u8,
  }

  pub struct M4aEngine {
      engine: Option<EngineHandle>,
      voicegroup: Option<LoadedVoiceGroupHandle>,
  }

  impl M4aEngine {
      pub fn new(config: EngineConfig) -> Result<Self, EngineError>;
      pub fn load_voicegroup(&mut self, project_root: &str, bank: &str) -> Result<(), EngineError>;
      pub fn clear_voicegroup(&mut self);
      pub fn reset(&mut self) -> Result<(), EngineError>;
      pub fn reconfigure(&mut self, config: EngineConfig) -> Result<(), EngineError>;
      pub fn is_ready(&self) -> bool;
      pub fn set_volume(&mut self, volume: u8);
      pub fn set_reverb_amount(&mut self, amount: u8);
      pub fn all_notes_off(&mut self, track: i32);
      pub fn all_sound_off(&mut self);
  }

  impl ProcessRuntime for M4aEngine { ... }
  ```

- [ ] **Step 1: Write the failing tests inside `runtime.rs`**

Add tests to verify:
- Failed reset drops handles and returns `Err(EngineError::ResetFailed)`.
- Reconfiguring with the same sample rate performs a soft reset, and with a different sample rate performs a complete recreation.
- Drop order: verify we can instantiate, load, and drop without panicking.

Replace the `mod tests` block at the end of `packages/poryaaaa/plugin/src/runtime.rs`:

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_engine_reconfigure_and_reset() {
        let config = EngineConfig {
            sample_rate: 44100.0,
            volume: 100,
            reverb: 10,
        };
        let mut engine = M4aEngine::new(config).expect("create engine");
        assert!(engine.is_ready() == false);

        // Reset should succeed when engine exists
        assert!(engine.reset().is_ok());

        // Reconfiguring to the same sample rate does not recreate engine
        assert!(engine.reconfigure(config).is_ok());

        // Reconfiguring to a different sample rate works
        let new_config = EngineConfig {
            sample_rate: 48000.0,
            volume: 120,
            reverb: 20,
        };
        assert!(engine.reconfigure(new_config).is_ok());
    }

    #[test]
    fn test_failed_reset_retires_engine() {
        let config = EngineConfig {
            sample_rate: 44100.0,
            volume: 100,
            reverb: 10,
        };
        let mut engine = M4aEngine::new(config).expect("create engine");
        engine.engine = None; // simulate failure or force retirement
        assert!(engine.reset().is_err());
    }
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cargo test -p poryaaaa-plugin --lib runtime::tests`
Expected: Compile failure (types do not exist yet).

- [ ] **Step 3: Write minimal implementation in `runtime.rs`**

Rewrite `packages/poryaaaa/plugin/src/runtime.rs` to implement the `M4aEngine` facade, including:
1. `EngineError` and `EngineConfig`.
2. `EngineHandle` and `LoadedVoiceGroupHandle` wrapping raw pointers.
3. `M4aEngine` with the methods and `ProcessRuntime` implementation.
4. Custom `Drop` implementation on `M4aEngine` that calls `unbind_voicegroup_ptr(None)` to break the borrow before handles are freed.

```rust
use crate::ffi;
use crate::process::ProcessRuntime;
use std::ffi::{CStr, CString};
use std::ptr::NonNull;

pub(crate) struct EngineHandle {
    ptr: NonNull<ffi::M4AEngine>,
}

impl EngineHandle {
    fn new(sample_rate: f32) -> Result<Self, EngineError> {
        let ptr = unsafe { ffi::m4a_engine_create(sample_rate) };
        let ptr = NonNull::new(ptr).ok_or(EngineError::EngineCreateFailed { sample_rate })?;
        Ok(Self { ptr })
    }

    fn as_ptr(&mut self) -> *mut ffi::M4AEngine {
        self.ptr.as_ptr()
    }
}

impl Drop for EngineHandle {
    fn drop(&mut self) {
        unsafe {
            ffi::m4a_engine_free(self.ptr.as_ptr());
        }
    }
}

pub(crate) struct LoadedVoiceGroupHandle {
    ptr: NonNull<ffi::LoadedVoiceGroup>,
}

impl LoadedVoiceGroupHandle {
    fn load(project_root: &str, bank: &str) -> Result<Self, EngineError> {
        let project_root_c = CString::new(project_root)
            .map_err(|_| EngineError::InvalidCString { field: "project_root" })?;
        let bank_c = CString::new(bank)
            .map_err(|_| EngineError::InvalidCString { field: "bank" })?;
        let ptr = unsafe { ffi::voicegroup_load(project_root_c.as_ptr(), bank_c.as_ptr()) };
        let ptr = NonNull::new(ptr).ok_or_else(|| {
            EngineError::VoicegroupLoadFailed(last_voicegroup_error())
        })?;
        Ok(Self { ptr })
    }

    fn voices(&mut self) -> Option<NonNull<ffi::ToneData>> {
        NonNull::new(unsafe { ffi::voicegroup_loaded_voices(self.ptr.as_ptr()) })
    }
}

impl Drop for LoadedVoiceGroupHandle {
    fn drop(&mut self) {
        unsafe {
            ffi::voicegroup_free(self.ptr.as_ptr());
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum EngineError {
    EngineCreateFailed { sample_rate: f32 },
    InvalidCString { field: &'static str },
    VoicegroupLoadFailed(String),
    VoicegroupHasNoVoices,
    ResetFailed,
}

impl std::fmt::Display for EngineError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::EngineCreateFailed { sample_rate } => write!(f, "Failed to create engine at sample rate {sample_rate}"),
            Self::InvalidCString { field } => write!(f, "Field '{field}' contains interior NUL byte"),
            Self::VoicegroupLoadFailed(msg) => write!(f, "Voicegroup load failed: {msg}"),
            Self::VoicegroupHasNoVoices => write!(f, "Loaded voicegroup has no voices"),
            Self::ResetFailed => write!(f, "Engine reset failed"),
        }
    }
}

impl std::error::Error for EngineError {}

#[derive(Clone, Copy, PartialEq)]
pub struct EngineConfig {
    pub sample_rate: f32,
    pub volume: u8,
    pub reverb: u8,
}

pub struct M4aEngine {
    pub(crate) engine: Option<EngineHandle>,
    pub(crate) voicegroup: Option<LoadedVoiceGroupHandle>,
    last_applied_rate: f32,
}

unsafe impl Send for M4aEngine {}

impl M4aEngine {
    pub fn new(config: EngineConfig) -> Result<Self, EngineError> {
        let engine = EngineHandle::new(config.sample_rate)?;
        let mut this = Self {
            engine: Some(engine),
            voicegroup: None,
            last_applied_rate: config.sample_rate,
        };
        this.set_volume(config.volume);
        this.set_reverb_amount(config.reverb);
        Ok(this)
    }

    pub fn load_voicegroup(&mut self, project_root: &str, bank: &str) -> Result<(), EngineError> {
        let mut loaded = LoadedVoiceGroupHandle::load(project_root, bank)?;
        let voices = loaded
            .voices()
            .ok_or(EngineError::VoicegroupHasNoVoices)?;
        self.bind_voicegroup_ptr(Some(voices));
        self.voicegroup = Some(loaded);
        Ok(())
    }

    pub fn clear_voicegroup(&mut self) {
        self.bind_voicegroup_ptr(None);
        self.voicegroup = None;
    }

    pub fn reset(&mut self) -> Result<(), EngineError> {
        let Some(engine) = self.engine.as_mut() else {
            return Err(EngineError::ResetFailed);
        };
        let ok = unsafe { ffi::m4a_engine_reset(engine.as_ptr()) };
        if ok {
            self.rebind_loaded_voicegroup();
            Ok(())
        } else {
            self.retire_after_failed_reset();
            Err(EngineError::ResetFailed)
        }
    }

    pub fn reconfigure(&mut self, config: EngineConfig) -> Result<(), EngineError> {
        if self.engine.is_none() || (config.sample_rate - self.last_applied_rate).abs() > 0.001 {
            // Need recreation
            self.clear_voicegroup();
            self.engine = None;
            let engine = EngineHandle::new(config.sample_rate)?;
            self.engine = Some(engine);
            self.last_applied_rate = config.sample_rate;
        } else {
            // Soft reset
            self.reset()?;
        }
        self.set_volume(config.volume);
        self.set_reverb_amount(config.reverb);
        Ok(())
    }

    pub fn is_ready(&self) -> bool {
        self.voicegroup.is_some() && self.engine.is_some()
    }

    pub fn set_volume(&mut self, volume: u8) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_set_volume(engine.as_ptr(), volume) }
        }
    }

    pub fn set_reverb_amount(&mut self, amount: u8) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_set_reverb_amount(engine.as_ptr(), amount) }
        }
    }

    pub fn all_notes_off(&mut self, track: i32) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_all_notes_off(engine.as_ptr(), track) }
        }
    }

    pub fn all_sound_off(&mut self) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_all_sound_off(engine.as_ptr()) }
        }
    }

    fn retire_after_failed_reset(&mut self) {
        self.clear_voicegroup();
        self.engine = None;
    }

    fn rebind_loaded_voicegroup(&mut self) {
        let voices = self
            .voicegroup
            .as_mut()
            .and_then(LoadedVoiceGroupHandle::voices);
        self.bind_voicegroup_ptr(voices);
    }

    fn bind_voicegroup_ptr(&mut self, voices: Option<NonNull<ffi::ToneData>>) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe {
                ffi::m4a_engine_set_voicegroup(
                    engine.as_ptr(),
                    voices.map_or(std::ptr::null_mut(), NonNull::as_ptr),
                );
            }
        }
    }
}

impl ProcessRuntime for M4aEngine {
    fn set_tempo_bpm(&mut self, bpm: f64) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_set_tempo_bpm(engine.as_ptr(), bpm) }
        }
    }

    fn note_on(&mut self, track: i32, key: u8, velocity: u8) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_note_on(engine.as_ptr(), track, key, velocity) }
        }
    }

    fn note_off(&mut self, track: i32, key: u8) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_note_off(engine.as_ptr(), track, key) }
        }
    }

    fn program_change(&mut self, track: i32, program: u8) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_program_change(engine.as_ptr(), track, program) }
        }
    }

    fn cc(&mut self, track: i32, cc: u8, value: u8) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_cc(engine.as_ptr(), track, cc, value) }
        }
    }

    fn pitch_bend(&mut self, track: i32, bend: i16) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_pitch_bend(engine.as_ptr(), track, bend) }
        }
    }

    fn process(&mut self, left: &mut [f32], right: &mut [f32]) {
        debug_assert_eq!(left.len(), right.len());
        let frames = left.len().min(i32::MAX as usize) as i32;
        if let Some(engine) = self.engine.as_mut() {
            unsafe {
                ffi::m4a_engine_process(
                    engine.as_ptr(),
                    left.as_mut_ptr(),
                    right.as_mut_ptr(),
                    frames,
                )
            }
        }
    }
}

impl Drop for M4aEngine {
    fn drop(&mut self) {
        self.clear_voicegroup();
    }
}

fn last_voicegroup_error() -> String {
    let ptr = unsafe { ffi::voicegroup_loader_last_error() };
    if ptr.is_null() {
        return "voicegroup_load failed".to_owned();
    }
    let message = unsafe { CStr::from_ptr(ptr) }
        .to_string_lossy()
        .into_owned();
    if message.is_empty() {
        "voicegroup_load failed".to_owned()
    } else {
        message
    }
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cargo test -p poryaaaa-plugin --lib runtime::tests`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add packages/poryaaaa/plugin/src/runtime.rs
git commit -m "feat: implement safe M4aEngine facade and safety invariants"
```

---

### Task 2: Integrate M4aEngine into PoryaaaaPlugin

**Files:**
- Modify: `packages/poryaaaa/plugin/src/plugin.rs`

**Interfaces:**
- Consumes: `M4aEngine`, `EngineConfig`, `EngineError` from `runtime.rs`.
- Produces: Updated `PoryaaaaPlugin` implementing `nice_plug::Plugin` using `M4aEngine`.

- [ ] **Step 1: Write a failing test in `plugin.rs` (or modify `plugin.rs` to break compilation first)**

Open `packages/poryaaaa/plugin/src/plugin.rs`, replace imports of `CPluginRuntime` with `M4aEngine`. This breaks the compile as `PoryaaaaPlugin` field uses the old type.

- [ ] **Step 2: Run test/build to verify it fails**

Run: `cargo check -p poryaaaa-plugin`
Expected: Compile error on `CPluginRuntime` not found.

- [ ] **Step 3: Update `plugin.rs` implementation**

Update `PoryaaaaPlugin`'s runtime type and lifecycle logic in `packages/poryaaaa/plugin/src/plugin.rs`.

1. Swap imports:
   ```rust
   use crate::{
       config::PluginConfig,
       editor,
       process::{self, ProcessRuntime},
       runtime::{M4aEngine, EngineConfig, EngineError},
       voicegroup, PoryaaaaParams, PROGRAM_COUNT,
   };
   ```
2. Update struct field:
   ```rust
   pub struct PoryaaaaPlugin {
       params: Arc<PoryaaaaParams>,
       runtime: Arc<Mutex<Option<M4aEngine>>>,
       last_host_tempo_bpm: Option<f64>,
       config: PluginConfig,
   }
   ```
3. Update `initialize()`:
   ```rust
   fn initialize(
       &mut self,
       _audio_io_layout: &AudioIOLayout,
       buffer_config: &BufferConfig,
       _context: &mut impl InitContext<Self>,
   ) -> bool {
       let config = EngineConfig {
           sample_rate: buffer_config.sample_rate,
           volume: self.config.volume,
           reverb: self.config.reverb,
       };
       let mut runtime = match M4aEngine::new(config) {
           Ok(runtime) => runtime,
           Err(err) => {
               self.set_runtime_voicegroup_error(err.to_string());
               *self.runtime.lock().expect("runtime lock") = None;
               return false;
           }
       };

       let project_root = self
           .params
           .project_root
           .read()
           .expect("project root read")
           .clone();
       let bank = self
           .params
           .voicegroup
           .read()
           .expect("voicegroup read")
           .clone();
       if !project_root.is_empty() && !bank.is_empty() {
           let committed_load = voicegroup::default_projects_json_path()
               .and_then(|path| Self::load_committed_voicegroup(self.params.as_ref(), &path));
           match committed_load {
               Some(status) if status.is_error => {
                   self.set_runtime_voicegroup_error(status.text);
                   runtime.clear_voicegroup();
               }
               Some(_) => match runtime.load_voicegroup(&project_root, &bank) {
                   Ok(()) => Self::write_runtime_voicegroup_status(self.params.as_ref(), None),
                   Err(err) => {
                       self.set_runtime_voicegroup_error(err.to_string());
                       runtime.clear_voicegroup();
                   }
               },
               None => {
                   self.set_runtime_voicegroup_error(
                       "default projects.json path is unavailable".to_owned(),
                   );
                   runtime.clear_voicegroup();
               }
           }
       }

       *self.runtime.lock().expect("runtime lock") = Some(runtime);
       self.reapply_host_state_to_runtime();
       true
   }
   ```
4. Update `reset()`:
   ```rust
   fn reset(&mut self) {
       let mut runtime = self.runtime.lock().expect("runtime lock");
       if let Some(runtime) = runtime.as_mut() {
           if runtime.reset().is_ok() {
               drop(runtime); // Release lock before state re-application
               self.reapply_host_state_to_runtime();
           }
       }
   }
   ```
5. Update `runtime_has_loaded_voicegroup()`:
   ```rust
   pub(crate) fn runtime_has_loaded_voicegroup(&self) -> bool {
       self.runtime
           .lock()
           .expect("runtime lock")
           .as_ref()
           .is_some_and(M4aEngine::is_ready)
   }
   ```
6. Update `run_background_task()`:
   ```rust
   fn run_background_task(
       params: &PoryaaaaParams,
       runtime: &Mutex<Option<M4aEngine>>,
       task: PoryaaaaBackgroundTask,
   ) {
       match task {
           PoryaaaaBackgroundTask::LoadVoicegroup {
               project_root,
               bank,
               projects_json_path,
           } => {
               if let Err(message) =
                   Self::publish_voicegroup(&project_root, &bank, &projects_json_path)
               {
                   Self::write_runtime_voicegroup_status(
                       params,
                       Some(voicegroup::VoicegroupLoadStatus {
                           text: message,
                           is_error: true,
                       }),
                   );
                   return;
               }

               let mut runtime = runtime.lock().expect("runtime lock");
               if let Some(runtime) = runtime.as_mut() {
                   match runtime.load_voicegroup(&project_root, &bank) {
                       Ok(()) => {
                           Self::write_runtime_voicegroup_status(
                               params,
                               Some(Self::loaded_status(&bank)),
                           );
                       }
                       Err(err) => {
                           Self::write_runtime_voicegroup_status(
                               params,
                               Some(voicegroup::VoicegroupLoadStatus {
                                   text: err.to_string(),
                                   is_error: true,
                               }),
                           );
                       }
                   }
               }
           }
       }
   }
   ```
7. Update `process()`:
   ```rust
   fn process(
       &mut self,
       buffer: &mut Buffer,
       _aux: &mut AuxiliaryBuffers,
       context: &mut impl ProcessContext<Self>,
   ) -> ProcessStatus {
       let channels = buffer.as_slice();
       if channels.len() < 2 {
           for channel in channels {
               channel.fill(0.0);
           }
           return ProcessStatus::Normal;
       }

       let (left_channel, remaining_channels) = channels.split_at_mut(1);
       let left = &mut left_channel[0];
       let right = &mut remaining_channels[0];
       let tempo = context.transport().tempo;
       self.remember_host_tempo(tempo);
       let mut runtime = self.runtime.lock().expect("runtime lock");
       let Some(runtime) = runtime.as_mut() else {
           process::clear_stereo(left, right);
           process::drain_midi_activity(self.params.midi_activity.as_ref(), || {
               context.next_event()
           });
           return ProcessStatus::Normal;
       };
       if !runtime.is_ready() {
           process::clear_stereo(left, right);
           process::drain_midi_activity(self.params.midi_activity.as_ref(), || {
               context.next_event()
           });
           return ProcessStatus::Normal;
       }

       process::process_stereo(
           runtime,
           left,
           right,
           tempo,
           Some(self.params.midi_activity.as_ref()),
           || context.next_event(),
       );

       ProcessStatus::KeepAlive
   }
   ```

- [ ] **Step 4: Run test to verify it passes**

Run: `cargo check -p poryaaaa-plugin`
Expected: Compile success for `plugin.rs`.

- [ ] **Step 5: Commit**

```bash
git add packages/poryaaaa/plugin/src/plugin.rs
git commit -m "refactor: integrate M4aEngine into PoryaaaaPlugin lifecycle"
```

---

### Task 3: Update Integration Tests in lib.rs

**Files:**
- Modify: `packages/poryaaaa/plugin/src/lib.rs`

**Interfaces:**
- Consumes: `M4aEngine` integration inside tests.
- Produces: Green test suite for the plugin.

- [ ] **Step 1: Write a failing test in `lib.rs` (or modify `lib.rs` to break compilation first)**

Open `packages/poryaaaa/plugin/src/lib.rs` and search for references to `CPluginRuntime`. Rename them to `M4aEngine` (and use the new constructors/initialization methods where applicable). This initially breaks compilation if references or parameters don't match the new facade signatures.

- [ ] **Step 2: Run test/build to verify it fails**

Run: `cargo test -p poryaaaa-plugin --lib tests`
Expected: Compile failure due to obsolete test patterns.

- [ ] **Step 3: Update integration tests**

Update the FFI integration tests in `packages/poryaaaa/plugin/src/lib.rs`:

```rust
    #[test]
    fn c_runtime_creates_resets_and_drops_engine() {
        let config = crate::runtime::EngineConfig {
            sample_rate: 48_000.0,
            volume: 100,
            reverb: 10,
        };
        let mut runtime = crate::runtime::M4aEngine::new(config).expect("runtime");

        assert!(runtime.reset().is_ok());
        assert!(!runtime.is_ready());
    }

    #[test]
    fn failed_runtime_voicegroup_load_keeps_no_loaded_voicegroup() {
        let config = crate::runtime::EngineConfig {
            sample_rate: 48_000.0,
            volume: 100,
            reverb: 10,
        };
        let mut runtime = crate::runtime::M4aEngine::new(config).expect("runtime");

        let result = runtime.load_voicegroup("/definitely/not/a/poryaaaa/project", "voicegroup000");

        assert!(result.is_err());
        assert!(!runtime.is_ready());
    }

    #[test]
    fn failed_runtime_voicegroup_replacement_keeps_loaded_voicegroup() {
        let root = temp_project("runtime-replace");
        write_file(
            &root,
            "sound/voice_groups.inc",
            "\
                \t.align 2
                voicegroup000::
                \tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3
            ",
        );
        let config = crate::runtime::EngineConfig {
            sample_rate: 48_000.0,
            volume: 100,
            reverb: 10,
        };
        let mut runtime = crate::runtime::M4aEngine::new(config).expect("runtime");
        runtime
            .load_voicegroup(&root.to_string_lossy(), "voicegroup000")
            .expect("initial load");

        let result = runtime.load_voicegroup("/definitely/not/a/poryaaaa/project", "voicegroup000");

        assert!(result.is_err());
        assert!(runtime.is_ready());
        assert!(runtime.reset().is_ok());
        assert!(runtime.is_ready());

        runtime.program_change(0, 0);
        runtime.cc(0, 7, 127);
        runtime.cc(0, 10, 64);
        runtime.note_on(0, 60, 100);
        let mut peak = 0.0f32;
        for _ in 0..8 {
            let mut left = [0.0f32; 512];
            let mut right = [0.0f32; 512];
            runtime.process(&mut left, &mut right);
            peak = left
                .iter()
                .chain(right.iter())
                .fold(peak, |peak, sample| peak.max(sample.abs()));
        }
        assert!(peak > 0.0001);

        fs::remove_dir_all(root).expect("remove temp project");
    }
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cargo test -p poryaaaa-plugin`
Expected: All tests pass.

- [ ] **Step 5: Commit**

```bash
git add packages/poryaaaa/plugin/src/lib.rs
git commit -m "test: update integration tests to use M4aEngine facade"
```
