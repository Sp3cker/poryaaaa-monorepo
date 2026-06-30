# poryaaaa-rs Restart Load Transaction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the poryaaaa-rs Load button validate and commit the selected project/bank, then request host restart instead of hot-swapping the active audio runtime.

**Architecture:** The Load button is a transaction script: validate/publish through `voicegroup-core`, commit persisted params only after success, update editor status, and request host restart. `initialize()` remains the only place that materializes the committed C voicegroup into `M4aEngine`; the background Load task must not call `runtime.load_voicegroup()` on the active engine.

**Tech Stack:** Rust 2021, NicePlug, nice-plug-egui, voicegroup-core, existing C `M4aEngine`/voicegroup loader through FFI.

---

## Assumptions

- This slice does not move sample materialization into Rust.
- `project_root` and `voicegroup` remain persisted params.
- Typing and Browse remain draft-only; only Load commits.
- The editor is open when the user presses Load, so it can consume a one-shot restart flag and call NicePlug's GUI-thread `ParamSetter::request_restart()`.
- If a host reports restart as unsupported, surface that in editor status. Do not fake a restart by mutating the live runtime.

## File Structure

- Modify `packages/poryaaaa/plugin/src/params.rs`
  - Add a non-persisted restart-request flag.
  - Add small methods for committed voicegroup state, status, and restart flag access.
- Modify `packages/poryaaaa/plugin/src/plugin.rs`
  - Keep `PoryaaaaBackgroundTask::LoadVoicegroup` data-only.
  - Change the task executor to run the Load transaction without runtime access.
  - Remove active runtime hot-swap from the background task.
  - Keep `initialize()` responsible for loading committed params into `M4aEngine`.
- Modify `packages/poryaaaa/plugin/src/editor.rs`
  - Consume the restart flag in the egui update closure.
  - Call `ParamSetter::request_restart()` on the GUI thread.
- Modify `packages/poryaaaa/plugin/src/lib.rs`
  - Replace tests that expect hot-swap behavior with restart-policy tests.
- Do not modify `packages/voicegroup-core` for this slice.
- Do not modify `third_party/nice-plug`; `ParamSetter::request_restart()` already exists.

## Success Criteria

- Pressing Load with a valid root/bank commits persisted params and emits `projects.json`.
- A successful Load sets exactly one pending restart request.
- The background Load task does not lock or mutate the active `M4aEngine`.
- A failed Load preserves committed params, preserves the active runtime, and does not request restart.
- `initialize()` loads the committed project/bank into a fresh runtime.
- Focused Rust plugin tests pass serially and `cargo check` passes in `packages/poryaaaa/plugin`.

---

### Task 1: Add Param Facade And Restart Flag

**Files:**
- Modify: `packages/poryaaaa/plugin/src/params.rs`
- Test: `packages/poryaaaa/plugin/src/lib.rs`

- [ ] **Step 1: Write failing tests for restart flag and committed state helpers**

Add tests near the existing editor/plugin tests in `packages/poryaaaa/plugin/src/lib.rs`:

```rust
#[test]
fn params_restart_request_is_one_shot() {
    let params = PoryaaaaParams::default();

    assert!(!params.take_host_restart_request());
    params.request_host_restart();
    assert!(params.take_host_restart_request());
    assert!(!params.take_host_restart_request());
}

#[test]
fn params_committed_voicegroup_helpers_read_and_write_state() {
    let params = PoryaaaaParams::default();

    assert_eq!(params.committed_voicegroup_selection(), None);

    params.commit_voicegroup_selection("/tmp/project", "voicegroup000");

    assert_eq!(
        params.committed_voicegroup_selection(),
        Some(("/tmp/project".to_string(), "voicegroup000".to_string()))
    );
}
```

- [ ] **Step 2: Run tests and verify they fail**

Run:

```bash
cd packages/poryaaaa/plugin
cargo test params_restart_request_is_one_shot params_committed_voicegroup_helpers_read_and_write_state -- --test-threads=1
```

Expected: compile failure because `take_host_restart_request`, `request_host_restart`, `committed_voicegroup_selection`, and `commit_voicegroup_selection` do not exist yet.

- [ ] **Step 3: Add minimal implementation**

In `packages/poryaaaa/plugin/src/params.rs`, add the atomic import:

```rust
use std::sync::atomic::{AtomicBool, Ordering};
```

Add this field to `PoryaaaaParams`:

