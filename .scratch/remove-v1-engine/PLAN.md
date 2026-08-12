# Remove the v1 Engine Plan

Status: Complete
Reviewer: `FinalV1RemovalReview` — PASS, no remaining concrete blockers
Scope: `packages/poryaaaa` plus the `packages/poryaaaa-m4l` consumer and stale documentation

## Goal

Remove the v1 `M4AEngine` interface, implementation, compatibility state, and historical design references. Production audio must use the current `M4ADriver` → `M4ARegWriteBatch` → `HwAudio` path directly.

This is a clean cutover. The final tree must contain no compatibility facade, deprecated alias, wrapper retaining the old interface, or second engine hidden behind a new name.

## Current Finding

`plugin/m4a_engine.c/.h` is not an independent sound implementation anymore. It is an active compatibility facade around `M4ADriver` and `HwAudio`. It still owns duplicate state, shadow/audition driver sidecars, legacy telemetry structs, and wrapper entry points. The C CLAP plugin, Rust plugin, renderer, and tests still compile or call it.

The renderer already proves the intended architecture: it sends events to `M4ADriver`, advances the driver, renders its write batch through `HwAudio`, then consumes the writes. Its simultaneous `M4AEngine` dispatch is dead duplicate work because `render_frames()` ignores the engine.

## Decisions

- **The current production seam is authoritative.** Musical state and ingress belong to `M4ADriver`; register timing and audio output belong to `HwAudio`.
- **Do not replace `M4AEngine` with another owning facade.** Each product runtime owns one `M4ADriver` and one `HwAudio` and executes the explicit four-step render transaction.
- **Small host-specific runtimes are allowed.** Rust `ProcessRuntime` and CLAP plugin data may manage allocation and host lifecycle, but they must not recreate legacy tracks, channels, telemetry, sidecars, or ingress wrappers.
- **Host runtimes may coordinate but may not own musical compatibility state.** A runtime may contain allocation handles, host configuration mirrors, and render scratch buffers only. It must not contain track/channel arrays, poly-event queues, shadow/audition engines, or ingress methods that merely rename `m4a_*`. Final review must prove each audio runtime owns exactly one driver and one hardware renderer.
- **Delete facade-only behavior.** Shadow/audition engines, `polyDebugInvert`, audition state, poly-event telemetry, direct-write compatibility synchronization, and legacy public channel snapshots are not migrated.
- **Keep current production behavior already owned by v2.** Voicegroup assignment, note/CC/program/bend ingress, xCmd callbacks, tempo, song volume, reverb, analog filtering, maximum PCM channels, portamento/PWM behavior that is active in `M4ADriver`, ordered register writes, and `HwAudio` rendering remain.
- **Rename compatibility terminology that survives for real production reasons.** `compat_*` driver fields/functions and comments that describe the removed v1 implementation must receive domain names or be deleted. Do not remove audible behavior merely because its current identifier says `compat`.
- **No ABI shim.** Delete `M4AEngine`, `m4a_engine_*`, `M4ATrack`, `M4APCMChannel`, `M4ACGBChannel`, `M4APolyEvent`, and `M4AReverb` rather than leaving aliases or deprecated declarations.
- **Versioned formats are unrelated.** Trace format version 1, recorder `PRBY-v1`, and persisted plugin-state versions remain unless separately proven obsolete. They must not be renamed as part of this work.
- **Historical plans do not justify stale source references.** Delete superseded execution plans whose only remaining value is v1 migration history; rewrite durable behavior documents against current `M4ADriver` terminology.
- **Do not disturb parity work.** GBA-cycle ordering, write batching, FIFO/PSG behavior, mixer arithmetic, resampler phase, and pinned-mGBA gates are invariants.

## Target State

| Surface | Final state |
|---|---|
| `plugin/m4a/` | Sole musical driver implementation and public ingress/configuration interface. No v1 comparisons or `compat_*` names. |
| `plugin/hw_audio/` | Sole hardware renderer. Consumes ordered `M4ARegWriteBatch` values. |
| C CLAP plugin | Owns one driver and one hardware renderer; directly configures, dispatches, renders, resets, and destroys them. |
| Rust plugin | Rust runtime owns opaque driver and hardware handles through direct FFI; no `M4AEngine` binding. |
| Renderer CLI | Owns only its existing direct driver/hardware pair; no duplicate facade dispatch or lifecycle. |
| M4L external | Keeps its existing direct driver/hardware integration; stale v1 plan/cache prose is removed. |
| Tests | Assert observable driver/hardware and product behavior through production interfaces. No tests exist solely for the deleted facade. |
| Build graph | No `ENGINE_SOURCES`, `m4a_engine.c`, public `poryaaaa_engine` compatibility target, or Rust compilation of the facade. |
| Documentation | Describes only the current driver/hardware architecture. Unrelated schema/version identifiers remain. |

