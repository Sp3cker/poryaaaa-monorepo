# Recorder mid2agb Export Implementation Plan

**Goal:** Make recorder exports from `poryaaaa` and `poryaaaa-m4l` reliably
produce compact, mid2agb-friendly SMF files, with final MIDI byte generation
owned by the shared C++ recorder/writer.

**Current architecture:** `packages/poryaaaa/plugin/recorder/` is the shared
recorder and SMF writer module. `poryaaaa` already records beat-stamped events
from the CLAP adapter and writes SMF bytes in C++. `poryaaaa-m4l` still uses its
wrapper-local recorder buffer plus TypeScript SMF writer, and that current M4L
output remains the migration oracle until parity tests prove otherwise.

**Tech stack:** C++23 recorder code under `packages/poryaaaa/plugin/recorder/`,
C CLAP adapter code under `packages/poryaaaa/plugin/m4a_plugin.c`, poryaaaa
unit tests under `packages/poryaaaa/test/`, TypeScript M4L save/export adapter
code under `packages/poryaaaa-m4l/code-src/`, and local mid2agb validation from
the `packages/ccomidi/mid2agb` source/tool when available.

## Current State

### Completed in poryaaaa

- `RecorderCore` stores beat-stamped MIDI events as `MidiRecord { beats, status, data1, data2 }`.
- `m4a_recorder_push_beats()` exposes a C API for CLAP/plugin code to push beat-stamped events.
- `plugin/m4a_plugin.c` has a CLAP-side `RecorderBeatMapper` that reads block-start `process->transport`, handles mid-block `CLAP_EVENT_TRANSPORT`, and maps event frame positions to beat positions.
- Recorder capture is gated by `recorderArmed` and currently records only when host beat timing is available.
- `m4a_recorder_save_smf()` writes through the shared C++ `write_smf1()` path.
- `plugin/m4a_gui.cpp` saves recorder SMFs with `ppq = 96`.
- `smf_writer.cpp` anchors export time at the floored first real Note On beat.
- Note events round to the nearest 96 PPQ tick.
- Controller/program/pitch-style events use the backward coarse-grid path, coalesce latest values per same-tick cell, and preserve GBA extended-command CC bytes `0x1D`, `0x1E`, and `0x1F`.
- Music-track events are collected into pending per-channel descriptors and stable-sorted before writing, instead of depending on unsafe whole-track MidiFile sorting.
- `test/test_recorder_core.cpp` covers beat recording, bridge push counting, POSIX path rejection, coarse-grid/coalescing behavior, XCMD preservation, and note tick normalization.

### Still true in poryaaaa

- `SmfWriteOptions` only carries `ppq` and `tempoBpm`; it does not yet model time signature, export range, loop markers, ccomidi initial state, or an explicit anchor mode.
- The default 96 PPQ value is passed from GUI/bridge call sites rather than named as a shared recorder constant.
- Hosts that do not provide a beat timeline can still drive the engine, but the recorder does not stamp or export those events today.
- Tempo automation is not a full recorder export feature. Beat-stamped event timing is independent of tempo, and tempo is currently conductor metadata for export.
- Held-note tracking still uses one held note per channel/pitch key, so overlapping same-pitch notes on one channel are not represented as separate lifetimes.
- There is no dedicated mid2agb round-trip validation target yet.

### Still true in poryaaaa-m4l

- `code-src/recorder_smf_writer.ts` remains the current production SMF byte writer.
- The M4L external receives raw MIDI bytes plus a latched `beats <float>` value from `[plugsync~]`.
- The TypeScript writer uses `PPQ = 96`, beat-relative timing, export range anchoring, state replay, loop-marker support when explicit marker fields are supplied, and writer-specific M4L tick compensation.
- `code-src/ccomidi_recorder.ts` remains the Live API/save adapter. It collects export range, output path, ccomidi voicemap, initial CC state, status reporting, and the dumped PRBY buffer.
- Several recorder-focused TypeScript tests exist but are not all included in the default `npm test` command.
- Wrapper-local recorder C++ under `source/audio/poryaaaa~/recorder/` still shadows the shared poryaaaa recorder module.