```rust
pub(crate) host_restart_pending: Arc<AtomicBool>,
```

Initialize it in `Default`:

```rust
host_restart_pending: Arc::new(AtomicBool::new(false)),
```

Add these methods to `impl PoryaaaaParams`:

```rust
/// Reads the committed voicegroup selection used for the next runtime initialization.
pub(crate) fn committed_voicegroup_selection(&self) -> Option<(String, String)> {
    let project_root = self
        .project_root
        .read()
        .expect("project root read")
        .clone();
    let bank = self.voicegroup.read().expect("voicegroup read").clone();
    (!project_root.is_empty() && !bank.is_empty()).then_some((project_root, bank))
}

/// Commits a voicegroup selection after validation succeeds.
pub(crate) fn commit_voicegroup_selection(&self, project_root: &str, bank: &str) {
    *self.project_root.write().expect("project root write") = project_root.to_string();
    *self.voicegroup.write().expect("voicegroup write") = bank.to_string();
}

/// Mirrors voicegroup load status into editor-visible state.
pub(crate) fn write_voicegroup_status(&self, status: Option<VoicegroupLoadStatus>) {
    *self
        .runtime_voicegroup_status
        .write()
        .expect("runtime voicegroup status write") = status;
}

/// Reads the latest editor-visible voicegroup load status.
pub(crate) fn voicegroup_status(&self) -> Option<VoicegroupLoadStatus> {
    self.runtime_voicegroup_status
        .read()
        .expect("runtime voicegroup status read")
        .clone()
}

/// Requests a host deactivate/reactivate cycle after a successful Load transaction.
pub(crate) fn request_host_restart(&self) {
    self.host_restart_pending.store(true, Ordering::Release);
}

/// Consumes a pending host restart request exactly once.
pub(crate) fn take_host_restart_request(&self) -> bool {
    self.host_restart_pending.swap(false, Ordering::AcqRel)
}
```

- [ ] **Step 4: Run tests and verify they pass**

Run:

```bash
cd packages/poryaaaa/plugin
cargo test params_restart_request_is_one_shot params_committed_voicegroup_helpers_read_and_write_state -- --test-threads=1
```

Expected: both tests pass.

- [ ] **Step 5: Commit**

```bash
git add packages/poryaaaa/plugin/src/params.rs packages/poryaaaa/plugin/src/lib.rs
git commit -m "Add restart request state for poryaaaa load"
```

---

### Task 2: Convert Background Load To Commit-And-Restart Transaction

**Files:**
- Modify: `packages/poryaaaa/plugin/src/plugin.rs`
- Test: `packages/poryaaaa/plugin/src/lib.rs`

- [ ] **Step 1: Replace hot-swap tests with restart-policy tests**

Rename `load_voicegroup_task_swaps_active_runtime_without_reinitialize` to:

```rust
#[test]
fn load_voicegroup_task_commits_params_and_requests_restart_without_runtime_swap() {
    use nice_plug::prelude::*;

    let root = temp_project("runtime-task-restart");
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
    let layout = PoryaaaaPlugin::AUDIO_IO_LAYOUTS[0];
    let buffer_config = BufferConfig {
        sample_rate: 48_000.0,
        min_buffer_size: None,
        max_buffer_size: 512,
        process_mode: ProcessMode::Realtime,
    };
    let mut context = TestInitContext;
    assert!(plugin.initialize(&layout, &buffer_config, &mut context));
    assert!(!plugin.runtime_has_loaded_voicegroup());

    let execute = plugin.task_executor();
    execute(crate::plugin::PoryaaaaBackgroundTask::LoadVoicegroup {
        project_root: root.to_string_lossy().into_owned(),
        bank: "voicegroup000".to_owned(),
        projects_json_path: root.join("out/projects.json"),
    });

    assert!(!plugin.runtime_has_loaded_voicegroup());
    assert_eq!(
        plugin.params_for_test().voicegroup.read().expect("voicegroup read").as_str(),
        "voicegroup000",
    );
    assert!(plugin.params_for_test().take_host_restart_request());
    assert!(fs::read_to_string(root.join("out/projects.json"))
        .expect("projects.json")
        .contains("\"bank\": \"voicegroup000\""));

    fs::remove_dir_all(root).expect("remove temp project");
}
```

Update `load_voicegroup_task_failure_keeps_loaded_runtime_and_committed_params` so it asserts no restart request on failure:

```rust
assert!(!plugin.params_for_test().take_host_restart_request());
```