## Protected Work

Before implementation, preserve the current untracked research and stress-validator files without adding them to the removal commits:

- `.scratch/arch-fix/**`
- `.scratch/audio-parity-merge-readiness/**`
- `.scratch/psw-validation/**`
- `.scratch/remove-v1-engine/**`
- `packages/poryaaaa/tools/mgba-reference/validate_hardware_stress.py`
- `packages/poryaaaa/tools/mgba-reference/test_validate_hardware_stress.py`

The current parity working tree still has the six-argument M4L call while the five-argument fix is committed separately on `feature/poryaaaa-m4l-recorder`. Phase 0 must preserve that branch commit, and this plan must apply the five-argument call to the branch being verified without adding a shim.

## Phase 0 — Baseline and Contract Inventory

### Task 0.1 — Preserve the working tree

Owner: `task` subagent

- Record branch and worktree state.
- Preserve tracked and untracked work byte-for-byte before any source edit.
- Keep this plan local unless the user explicitly asks to commit it.

Acceptance:

- Every pre-existing local file can be restored exactly.
- No research artifact enters a source commit.

### Task 0.2 — Record the direct-path baseline

Owner: `reviewer` subagent

From `packages/poryaaaa`, configure the default build and run:

```sh
cmake -B build -DPORYAAAA_BUILD_TRACE_TOOLS=OFF
cmake --build build --target poryaaaa poryaaaa_render poryaaaa_test poryaaaa_unit_tests poryaaaa_engine_lifecycle
ctest --test-dir build -R '^poryaaaa_default_trace_link_graph$' --output-on-failure
./build/poryaaaa_unit_tests
./build/poryaaaa_engine_lifecycle
./build/poryaaaa_test <project-root> <voicegroup-name> /tmp/poryaaaa-v1-removal-baseline.wav
```

Also run one renderer MIDI smoke and record hashes or comparison results for representative output before code changes. Use the documented external project/voicegroup fixture when available; report the exact missing prerequisite otherwise.

Acceptance:

- Product, unit, lifecycle, WAV-export, and renderer status is recorded.
- At least one ordinary playback path is exercised.
- Baseline failures are distinguished from migration regressions.

### Task 0.3 — Freeze the behavior disposition

Owner: `task` subagent

Classify every public `m4a_engine_*` function and every public `M4AEngine` field before deletion:

1. direct `M4ADriver` equivalent,
2. host lifecycle/render transaction,
3. facade-only state to delete,
4. already unused symbol.

Explicitly verify the disposition of xCmd callbacks, voice refresh, PCM mix rate, portamento/PWM settings, analog filtering, legacy snapshots, poly-debug sidecars, and audition state.

Inventory all seven current driver flags — `compat_respect_base_midi_key`, `compat_portamento_enabled`, `compat_pwm_enabled`, `compat_pwm_active`, `compat_shadow_note`, `compat_zero_pcm_is_silent`, and `compat_skip_pwm_tick` — plus `m4a_internal_compat_effects_tick`, `m4a_internal_reset_portamento`, `m4a_internal_disable_pwm`, and `m4a_internal_compat_tick`. Include related track portamento/PWM state. Assign each a concrete deletion or domain-name destination and identify the direct setter when retained.

Inventory `test/test_engine.c` case-by-case. Tag each case as facade-only deletion, product lifecycle migration, or retained driver/write-batch/hardware contract. Merely including `m4a_engine.h` does not make an otherwise valuable PSG/PCM contract disposable.

Acceptance:

- Every exported symbol has exactly one destination or deletion reason.
- No behavior is silently retained through a compatibility alias.
- Every `compat_*` flag/function and related portamento/PWM field has an explicit final identifier, setter, and test or an explicit deletion reason.
- Every `test/test_engine.c` case has a recorded disposition; chip contracts are preserved before facade tests are deleted.

### Task 0.4 — Decouple shared headers before product cutover