## Migration Rule

Treat current M4L output as the oracle for M4L migration. The first safe
migration check is:

1. Capture or construct one PRBY/event fixture.
2. Write SMF bytes with `packages/poryaaaa-m4l/code-src/recorder_smf_writer.ts`.
3. Write SMF bytes with the shared C++ writer from the same beat-stamped events and save-time metadata.
4. Compare parsed SMF structure and timing, not raw bytes unless the writer implementation is expected to be byte-identical.

Any M4L-specific one-tick compensation belongs in the M4L adapter/export bridge,
not in `RecorderCore` or generic `smf_writer.cpp`.

## mid2agb Constraint

The local mid2agb source scales MIDI ticks like this:

- event time: `(24 * g_clocksPerBeat * event.time) / g_midiTimeDiv`
- note duration: `(24 * g_clocksPerBeat * event.param2) / g_midiTimeDiv`
- default `g_clocksPerBeat = 1`, so the default target is 24 clocks per quarter note
- `-X` changes this to 48 clocks per quarter note

For a 96 PPQ SMF at default mid2agb settings, meaningful event starts and
durations should land on multiples of 4 MIDI ticks to avoid integer truncation.
The shared writer already uses a 4-tick coarse grid for controller/program/pitch
state events. Remaining work is to explicitly test note starts/durations against
mid2agb scaling and decide whether note durations should also be quantized to a
mid2agb grid.

## File Responsibilities

- `packages/poryaaaa/plugin/recorder/recorder_core.{h,cpp}`: host-neutral beat-stamped capture buffer and snapshot shape.
- `packages/poryaaaa/plugin/recorder/smf_writer.{h,cpp}`: SMF tick conversion, per-channel ordering, state coalescing, held-note flush, and future export metadata.
- `packages/poryaaaa/plugin/m4a_engine_recorder.{h,cpp}`: C bridge from plugin/GUI code into the C++ recorder and writer.
- `packages/poryaaaa/plugin/m4a_plugin.c`: CLAP timing adapter, recorder gating, MIDI/CLAP event conversion, and host tempo handling.
- `packages/poryaaaa/plugin/m4a_gui.cpp`: recorder tab UI and save call.
- `packages/poryaaaa/test/test_recorder_core.cpp`: current focused recorder and SMF writer tests.
- `packages/poryaaaa/CMakeLists.txt`: recorder target and test target wiring.
- `packages/poryaaaa-m4l/code-src/recorder_smf_writer.ts`: current M4L writer oracle and eventual retirement target.
- `packages/poryaaaa-m4l/code-src/ccomidi_recorder.ts`: temporary Live API/save adapter and future caller of the shared writer surface.
- `packages/poryaaaa-m4l/source/audio/poryaaaa~/poryaaaa~.cpp`: M4L external surface for future shared-writer save/write commands.
- `packages/poryaaaa-m4l/source/audio/poryaaaa~/recorder/`: wrapper-local recorder buffer implementation to retire after parity.

## Task 1: Update And Preserve poryaaaa Recorder Baseline

**Files:**
- Modify: `packages/poryaaaa/test/test_recorder_core.cpp`
- Modify: `packages/poryaaaa/plugin/recorder/smf_writer.h`
- Modify: `packages/poryaaaa/plugin/recorder/smf_writer.cpp`
- Modify only if needed: `packages/poryaaaa/plugin/m4a_gui.cpp`

