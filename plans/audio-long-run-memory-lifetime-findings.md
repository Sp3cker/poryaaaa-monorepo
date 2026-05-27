# Audio Long-Run Memory And Lifetime Findings

Date: 2026-05-27

Scope: review of memory leaks, resource lifetime bugs, realtime allocation/blocking, and state accumulation paths that could degrade long-running `poryaaaa` audio. Four read-only subagents reviewed engine/driver, hardware audio, plugin/UI/recorder, and loader/renderer slices. The normal unit suite passed after the review (`415/415 tests passed`). An ASan/UBSan build ran far enough to expose a test callback lifetime bug, then aborted before completing the suite.

## Remaining Recommended Fix Order

1. Decide whether PCM interpolation should defensively support non-loader `WaveData` without a sentinel.
2. Fix `m4a_engine_process()` event-queue chunking.
3. Fix realtime recorder allocation/blocking.
4. Fix GUI voice editing ownership/race.
5. Fix SQ2 enable semantics and other lower-risk cleanup paths.

The sanitizer blockers are fixed. The remaining order moves from the last PCM memory-safety decision into broader engine and realtime architecture changes.

## Findings

### 1. ASan-blocking xCmd callback lifetime bug

Severity: High for verification, likely test-only.

Status: Fixed in `test/test_engine.c` by keeping the xCmd capture context alive for the full `test_xcmd_subcommands()` engine lifetime. ASan now progresses past `Testing XCMD subcommands...` and stops later on finding 3, the PCM interpolation tail read.

Evidence:

- `test/test_engine.c`: `test_xcmd_subcommands()` installs `capture_xcmd` with scoped stack captures such as `xcmd_cap`, `xwave_cap`, and `wait_cap`.
- After the first scoped `XcmdCapture` goes out of scope, the engine still holds that callback context. Later xCmd payloads invoke the stale pointer.
- ASan reported `stack-use-after-scope` in `capture_xcmd`.

Impact:

- Blocks full sanitizer verification.
- The production API still trusts caller callback lifetime, but plugin production usage sets the callback to long-lived `M4APluginData`.

Likely fix:

- Keep the capture object alive for the whole test, or clear/reassign the callback before leaving each scope.

### 2. PCM interpolation reads one byte past tail sample

Severity: High for memory safety, medium for audible impact.

Status: The ASan failure observed after finding 1 was a test fixture bug in `test_v2_pcm_frequency_scale()`: it created synthetic `WaveData` with `size == 1024` but only `1024` bytes of backing data. Real loader paths allocate `size + 1` and set a sentinel at `data[size]`. The test now allocates `fakeData[1025]` and sets `fakeData[1024] = fakeData[1023]`. Full ASan unit tests pass after this fixture fix. A separate production hardening decision remains: whether `render_channel()` should defensively tolerate arbitrary `WaveData` that did not come from the loader.

Evidence:

- `plugin/m4a/m4a_pcm.c`: `render_channel()` reads `ptr[1]` for interpolation before checking whether the segment has at least two readable samples.
- `count` is decremented only after the read.

Impact:

- One-sample waves or final samples of non-looped/looped regions can read past valid sample data.
- Could produce tail clicks, data-dependent interpolation garbage, or sanitizer failures.

Likely fix:

- Add a focused unit test with a tiny WaveData sample.
- Clamp/interpolate against the last valid sample at segment end, preserving the existing extra sentinel behavior where it is valid.

### 3. Engine process can overflow bounded event queue on large blocks

Severity: Medium.

Evidence:

- `plugin/m4a_engine.c`: `m4a_engine_process()` calls `m4a_advance(engine->driver, numSamples)` once, then renders and consumes once.
- The driver event queue is bounded. Existing tests and comments rely on chunking with `M4A_RECOMMENDED_MAX_ADVANCE_FRAMES`.
- The CLAP process path chunks internally, but direct engine callers and offline paths can pass larger blocks.

Impact:

- Dropped CGB register writes and PCM publish events can desync chip state from driver state.
- Long offline renders or unusual host buffer sizes can degrade output without a hard failure.

Likely fix:

- Move the chunking invariant into `m4a_engine_process()` so callers cannot violate it.
- Keep the CLAP-level chunking if useful, but make the engine API safe by default.

### 4. Realtime recorder path allocates and locks on the audio thread

Severity: High for long-session plugin stability.

Evidence:

- `plugin/m4a_plugin.c`: `process_midi_event()` calls `m4a_recorder_push()` from the process path when armed.
- `plugin/recorder/recorder_core.cpp`: `push_event_in_block()` takes a `std::mutex` and appends to `std::vector`.
- Tempo and advance paths also lock recorder state.

Impact:

- Armed recording can allocate, grow memory unbounded, and block the audio thread.
- GUI save/clear/snapshot can contend with process-time recorder writes.

Likely fix:

- Do not write directly into `std::vector` from the audio thread.
- Use a preallocated lock-free or single-producer queue from process to main/worker thread, then drain into the vector outside realtime.
- Add a clear cap/backpressure policy for long recordings.

### 5. GUI voice editor mutates live engine-owned voice data

Severity: High for data races and unstable audio while editing.

Evidence:

- Plugin gives GUI direct pointers to `loadedVg->voices`.
- GUI sliders mutate `ToneData` directly on the GUI/main thread.
- Timer code later calls `m4a_engine_refresh_voices()` while audio/MIDI processing can read current program/voice data.

Impact:

- Data race between GUI writes and audio-thread reads.
- Can create unstable timbre, inconsistent track snapshots, or crashes when editing during playback.

Likely fix:

- Stop giving the GUI mutable engine-owned pointers.
- Give GUI an editable copy/draft model.
- Commit changes through an explicit main-to-audio command path at a safe boundary, or restart/reload the engine state atomically.

### 6. SQ2 can be enabled by NR22 without a trigger

Severity: Medium.

Evidence:

- `plugin/hw_audio/hw_psg.c`: `M4A_REG_NR22` writes set `sq2_enabled` based on envelope/DAC bits.
- Driver can emit envelope writes during sustain/release without a fresh trigger.
- SQ1, wave, and noise paths are more explicitly trigger/DAC gated.

Impact:

- A volume/update write can revive stale SQ2 state and produce ghost output.

Likely fix:

- Add chip-level test: disable SQ2, apply `NR22` without `NR24` trigger, assert silence.
- Make `NR22` update envelope/DAC state but not enable the channel unless trigger semantics require it.

### 7. Project asset index can go stale on project-root reload

Severity: Medium, not likely the core audio degradation.

Evidence:

- GUI reload updates `projectRoot` and `voicegroupName`, clears overrides, and requests restart.
- It does not rebuild `ProjectAssetIndex`.
- `project_asset_index_rebuild()` has the right cleanup path but is not called there.

Impact:

- Sample selector can keep old project asset entries and resolve them against the new root.

Likely fix:

- Rebuild or destroy/recreate `assetIndex` when project root changes.

### 8. Renderer `--solo` error path leaks initialized resources

Severity: Medium for CLI error path, low for plugin audio.

Evidence:

- `cmd/poryaaaa_render.c`: unknown `--solo` returns after MIDI events, loop expansion, voicegroup, engine, and v2 globals may already be live.

Impact:

- CLI exits with leaked resources on that failure path.

Likely fix:

- Route this path through the same cleanup block as later failures.

### 9. Repeated override application retains old sample allocations

Severity: Medium if overrides can be applied repeatedly to the same loaded voicegroup.

Evidence:

- `project_asset_index_apply_overrides()` loads a new sample/prog-wave and overwrites `voice->wav` or `voice->wavePointer`.
- The replacement allocation is registered for `voicegroup_free()`.
- Prior replacement allocations remain retained until the full voicegroup is freed.

Impact:

- Repeated applies can grow sample memory.

Likely fix:

- Prefer rebuilding a fresh `LoadedVoiceGroup` for override application, or track per-override owned replacements so repeated replacement frees/reuses the previous one safely.

### 10. Lower-risk cleanup items

- `hw_pcm_render()` has an unsafe helper-level `frames <= 0 || !ring` branch that would turn negative frames into a huge `memset` size if called directly. Main caller currently guards it.
- Parser maps and registration helpers assign `realloc()` directly to owned pointers. On OOM this leaks the old owner and can dereference `NULL`.
- `log=<path>` in config intentionally leaks a process-lifetime `strdup`.

## Verification Status

Completed:

- `cmake --build build --target poryaaaa_unit_tests`
- `./build/poryaaaa_unit_tests`
- Result: `415/415 tests passed`

Partial:

- Configured and built ASan/UBSan unit test target in `build-asan`.
- `./build-asan/poryaaaa_unit_tests` aborted on the test-only xCmd callback lifetime bug before completing.

Open:

- Full sanitizer pass after fixing the test callback lifetime issue.
- Targeted tests for PCM tail interpolation, engine chunking, and SQ2 no-trigger enable behavior.