Owner: `task` subagent

- Replace `plugin/voicegroup_loader.h`'s `m4a_engine.h` include with `voicegroup/voicegroup_types.h`.
- Decouple `plugin/voicegroup/voicegroup_loader.h`, GUI headers, `m4a_params.c`, and product headers from facade-owned types before changing product storage.
- Move `gPulseWidthModPatterns` out of `m4a_engine.h`; `plugin/m4a/m4a_track.c` currently includes the facade solely for this declaration. Put the declaration with its owning driver/voicegroup data rather than in a new compatibility header.
- Update `m4a_gui_set_voice_data` and related GUI declarations to use `ToneData`/voicegroup types only.
- Check `bench/audio_engine_bench.c`, mGBA-reference recorder/tools, and `plugin/m4a_engine_recorder.cpp/.h`. Retain already-direct or unrelated consumers without forcing migration; rename the recorder bridge only if the `m4a_engine` prefix would falsely imply facade ownership after deletion.
- Apply the M4L five-argument `hw_audio_render_events(hw, events, outL, outR, frames)` call if the current branch still passes `m4a_get_pcm_ring`; never restore the removed argument in `HwAudio`.

Acceptance:

- `m4a_track.c`, both voicegroup loader headers, GUI headers, and M4L compile surfaces no longer require `m4a_engine.h`.
- Header self-containment checks pass before `M4AEngine` storage is removed from either product.
- Current direct consumers stay direct; unrelated recorder behavior is unchanged.

## Phase 1 — Remove Facade Ownership from Products

Tasks 1.1 and 1.2 may run in parallel only after Task 0.4 lands because they touch separate product implementations. Task 1.3 follows after their direct ownership pattern is established.

### Task 1.1 — Cut the C CLAP plugin to direct ownership

Owner: `task` subagent

- Replace embedded `M4AEngine` in `M4APluginData` with one `M4ADriver*` and one `HwAudio*`.
- Allocate, reset, and destroy both explicitly in plugin lifecycle methods.
- Route voicegroups, settings, GUI voice refresh, MIDI ingress, and xCmd callbacks directly to `M4ADriver`.
- Update `m4a_params.c` and GUI/plugin signatures together: replace `data->engine` and `M4ATrack` assumptions with the driver pointer and `ToneData`/voicegroup types.
- Treat allocation as a transaction: create the driver, create hardware, unwind the driver if hardware creation fails, publish `activated` only after both succeed, and destroy/reset in one documented order.
- In the process callback, use exactly:
  1. `m4a_advance`,
  2. `m4a_get_pending_writes`,
  3. `hw_audio_render_events`,
  4. `m4a_consume_writes`.
- Preserve the recommended maximum advance-frame chunking.
- Replace `M4A_ENGINE_MAX_PROCESS_FRAMES` in the plugin process loop with `M4A_RECOMMENDED_MAX_ADVANCE_FRAMES`; do not copy the legacy numeric constant.
- Remove all includes and field access requiring `m4a_engine.h`.

Acceptance:

- The CLAP target has no symbol or header dependency on `M4AEngine`.
- Activation, reset, state restore, voicegroup reload, GUI voice edits, MIDI ingress, and audio rendering use the direct path.
- Failure cleanup cannot leak either allocation or leave a half-initialized plugin active.

### Task 1.2 — Cut the Rust plugin to direct ownership

Owner: `task` subagent

- Replace the opaque `M4AEngine` FFI with opaque `M4ADriver` and `HwAudio` bindings.
- Replace `EngineHandle` with explicit driver/hardware ownership in `ProcessRuntime`.
- Bind only direct lifecycle, configuration, ingress, write-batch, and rendering functions.
- Implement the same bounded four-step render transaction as the C and M4L paths.
- Remove `m4a_engine.c` from `NATIVE_SOURCES` and `m4a_engine.h` from `NATIVE_HEADERS` in `plugin/build.rs`.
- Remove the Rust `M4A_ENGINE_MAX_PROCESS_FRAMES` mirror and make `process.rs` chunk using a direct driver limit exported from the native interface; do not introduce a second hard-coded `2048`.
- Update Rust tests to exercise output and lifecycle through the direct runtime.

Acceptance:

- Rust source and generated native compilation contain no `M4AEngine` or `m4a_engine_*` reference.
- Rust panic/error paths release both native allocations exactly once.
- Existing Rust plugin behavior and audio output pass their package-local tests.

