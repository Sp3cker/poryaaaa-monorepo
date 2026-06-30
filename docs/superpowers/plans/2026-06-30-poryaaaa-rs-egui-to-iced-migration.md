# poryaaaa-rs GUI Migration from egui to iced

> **For agentic workers:** implement this plan task-by-task with `subagent-driven-development` or `executing-plans`. Keep the GUI migration isolated to `packages/poryaaaa/plugin/` unless a verification step proves a boundary change is required.

## Goal

Convert the Rust CLAP plugin GUI in `packages/poryaaaa/plugin/` from `egui`/`nice-plug-egui` to `iced`/`nice-plug-iced`, preserving current poryaaaa-rs behavior:

- 16 MIDI activity indicators.
- Automatable 0..127 Volume and Reverb rotary controls with stable CLAP parameter IDs `vol` and `rev`.
- Project-root and voicegroup draft fields.
- A native folder dialog is acceptable for selecting the project root.
- Do **not** add explicit drag-and-drop behavior.
- "Load" must keep the current transaction semantics: validate/publish the selected voicegroup, commit params only after success, then request host restart through the existing path.

## Verified reference points

### Current poryaaaa-rs egui integration

- `packages/poryaaaa/plugin/Cargo.toml`
  - Currently depends on:
    - `egui = { version = "=0.34.1", features = ["default_fonts"] }`
    - `egui-file-dialog = "0.13.0"`
    - `nice-plug-egui = { path = "../../../third_party/nice-plug/crates/nice-plug-egui" }`
- `packages/poryaaaa/plugin/src/params.rs`
  - `PoryaaaaParams` persists `editor_state: Arc<EguiState>` under `#[persist = "editor-state"]`.
  - `DEFAULT_EDITOR_WIDTH = 525`, `DEFAULT_EDITOR_HEIGHT = 325`.
  - Persistent voicegroup fields are `project_root` and `voicegroup`.
  - Audio params are `volume: IntParam` with ID `vol`, and `reverb: IntParam` with ID `rev`.
- `packages/poryaaaa/plugin/src/editor.rs`
  - `GuiState` owns the draft project root, draft voicegroup, `egui_file_dialog::FileDialog`, status, and `MidiActivityLight` array.
  - `show_project_root_selector()` renders the text field plus Browse button.
  - `project_root_dialog()` configures the current egui in-window directory picker.
  - `show_midi_activity()` requests repaint while activity is hot.
  - `show_audio_dial()` is a hand-drawn egui rotary dial using `ParamSetter`.
  - `create_editor()` wraps everything with `create_egui_editor()` and `ResizableWindow`.
- `packages/poryaaaa/plugin/src/plugin.rs`
  - `PoryaaaaPlugin::editor()` calls `editor::create_editor(self.params.clone(), async_executor)`.
  - `PoryaaaaBackgroundTask::LoadVoicegroup` is already the GUI-to-background load path.

### Repo-local iced references

- `third_party/nice-plug/examples/gain_iced/src/lib.rs`
  - Uses `WindowState` persisted as `#[persist = "window-state"]`.
  - Calls `create_iced_editor(window_state, editor_state, notifier, Default::default(), |editor_state, nice_ctx| application(...).theme(...).subscription(...).run())`.
  - Uses `PollSubNotifier` and `iced::poll_events()` for GUI polling/redraw.
  - `MyGui::update()` may return `()`; the adapter accepts return values convertible into `iced::Task`.
- `third_party/nice-plug/examples/iced_audio_widget/Cargo.toml`
  - Documents that `nice-plug-iced` needs the `canvas` feature for `iced_audio` widgets.
  - Uses `iced_audio = { version = "0.14.1", features = ["nice-plug"] }`.
- `third_party/nice-plug/examples/iced_audio_widget/src/lib.rs`
  - Uses `iced_audio::Knob::new(nice_to_iced(&params.gain)).on_gesture(...)`.
  - Uses `iced_audio::param::set_nice_param(&param, gesture, &setter)` for nice-plug parameter automation gestures.

## Target architecture

### Dependencies

Replace egui-specific GUI dependencies with iced dependencies:

```toml
# remove
egui = { version = "=0.34.1", features = ["default_fonts"] }
egui-file-dialog = "0.13.0"
nice-plug-egui = { path = "../../../third_party/nice-plug/crates/nice-plug-egui" }

# add
nice-plug-iced = { path = "../../../third_party/nice-plug/crates/nice-plug-iced", features = ["canvas"] }
iced_audio = { version = "0.14.1", features = ["nice-plug"] }
rfd = "0.15"
```

`rfd` is the planned native folder dialog dependency. If Cargo resolution or plugin-host embedding exposes a platform issue, treat that as a spike result and document the exact failing host/platform. Do not replace it with drag-and-drop.

### Persistent GUI state

Update `PoryaaaaParams`:

- Replace `nice_plug_egui::EguiState` with `nice_plug_iced::WindowState`.
- Prefer a clean cutover field:

```rust
#[persist = "window-state"]
pub window_state: Arc<WindowState>,
```

This will not preserve old egui window-size state, which is acceptable for a GUI framework migration. Keep `DEFAULT_EDITOR_WIDTH` and `DEFAULT_EDITOR_HEIGHT`.

### Iced editor shape

Model `editor.rs` after the repo-local `gain_iced` example:

```rust
create_iced_editor(
    params.window_state.clone(),
    PoryaaaaEditorState { params: params.clone() },
    notifier.clone(),
    Default::default(),
    move |editor_state, nice_ctx| {
        application(
            editor_state,
            nice_ctx,
            PoryaaaaGui::new,
            move |gui, message| gui.update(message, &async_executor),
            PoryaaaaGui::view,
        )
        .theme(PoryaaaaGui::theme)
        .subscription(|_| iced::poll_events().map(|_| Message::Poll))
        .run()
    },
)
```

The exact state names are not important; preserving the separation is:

- `PoryaaaaEditorState`: persistent/editor-open state passed through `EditorState`.
- `PoryaaaaGui`: iced UI state for draft fields, latest status, and activity light decay.
- `Message`: all UI events.
- `NiceGuiContext`: source of the `ParamSetter`.

### Polling and MIDI activity

The egui editor can call `request_repaint_after()`. The iced migration should use the nice-plug-iced polling pattern:

- Add a `PollSubNotifier` field to `PoryaaaaPlugin` if process-driven redraw notifications are needed.
- Pass a clone of the notifier into `create_iced_editor()`.
- Subscribe with `iced::poll_events().map(|_| Message::Poll)`.
- On `Message::Poll`, read `params.midi_activity.snapshot()`, update `[MidiActivityLight; MIDI_CHANNEL_COUNT]`, and refresh `voicegroup_status`.

Initial implementation may use conservative polling/redraw while the editor is open. If CPU usage is noticeable, follow the `gain_iced` example more closely: notify only when the process thread observes new MIDI activity or status changes.

### Parameter controls

Replace `show_audio_dial()` with `iced_audio::Knob`:

- Volume: `iced_audio::Knob::new(nice_to_iced(&params.volume)).on_gesture(Message::VolumeGestured)`
- Reverb: `iced_audio::Knob::new(nice_to_iced(&params.reverb)).on_gesture(Message::ReverbGestured)`
- In `update()`, call `iced_audio::param::set_nice_param(&params.volume, gesture, &setter)` and the same for reverb.
- Display current values using `normalized_value_to_string()` or the existing `IntParam` value string path. Avoid reading smoothed audio values; these are UI parameters.

### Project-root picker

Use a native directory dialog through `rfd`, because native dialogs are acceptable for this migration. Keep behavior identical after selection:

- `BrowseClicked` opens an async native folder picker.
- Selection updates only `draft_project_root`.
- It must not write `params.project_root`.
- It must not trigger a load by itself.
- `LoadClicked` remains the only commit/load action.

Use iced `Task`, not `Command`, with this nice-plug-iced version:

```rust
Message::BrowseClicked => {
    return Task::perform(
        async move {
            rfd::AsyncFileDialog::new()
                .set_title("Choose Project Root")
                .pick_folder()
                .await
                .map(|handle| handle.path().to_path_buf())
        },
        Message::DirectorySelected,
    );
}
```

`[INFERENCE]` If `rfd::AsyncFileDialog` cannot run correctly inside the CLAP-host window on a target platform, fall back to a blocking native folder dialog launched from the UI message handler only after confirming it does not deadlock the tested host. Do not add drag-and-drop as the fallback.

### Voicegroup load transaction

Keep the existing transaction semantics from `editor.rs` and `plugin.rs`:

1. User edits `draft_project_root` and `draft_voicegroup`.
2. User clicks `Load`.
3. `editor.rs` resolves `shared_projects_json::default_projects_json_path()`.
4. GUI writes a temporary "Loading ..." status.
5. GUI sends `PoryaaaaBackgroundTask::LoadVoicegroup { project_root, bank, projects_json_path }`.
6. Background load validates/publishes via the existing plugin path.
7. Only a successful load commits `params.project_root` and `params.voicegroup`.
8. Existing host restart request handling remains the way the active runtime sees the new voicegroup.

Do not introduce live mutation of audio-thread engine state from the GUI.

### Styling and layout

The first iced version should preserve layout, not chase visual polish:

- Dark theme via `Theme::Dark`, matching `gain_iced`.
- Header: `poryaaaa`.
- Separator or spacing after header.
- MIDI activity row with 16 numbered/positioned indicators.
- Volume and Reverb knobs side by side.
- Project root label + text input + Browse button.
- Voicegroup label + text input + Load button.
- Status label with error color for `is_error`.

Custom Calamity fonts are currently egui-specific through `calamity_font_definitions()`. `[INFERENCE]` Iced font loading should be handled after the functional port compiles, likely through iced font registration/settings. Do not block the first compile on custom fonts; add them as a follow-up phase in this same migration before acceptance.