- [x] Store beat-stamped events in `RecorderCore`.
- [x] Add C bridge support for beat-stamped pushes.
- [x] Use CLAP transport beat timing in the plugin adapter.
- [x] Save recorder SMFs at 96 PPQ from the GUI.
- [x] Add tests for beat capture, note tick rounding, coarse-grid controller behavior, coalescing, and XCMD preservation.
- [ ] Add a named shared constant such as `kDefaultRecorderPpq = 96` and use it from GUI/bridge call sites.
- [ ] Add a test that reads the generated SMF header and asserts `ticksPerBeat == 96`.
- [ ] Add a test that documents current behavior for hosts without CLAP beat timeline: no recorder events are stamped/exported.
- [ ] Run `cmake --build build --target poryaaaa_unit_tests`.
- [ ] Run `./build/poryaaaa_unit_tests`.

## Task 2: Lock In mid2agb-Facing Writer Tests

**Files:**
- Modify: `packages/poryaaaa/test/test_recorder_core.cpp`
- Create if the test grows too large: `packages/poryaaaa/test/test_recorder_mid2agb.cpp`
- Modify if a new test target is created: `packages/poryaaaa/CMakeLists.txt`

- [x] Test dense CC/PC coalescing while preserving ordered GBA extended-command CCs.
- [x] Test near-grid note events rounding to the expected 96 PPQ ticks.
- [ ] Add a one-note test at beat 0 with a one-quarter duration and assert all emitted music-track deltas are non-negative.
- [ ] Add a mid2agb scaling test that validates exported note starts and durations land on multiples of 4 MIDI ticks for default 24-clock mid2agb mode, or explicitly documents why notes keep finer 96 PPQ placement.
- [ ] Add a same-pitch overlap test that documents the current failure or expected new behavior before changing held-note tracking.
- [ ] If a new target is created, register it in `CMakeLists.txt`; otherwise keep these under `poryaaaa_unit_tests`.
- [ ] Run `cmake --build build --target poryaaaa_unit_tests`.
- [ ] Run `./build/poryaaaa_unit_tests`.

## Task 3: Finish Shared Writer Export Metadata

**Files:**
- Modify: `packages/poryaaaa/plugin/recorder/smf_writer.h`
- Modify: `packages/poryaaaa/plugin/recorder/smf_writer.cpp`
- Modify: `packages/poryaaaa/test/test_recorder_core.cpp`

- [ ] Extend `SmfWriteOptions` with the metadata M4L currently supplies to `buildSmf()`: time signature, export range, explicit loop markers, ccomidi initial PC/CC state, anchor mode, and any M4L tick-compensation mode.
- [ ] Keep the default poryaaaa/CLAP path minimal: 96 PPQ, one tempo, no range unless requested.
- [ ] Add an explicit anchor-mode option for current first-note anchoring versus explicit export-range anchoring.
- [ ] Preserve pre-anchor PC/CC state by clamping it to tick 0 when using first-note or explicit-range anchoring.
- [ ] Add tests for pre-note PC/CC setup at tick 0 and for long silent lead-in not generating a huge export.
- [ ] Run `cmake --build build --target poryaaaa_unit_tests`.
- [ ] Run `./build/poryaaaa_unit_tests`.

## Task 4: Decide And Test Tempo Policy

**Files:**
- Modify: `packages/poryaaaa/plugin/recorder/smf_writer.h`
- Modify: `packages/poryaaaa/plugin/recorder/smf_writer.cpp`
- Modify if adapter behavior changes: `packages/poryaaaa/plugin/m4a_plugin.c`
- Modify: `packages/poryaaaa/test/test_recorder_core.cpp`

- [ ] State the current policy in code/tests: beat-stamped events determine placement; `tempoBpm` writes conductor metadata.
- [ ] Decide whether tempo automation is unsupported, flattened, or integrated.
- [ ] Add a constant-tempo test that proves beat-stamped event placement does not depend on sample rate.
- [ ] Add a tempo-change test that captures the chosen policy.
- [ ] Avoid moving CLAP-specific tempo reconstruction into `RecorderCore`.
- [ ] Run `cmake --build build --target poryaaaa_unit_tests`.
- [ ] Run `./build/poryaaaa_unit_tests`.

## Task 5: Fix Held-Note Lifetime Tracking