### Task 1.3 — Remove duplicate renderer ownership

Owner: `sonic` subagent

- Delete renderer `M4AEngine` allocation, configuration, cleanup, includes, and every duplicate `m4a_engine_*` call.
- Change `dispatch_event(M4AEngine*, ...)` and `render_frames(M4AEngine*, ...)` to remove the engine parameter; delete the `(void)engine` marker.
- Keep its existing direct driver/hardware behavior, then rename `g_v2_drv/g_v2_hw` so their names no longer imply a competing engine.
- Explicitly remove the duplicate settings/event calls around current lines 891–915, 1289–1297, 1313, and 1438 while retaining the single direct calls.
- Do not change MIDI mapping, `--solo`, voicegroup selection, song volume, reverb, maximum-channel configuration, render chunking, or output formatting.

Acceptance:

- Every renderer event is dispatched once.
- The renderer owns exactly one driver and one hardware renderer.
- Baseline and post-change representative output are byte-identical unless an existing nondeterministic metadata field is documented.

## Phase 2 — Delete the Compatibility Module

### Task 2.1 — Migrate the remaining tests

Owner: `task` subagent

- Rewrite `test/test_wav_export.c`, `test/test_engine_lifecycle.c`, and only the cases tagged for migration in Task 0.3 through direct production interfaces or product entry points.
- Keep observable xCmd, lifecycle, voicegroup, rendering, timing, PSG, PCM, write ordering, and output contracts.
- Delete only tests tagged facade-only after every retained driver/write-batch/hardware contract has a replacement test in the same commit.
- Do not add test-only accessors to driver or hardware internals.

Acceptance:

- Every retained test can fail on a plausible production regression.
- No test includes `m4a_engine.h` or constructs `M4AEngine`.
- Contract coverage exists at the driver/write-batch/hardware seam and product lifecycle seam.

### Task 2.2 — Confirm residual include decoupling

Owner: `task` subagent

- Re-run the Task 0.4 dependency inventory after product and test migrations.
- Remove any residual `m4a_engine.h` include from first-party source; do not defer a known header dependency to source-deletion time.
- Confirm M4L consumes the driver and hardware headers directly and uses the five-argument render call.

Acceptance:

- No public/shared header depends on the facade scheduled for deletion.
- `m4a_track.c`, both voicegroup loader headers, GUI/plugin headers, tests, Rust native headers, and M4L are clean before Task 2.3.

### Task 2.3 — Remove source and build wiring

Owner: `task` subagent

- Delete `plugin/m4a_engine.c` and `plugin/m4a_engine.h`.
- Delete `plugin/m4a_reverb.h` if the final reference search confirms it remains facade-only.
- Remove `ENGINE_SOURCES` and facade sources from every target in `packages/poryaaaa/CMakeLists.txt`.
- Delete `plugin/porydaw`'s unconsumed `poryaaaa_engine` target and `poryaaaa::engine` alias; verify no first-party CMake file references either. Keep `voicegroup_loader.c` owned by the existing `voicegroup_loader` target rather than coupling it to a replacement engine target.
- Remove the already-migrated Rust build and FFI entries; deletion must find no remaining facade source/header entry.
- Remove now-unused facade-only test source groups without deleting the retained voicegroup loader library.

Acceptance:

```sh
# Conceptual repository assertion; use repository search tooling in execution.
M4AEngine        -> 0 first-party source/build references
m4a_engine_      -> 0 first-party source/build references
m4a_engine.[ch]  -> absent
ENGINE_SOURCES   -> absent
```

- Default product binaries link one driver implementation and one hardware implementation.
- No deprecated alias, forwarding header, or dead CMake target remains.

## Phase 3 — Remove v1 Semantics from the Current Driver

### Task 3.1 — Delete facade-only behavior

Owner: `task` subagent

Confirm these disappear with the facade rather than being reimplemented:

- shadow driver/hardware,
- audition driver/hardware and slot maps,
- `polyDebugInvert`, audition flags, poly event buffers/counters,
- public legacy track/PCM/CGB mirror structs,
- direct-write state synchronization,
- facade low-pass/reverb buffers and scratch arrays,
- unused `m4a_engine_tick` and `m4a_track_vol_pit_set` behavior.

Acceptance:

- No product allocates more than one driver/hardware pair per audio runtime for compatibility behavior.
- No removed state is copied into a newly named struct.

### Task 3.2 — Rename or remove `compat_*` driver state

Owner: `task` subagent

Use the Task 0.3 disposition for every current flag and function:

- `compat_respect_base_midi_key`
- `compat_portamento_enabled`
- `compat_pwm_enabled`
- `compat_pwm_active`
- `compat_shadow_note`
- `compat_zero_pcm_is_silent`
- `compat_skip_pwm_tick`
- `m4a_internal_compat_effects_tick`
- `m4a_internal_reset_portamento`
- `m4a_internal_disable_pwm`
- `m4a_internal_compat_tick`

Delete facade-only items. Rename retained items for their actual domain behavior, expose a direct setter only when a product actively configures it, and update related `M4ADriverTrack` portamento/PWM fields. Never introduce a `compat_*` alias or move the prefix to another module. Rewrite comments that cite v1 source line parity as current design authority.

Acceptance:

- No `compat_*` identifier remains in first-party production code.
- Audible retained behavior has a domain-named interface and observable contract.
- No code comment references removed `m4a_engine.c` as implementation authority.

## Phase 4 — Documentation and Cross-Package Cleanup

### Task 4.1 — Remove superseded v1 plans

Owner: `sonic` subagent

Inspect documents whose purpose may have been the completed v1-to-v2 migration. Delete only documents with no current behavioral or verification content. Rewrite live specifications in place against `M4ADriver` and `HwAudio`.

Use repository-relative paths and inspect at minimum:

- `packages/poryaaaa/HW_AUDIO_SCAFFOLD_PLAN.md`
- `packages/poryaaaa/NEXT_SESSION.md`
- `packages/poryaaaa/pushback v1.md`, `pushback v2.md`, `pushback v3.md`, and follow-ups
- `packages/poryaaaa/plans/m4a-engine-v2-clap-refactor.md`
- `packages/poryaaaa/noise_verification_gate.md`
- `packages/poryaaaa/xcmd.md`
- `packages/poryaaaa-m4l/source/audio/poryaaaa~/PLAN.md`

`xcmd.md` describes live selectors and must be retained and rewritten, not deleted. Preserve recorder `PRBY-v1` and trace-format version documentation.

Acceptance:

- No durable documentation tells maintainers to use or compare against the removed v1 engine.
- Current xCmd/noise/parity documentation names only live symbols and paths.
- Historical deletion does not remove unrelated trace/recorder format documentation.

### Task 4.2 — Update active package guidance

Owner: `task` subagent

- Update `packages/poryaaaa/README.md` and package guidance that describe `m4a_engine` as the public wrapper.
- Update the stale M4L `PORYA_ROOT` cache description mentioning `m4a_engine.c`.
- Document the direct ownership/render sequence once at the narrowest durable location.

Acceptance:

- Build and architecture documentation matches the final link graph.
- Both touched package boundaries are documented and independently verified.

## Phase 5 — Verification

### Task 5.1 — Format and static-check touched source

Owner: `sonic` subagent

- Run root `xcrun clang-format` only on intentionally touched C/C++ files.
- Run Rust formatting only if Rust source changed.
- Run language-server diagnostics for touched C/C++ and Rust source where configured.
- Search first-party paths for removed symbols, obsolete source names, and engine-specific v1 prose; manually classify unrelated format-version hits.

Acceptance:

- No affected-source diagnostics remain.
- Zero source/build references remain to the removed interface.
- Any remaining text `v1` occurrence is proven to denote a live external/storage/trace format, not the engine.

### Task 5.2 — Verify `packages/poryaaaa`

Owner: `reviewer` subagent

Run the default product, test, lifecycle, renderer, and WAV-export commands from Phase 0. Then run the opt-in trace build and gates:

```sh
cmake -B build -DPORYAAAA_BUILD_TRACE_TOOLS=ON
cmake --build build --target poryaaaa_audio_trace poryaaaa_driver_trace poryaaaa_audio_trace_tool_tests
ctest --test-dir build -R '^poryaaaa_opt_in_trace_link_graph$' --output-on-failure
./build/poryaaaa_audio_trace_tool_tests
python3 -m unittest discover -s tools/mgba-reference -p 'test_validate_driver.py'
python3 -m unittest discover -s tools/mgba-reference -p 'test_validate_driver_matrix.py'
python3 -m unittest discover -s tools/mgba-reference -p 'test_driver_compare.py'
python3 -m unittest discover -s tools/mgba-reference -p 'test_native_compare.py'
bash tools/mgba-reference/test_driver_reference_scenarios.sh
```