## Implementation phases

### Phase 1 — Dependency and persistent-state cutover

- [ ] Update `packages/poryaaaa/plugin/Cargo.toml`.
- [ ] Replace `EguiState` with `WindowState` in `params.rs`.
- [ ] Update constructors and tests that refer to `editor_state`.
- [ ] Run `cargo check` in `packages/poryaaaa/plugin`; expected failure at this phase is only unresolved `editor.rs` egui references.

### Phase 2 — Rewrite editor shell to nice-plug-iced

- [ ] Replace `create_egui_editor()` with `create_iced_editor()`.
- [ ] Define `PoryaaaaEditorState`, `PoryaaaaGui`, and `Message`.
- [ ] Carry `AsyncExecutor<PoryaaaaPlugin>` into the `application()` update closure for `LoadClicked`.
- [ ] Add or thread a `PollSubNotifier` as needed for `iced::poll_events()`.
- [ ] Remove egui imports and egui-only helpers from `editor.rs`.

### Phase 3 — Port controls and state updates

- [ ] Implement Volume and Reverb with `iced_audio::Knob`.
- [ ] Port MIDI activity state/decay to `Message::Poll`.
- [ ] Port project-root text field and native Browse dialog with `rfd`.
- [ ] Port voicegroup text field, Load button, status rendering, and host restart request handling.
- [ ] Keep all current draft-vs-committed param boundaries.

### Phase 4 — Tests

- [ ] Replace egui-specific tests in `packages/poryaaaa/plugin/src/lib.rs`.
- [ ] Keep or rewrite tests for:
  - editor factory returns an editor;
  - host resize behavior;
  - MIDI activity light hold/decay logic;
  - directory selection updates draft project root only;
  - Load commits only through the background transaction path.
- [ ] Do not delete behavior coverage just because the widget framework changed.

### Phase 5 — Fonts and polish

- [ ] Reintroduce Calamity fonts for iced if supported by the local iced stack.
- [ ] Match current minimum window size.
- [ ] Verify knob sizing and text-field expansion in a resizable host/editor window.
- [ ] Confirm no explicit drag-and-drop behavior was added.

## Verification commands

Run from `packages/poryaaaa/plugin` unless noted:

```bash
cargo check
cargo test
```

Run from the repo root for package-level confidence:

```bash
just test poryaaaa
```

If `just test poryaaaa` rebuilds C targets, verify failures by package boundary rather than assuming a Rust GUI issue.

Manual smoke test after compile:

1. Build/install the Rust CLAP bundle with the existing `just build poryaaaa-rs` recipe.
2. Open the editor in a CLAP host.
3. Confirm window opens at the expected size.
4. Move Volume and Reverb knobs and confirm host automation/parameter values update.
5. Type a project root manually and confirm it remains draft-only before Load.
6. Use Browse and confirm the native folder dialog updates only the draft project root.
7. Click Load and confirm status, persisted project-root/voicegroup, and host restart behavior.
8. Send MIDI and confirm the channel lights show activity without adding drag-and-drop behavior.

## Risks

- **Native dialog embedding:** `rfd` may behave differently under different CLAP hosts. Verify in the primary macOS host before declaring the picker complete.
- **Redraw pressure:** polling for MIDI activity can redraw more often than needed. Start simple; optimize with `PollSubNotifier::notify()` only if measurement shows a problem.
- **Font parity:** egui font setup does not translate directly to iced. Treat functional parity as first priority, then restore font assets.
- **API drift:** nice-plug-iced uses iced `Task` in this checkout. Do not copy older `Command` examples from external iced docs without checking local APIs.

## Rollback plan

Before implementation, note the pre-migration commit. To roll back only the GUI migration files:

```bash
git checkout <pre-migration-commit> -- \
  packages/poryaaaa/plugin/Cargo.toml \
  packages/poryaaaa/plugin/src/params.rs \
  packages/poryaaaa/plugin/src/editor.rs \
  packages/poryaaaa/plugin/src/lib.rs
```

If implementation adds a notifier field to `plugin.rs`, include that file in the rollback list.

## Acceptance checklist

- [ ] `packages/poryaaaa/plugin/Cargo.toml` no longer depends on egui, egui-file-dialog, or nice-plug-egui.
- [ ] `PoryaaaaParams` persists `WindowState`.
- [ ] Editor creation uses `nice_plug_iced::create_iced_editor`.
- [ ] Volume/Reverb use `iced_audio::Knob` and nice-plug parameter gestures.
- [ ] Native folder dialog updates only the draft project-root field.
- [ ] No explicit drag-and-drop support was added.
- [ ] Load transaction behavior matches the existing egui implementation.
- [ ] MIDI activity indicators still hold/decay for recent channel events.
- [ ] Rewritten tests cover behavior, not egui plumbing.
- [ ] `cargo check`, `cargo test`, and `just test poryaaaa` pass after implementation.