**Files:**
- Modify: `packages/poryaaaa/plugin/recorder/smf_writer.cpp`
- Modify: `packages/poryaaaa/test/test_recorder_core.cpp`

- [ ] Replace the held-note set with a per-channel, per-pitch count or stack.
- [ ] Ensure overlapping same-pitch Note On events require matching Note Off events before the note is considered closed.
- [ ] Flush every still-open note at export end with deterministic order.
- [ ] Run the same-pitch overlap test.
- [ ] Run `cmake --build build --target poryaaaa_unit_tests`.
- [ ] Run `./build/poryaaaa_unit_tests`.

## Task 6: Add mid2agb Round-Trip Validation

**Files:**
- Modify: `packages/poryaaaa/test/test_recorder_core.cpp`
- Create if needed: `packages/poryaaaa/test/test_recorder_mid2agb.cpp`
- Modify: `packages/poryaaaa/CMakeLists.txt`

- [ ] Detect an existing local mid2agb executable or build one from `packages/ccomidi/mid2agb` as a test fixture when available.
- [ ] For generated recorder MIDI fixtures, run mid2agb and assert the output `.s` file is produced.
- [ ] Add a size guard for the `.s` output so obvious timing blowups fail the test.
- [ ] Skip gracefully when the local mid2agb executable is unavailable.
- [ ] Run the round-trip test with mid2agb present.
- [ ] Run `cmake --build build --target poryaaaa_unit_tests`.
- [ ] Run `./build/poryaaaa_unit_tests`.

## Task 7: Build M4L Writer Parity Harness

**Files:**
- Modify: `packages/poryaaaa-m4l/package.json`
- Modify: `packages/poryaaaa-m4l/code-src/test/*`
- Modify or add harness support in: `packages/poryaaaa-m4l/code-src/recorder_smf_writer.ts`
- Modify or add shared-writer bridge fixtures under `packages/poryaaaa` only if needed.

- [ ] Add currently omitted recorder-focused TypeScript tests to `npm test`.
- [ ] Add fixture coverage for export range anchoring, first-note anchoring, loop markers, voicemap PC injection, initial CC replay, XCMD CC ordering, CC/PC dedupe, held-note flush, and M4L tick compensation.
- [ ] Define a parsed-SMF comparison helper that compares semantic structure and timing rather than requiring byte-identical output.
- [ ] Generate expected output through the current TypeScript writer first.
- [ ] Add a way to call the shared C++ writer from the same fixture data, either through a small test binary or an external-owned command harness.
- [ ] Mark any intentional differences explicitly before changing M4L save behavior.
- [ ] Run `npm test` from `packages/poryaaaa-m4l`.

## Task 8: Add Shared Writer Surface For M4L

**Files:**
- Modify: `packages/poryaaaa/plugin/recorder/smf_writer.h`
- Modify: `packages/poryaaaa/plugin/recorder/smf_writer.cpp`
- Modify: `packages/poryaaaa/plugin/m4a_engine_recorder.h`
- Modify: `packages/poryaaaa/plugin/m4a_engine_recorder.cpp`
- Modify: `packages/poryaaaa-m4l/source/audio/poryaaaa~/CMakeLists.txt`
- Modify: `packages/poryaaaa-m4l/source/audio/poryaaaa~/poryaaaa~.cpp`

- [ ] Add a C or C++ writer API that accepts beat-stamped MIDI events plus save-time metadata and writes a complete SMF.
- [ ] Link the poryaaaa-m4l external against the shared poryaaaa recorder/writer instead of the wrapper-local writer implementation.
- [ ] Keep M4L JavaScript temporarily responsible for Live API queries, export range parsing, loop marker parsing, output path management, ccomidi state collection, and status messages.
- [ ] Preserve the M4L `beats <float>` message as the host timing adapter into the shared recorder model.
- [ ] Add external status replies that mirror current save success/failure behavior.
- [ ] Run the poryaaaa unit tests.
- [ ] Run the poryaaaa-m4l external build check from `packages/poryaaaa-m4l`.