Run one exact native replay comparison against pinned mGBA when its existing fixture is available.

Acceptance:

- All default and opt-in gates pass or an exact external prerequisite is reported.
- Representative renderer output matches baseline.
- Link-graph checks prove no duplicate engine/audio implementation enters shipping binaries.

### Task 5.3 — Verify `packages/poryaaaa-m4l`

Owner: `reviewer` subagent

- Inspect `poryaaaa~.cpp` and apply the five-argument render call if the current branch still has six arguments.
- From `packages/poryaaaa-m4l`, run `npm run build:externals`; for a focused retry use `cmake --build build-ninja --target poryaaaa_tilde`, confirmed by the generated Ninja graph.
- If the existing `swiftly` wrapper still fails at its 4096-argument launch limit, prove the `poryaaaa~.cpp` object compiled first and record the wrapper limitation separately from compile success.

Acceptance:

- `poryaaaa~.cpp` compiles against the final driver/hardware headers.
- No M4L source or CMake description references the removed facade.

### Task 5.4 — Independent final review

Owner: `reviewer` subagent

Review the complete diff for:

- hidden compatibility aliases or renamed v1 state,
- duplicate driver/hardware ownership,
- incomplete Rust or C product migration,
- missing error-path cleanup,
- weakened playback/parity contracts,
- accidental removal of unrelated versioned formats,
- stale documentation/build references,
- host runtime structs acquiring legacy track/channel/poly/sidecar state under new names,
- residual `gPulseWidthModPatterns` or voicegroup coupling through a replacement compatibility header,
- bench/reference/recorder consumers accidentally broken or unnecessarily migrated,
- unrelated edits or committed scratch artifacts.

Acceptance:

- No concrete merge blocker remains.
- The reviewer can explain the final product ownership and render flow without mentioning `M4AEngine`.

## Commit Slices

Keep each commit buildable. Shared-header decoupling precedes product storage changes; source deletion follows all callers and tests.

1. `refactor(poryaaaa): decouple shared headers from v1 engine`
2. `refactor(poryaaaa): render C and Rust products through direct audio interfaces`
3. `refactor(poryaaaa): remove duplicate renderer engine ownership`
4. `test(poryaaaa): migrate engine contracts to production seams`
5. `refactor(poryaaaa): remove v1 engine compatibility module`
6. `docs(poryaaaa): remove obsolete v1 engine guidance`
7. Separate M4L consumer/documentation commit on its owning branch if branch topology still requires it.

If a product migration and its required build/header change cannot compile independently, combine them into one atomic commit rather than committing a broken intermediate.

## Definition of Done

- `m4a_engine.c/.h` and facade-only `m4a_reverb.h` are absent.
- `M4AEngine`, `m4a_engine_*`, `ENGINE_SOURCES`, legacy facade structs, shadow/audition sidecars, and `compat_*` production identifiers are absent.
- C CLAP, Rust plugin, renderer, and M4L each use the direct driver/hardware architecture.
- No compatibility shims, aliases, deprecated targets, or renamed facade modules remain.
- Default playback, lifecycle, renderer, WAV export, unit, trace, and pinned-mGBA gates retain their contracts.
- Active docs describe only the current architecture; remaining `v1` strings refer exclusively to intentionally versioned formats.
- Local research and stress-validator work remains preserved and uncommitted.

## Execution Result

- `packages/poryaaaa` default product, renderer, unit, lifecycle, link-graph, and WAV-export gates passed.
- Opt-in trace tools, parser tests, Python validation suites, driver reference scenarios, and the exact pinned-mGBA replay comparison passed.
- The `packages/poryaaaa-m4l` `poryaaaa_tilde` target compiled against the direct interfaces. The full external build remains blocked while linking unrelated `porya.reverb~`: the `swiftly` compiler wrapper receives 4,771 arguments, above its 4,096-argument macOS launch limit.
- The independent final review found no merge blocker and confirmed direct `M4ADriver` plus `HwAudio` ownership, render ordering, teardown, Rust FFI mutability, CMake ownership, trace-format consumers, and stale-reference cleanup.