If the setup currently relies on the first task hot-loading the runtime, change the test name to:

```rust
fn load_voicegroup_task_failure_keeps_committed_params_and_does_not_request_restart()
```

and remove assertions that require `runtime_has_loaded_voicegroup()` to be true before the invalid load.

- [ ] **Step 2: Run tests and verify they fail**

Run:

```bash
cd packages/poryaaaa/plugin
cargo test load_voicegroup_task_commits_params_and_requests_restart_without_runtime_swap load_voicegroup_task_failure_keeps_committed_params_and_does_not_request_restart -- --test-threads=1
```

Expected: the success test fails because the current task hot-loads the runtime and does not set the restart flag.

- [ ] **Step 3: Simplify `run_background_task`**

In `packages/poryaaaa/plugin/src/plugin.rs`, change `task_executor()` from capturing `runtime` to only capturing params:

```rust
fn task_executor(&mut self) -> TaskExecutor<Self> {
    let params = self.params.clone();
    Box::new(move |task| Self::run_background_task(params.as_ref(), task))
}
```

Replace `run_background_task` with:

```rust
/// Runs a GUI-dispatched voicegroup Load transaction outside the audio runtime.
fn run_background_task(params: &PoryaaaaParams, task: PoryaaaaBackgroundTask) {
    match task {
        PoryaaaaBackgroundTask::LoadVoicegroup {
            project_root,
            bank,
            projects_json_path,
        } => {
            let status = Self::load_voicegroup(params, &project_root, &bank, &projects_json_path);
            let should_restart = !status.is_error;
            params.write_voicegroup_status(Some(status));
            if should_restart {
                params.request_host_restart();
            }
        }
    }
}
```

Change `commit_voicegroup_params` to delegate to the params facade:

```rust
fn commit_voicegroup_params(params: &PoryaaaaParams, project_root: &str, bank: &str) {
    params.commit_voicegroup_selection(project_root, bank);
}
```

Change `write_runtime_voicegroup_status` to delegate to the params facade:

```rust
fn write_runtime_voicegroup_status(
    params: &PoryaaaaParams,
    status: Option<VoicegroupLoadStatus>,
) {
    params.write_voicegroup_status(status);
}
```

- [ ] **Step 4: Run tests and verify they pass**

Run:

```bash
cd packages/poryaaaa/plugin
cargo test load_voicegroup_task_commits_params_and_requests_restart_without_runtime_swap load_voicegroup_task_failure_keeps_committed_params_and_does_not_request_restart -- --test-threads=1
```

Expected: restart-policy tests pass.

- [ ] **Step 5: Commit**

```bash
git add packages/poryaaaa/plugin/src/plugin.rs packages/poryaaaa/plugin/src/lib.rs
git commit -m "Request restart after poryaaaa voicegroup load"
```

---

### Task 3: Consume Restart Requests In The Editor

**Files:**
- Modify: `packages/poryaaaa/plugin/src/editor.rs`
- Test: `packages/poryaaaa/plugin/src/lib.rs`

- [ ] **Step 1: Add a focused test for the one-shot restart flag path**

If direct editor closure testing is too brittle, keep this as a params-level test from Task 1 and verify the editor path by compile plus host smoke. Do not introduce a fake egui editor framework only for this.

- [ ] **Step 2: Update the editor closure to call `request_restart()`**

In `packages/poryaaaa/plugin/src/editor.rs`, rename the update closure argument from `_setter` to `setter`:

```rust
move |ui, setter, _queue, gui_state| {
```

Replace raw status writes in the Load button block with params facade calls:

```rust
params.write_voicegroup_status(Some(status.clone()));
```

Replace raw status reads with:

```rust
gui_state.voicegroup_status = params.voicegroup_status();
```

After reading status and before rendering it, consume the restart request:

```rust
if params.take_host_restart_request() && !setter.request_restart() {
    let status = VoicegroupLoadStatus {
        text: "Loaded voicegroup, but host restart is unavailable".to_string(),
        is_error: true,
    };
    params.write_voicegroup_status(Some(status.clone()));
    gui_state.voicegroup_status = Some(status);
}
```

- [ ] **Step 3: Run editor-related tests**

Run:

```bash
cd packages/poryaaaa/plugin
cargo test editor_factory_returns_editor_for_default_params editor_advertises_and_applies_host_resize directory_picker_selection_updates_draft_project_root_only -- --test-threads=1
```

Expected: all pass.