## Task 9: Port poryaaaa-m4l Save Flow To Shared Writer

**Files:**
- Modify: `packages/poryaaaa-m4l/code-src/ccomidi_recorder.ts`
- Modify: `packages/poryaaaa-m4l/source/audio/poryaaaa~/poryaaaa~.cpp`
- Modify: `packages/poryaaaa-m4l/source/audio/poryaaaa~/recorder/midi_buffer.h`
- Modify: `packages/poryaaaa-m4l/source/audio/poryaaaa~/recorder/export_capture_tests.cpp`
- Modify only after parity: `packages/poryaaaa-m4l/code-src/recorder_smf_writer.ts`

- [ ] Replace the M4L `dump <path>` / PRBY / TypeScript byte-rendering path with an external-owned `save` or `write_smf` command.
- [ ] Keep `ccomidi_recorder.ts` as the Live metadata adapter until every remaining responsibility has a non-JS owner.
- [ ] Add a guard or sanitizer for non-monotonic beat snapshots before first-note anchor selection.
- [ ] Fix the PRBY magic-byte comment to match ASCII file bytes if the PRBY path still exists at this stage.
- [ ] Decide whether explicit marker fields remain the only loop-marker source, or whether Live loop state should generate markers by default.
- [ ] Keep the constant-tempo limitation documented until tempo automation is implemented.
- [ ] Run the M4L parity harness before and after the save-flow change.
- [ ] Run `npm test` from `packages/poryaaaa-m4l`.

## Task 10: Retire M4L Shadow Writer/Recorder Pieces

**Files:**
- Delete when proven redundant: `packages/poryaaaa-m4l/code-src/recorder_smf_writer.ts`
- Delete or shrink when proven redundant: `packages/poryaaaa-m4l/source/audio/poryaaaa~/recorder/`
- Modify: `packages/poryaaaa-m4l/package.json`
- Modify: `packages/poryaaaa-m4l/source/audio/poryaaaa~/CMakeLists.txt`

- [ ] Delete `recorder_smf_writer.ts` only after behavioral parity tests prove the external-written SMF matches the current TypeScript writer for covered cases.
- [ ] Remove wrapper-local recorder shadowing from poryaaaa-m4l CMake once the shared poryaaaa recorder covers the M4L behavior.
- [ ] If the goal is to remove `ccomidi_recorder.ts` entirely, move its remaining responsibilities into Max patch wiring or external messages before deleting it.
- [ ] Remove obsolete tests only after equivalent shared-writer coverage exists.
- [ ] Run `npm test` from `packages/poryaaaa-m4l`.
- [ ] Run the poryaaaa-m4l external build check.

## Completion Criteria

- poryaaaa recorder baseline is covered by unit tests and uses a named 96 PPQ default.
- poryaaaa exports beat-stamped recorder SMFs with stable note/controller timing and no negative deltas.
- mid2agb-facing timing constraints are covered by tests or explicitly documented where the shared writer intentionally keeps finer 96 PPQ note placement.
- Exporting after a long silent lead-in does not generate an erroneously large `.s`.
- Same-tick CC/PC cleanup reduces noise without corrupting GBA extended-command pairs.
- Held-note flush handles overlapping same-pitch notes correctly.
- Tempo export policy is explicit and tested.
- The M4L parity harness compares current TypeScript writer output to shared C++ writer output from the same fixture data.
- poryaaaa-m4l can use poryaaaa's shared recorder/writer, with final SMF bytes generated by the external instead of TypeScript.
- Any remaining poryaaaa-m4l recorder JavaScript is limited to host/UI metadata plumbing, or has been replaced by Max patch/external message plumbing.
- Validation passes for touched packages: `cmake --build build --target poryaaaa_unit_tests`, `./build/poryaaaa_unit_tests`, and the relevant `packages/poryaaaa-m4l` test/build commands.
- The recorder mid2agb validation test either passes with local mid2agb or skips with a clear message when mid2agb is unavailable.