- [ ] **Step 4: Commit**

```bash
git add packages/poryaaaa/plugin/src/editor.rs packages/poryaaaa/plugin/src/lib.rs
git commit -m "Trigger host restart from poryaaaa editor"
```

---

### Task 4: Keep Initialize As The Runtime Materialization Point

**Files:**
- Modify: `packages/poryaaaa/plugin/src/plugin.rs`
- Test: `packages/poryaaaa/plugin/src/lib.rs`

- [ ] **Step 1: Write or keep initialize behavior tests**

Keep `plugin_default_config_loads_voicegroup_before_first_process` as the main contract test. It should continue to prove that committed `project_root` and `voicegroup` load into a fresh runtime during `initialize()`.

Add this focused test if it does not already exist:

```rust
#[test]
fn initialize_loads_committed_voicegroup_into_fresh_runtime() {
    use nice_plug::prelude::*;

    let root = temp_project("initialize-committed-load");
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

    let layout = PoryaaaaPlugin::AUDIO_IO_LAYOUTS[0];
    let buffer_config = BufferConfig {
        sample_rate: 48_000.0,
        min_buffer_size: None,
        max_buffer_size: 512,
        process_mode: ProcessMode::Realtime,
    };
    let mut context = TestInitContext;

    assert!(plugin.initialize(&layout, &buffer_config, &mut context));
    assert!(plugin.runtime_has_loaded_voicegroup());

    fs::remove_dir_all(root).expect("remove temp project");
}
```

- [ ] **Step 2: Simplify committed selection reads**

In `initialize()`, replace separate `project_root` and `bank` lock reads with:

```rust
if let Some((project_root, bank)) = self.params.committed_voicegroup_selection() {
    let committed_load = shared_projects_json::default_projects_json_path()
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
```

This preserves the existing initialization contract while making Load and initialize clearly separate phases.

- [ ] **Step 3: Run initialize tests**

Run:

```bash
cd packages/poryaaaa/plugin
cargo test plugin_default_config_loads_voicegroup_before_first_process initialize_loads_committed_voicegroup_into_fresh_runtime -- --test-threads=1
```

Expected: both pass.

- [ ] **Step 4: Commit**

```bash
git add packages/poryaaaa/plugin/src/plugin.rs packages/poryaaaa/plugin/src/lib.rs
git commit -m "Keep poryaaaa runtime load in initialize"
```

---

### Task 5: Final Verification

**Files:**
- No new files unless a test exposes a missing helper.

- [ ] **Step 1: Format**

Run:

```bash
cd packages/poryaaaa/plugin
cargo fmt --check
```

Expected: exits 0.

- [ ] **Step 2: Run serial tests**

Run:

```bash
cd packages/poryaaaa/plugin
cargo test -- --test-threads=1
```

Expected: all plugin tests pass.

- [ ] **Step 3: Run cargo check**

Run:

```bash
cd packages/poryaaaa/plugin
cargo check
```

Expected: exits 0. Existing `nice-plug` macOS dead-code warnings are acceptable if unchanged.

- [ ] **Step 4: Optional host smoke after code changes**

If the local clap-host checkout exists, build and instantiate the plugin:

```bash
cd packages/poryaaaa/plugin
cargo build
/Users/spencer/dev/cProjects/clap-host/builds/ninja-system/host/Debug/clap-host -p target/debug/libporyaaaa_clap_plugin.dylib -i 0
```

Expected output includes:

```text
Loading plugin with id: com.sp3cker.poryaaaa-rs index: 0
```

- [ ] **Step 5: Final commit if tasks were not committed individually**

```bash
git add packages/poryaaaa/plugin/src/params.rs packages/poryaaaa/plugin/src/plugin.rs packages/poryaaaa/plugin/src/editor.rs packages/poryaaaa/plugin/src/lib.rs
git commit -m "Use restart transaction for poryaaaa voicegroup load"
```

---

## Self-Review

- Spec coverage: The plan implements validate/publish, committed params, editor status, and host restart. It removes hot-swap behavior from the background task and keeps runtime materialization in `initialize()`.
- Placeholder scan: No `TBD`, generic "add validation", or undefined implementation steps remain.
- Type consistency: The plan consistently uses `PoryaaaaParams::request_host_restart()`, `take_host_restart_request()`, `commit_voicegroup_selection()`, `committed_voicegroup_selection()`, `write_voicegroup_status()`, and `voicegroup_status()`.
