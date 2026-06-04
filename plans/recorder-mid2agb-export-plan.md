# Recorder mid2agb Export Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make poryaaaa recorder exports reliably produce compact, mid2agb-friendly SMF files.

**Architecture:** Make poryaaaa's `plugin/recorder/` module the shared recorder implementation for both CLAP and poryaaaa-m4l. Borrow the good export properties from poryaaaa-m4l: beat-relative export, 96 PPQ, stable per-channel sorting, event coalescing, and explicit mid2agb validation. Treat poryaaaa-m4l's recorder/export path as the better current reference, but not as a complete correctness oracle.

**Tech Stack:** C++23 recorder code under `plugin/recorder/`, C CLAP integration under `plugin/m4a_plugin.c`, SMF validation via local `../ccomidi/mid2agb`, and poryaaaa unit tests under `test/`.

---

## Current Answer

Yes: poryaaaa-m4l's recorder/export path is better for mid2agb than poryaaaa's current recorder.

The important distinction is that poryaaaa-m4l's C++ recorder is only the capture buffer. Its TypeScript SMF writer owns ticks, tempo, loop markers, state replay, validation, and final MIDI bytes. That split currently gives poryaaaa-m4l the better mid2agb-facing behavior.

## Shared Code Direction

The long-term goal is to share the recorder buffer and writer policy between poryaaaa and poryaaaa-m4l.

poryaaaa should own the shared C++ recorder module under `plugin/recorder/`. poryaaaa-m4l should stop shadowing that recorder with wrapper-local C++ once the shared module supports beat-stamped capture and the M4L export behavior has been ported.

Each host should only provide a timing adapter:

- M4L source: `[plugsync~]` outlet 6 sends `beats <float>` into `poryaaaa~`; captured events use the latest latched beat value.
- CLAP source: when `process->transport` has `CLAP_TRANSPORT_HAS_BEATS_TIMELINE`, use `song_pos_beats` as the block-start beat position.
- CLAP event conversion: `event_beats = block_start_beats + sample_in_block * tempo / (60.0 * sample_rate)`.
- CLAP fallback: if the host does not provide beats, keep the current sample-time plus tempo path.

Recommended sharing path:

1. Share the buffer/time model first by adding optional beat stamps to poryaaaa's recorder events.
2. Port poryaaaa-m4l's proven SMF writer policy into poryaaaa's C++ writer.
3. Keep poryaaaa-m4l JavaScript responsible for Live API queries, save orchestration, filename/range/marker UI, and status reporting.
4. Replace poryaaaa-m4l's TypeScript SMF writer with calls into the shared C++ writer only after poryaaaa's C++ writer matches the current M4L behavior under tests.

## Evidence

### Why poryaaaa-m4l is better today

- `poryaaaa-m4l/code-src/recorder_smf_writer.ts` writes on a fixed `PPQ = 96` grid and writes the SMF header with the same `ticksPerBeat`.
- Captured events are beat-stamped, then converted with `Math.round((beats - anchor) * PPQ)`, so export timing is song/grid-oriented instead of host-sample-duration-oriented.
- The writer can rebase tick 0 to an explicit export range or to the first real note, which avoids large empty lead-in regions.
- It does stable per-channel sorting before byte emission.
- It coalesces same-tick CC/PC noise and preserves ordered GBA extended-command CC pairs.
- It flushes held notes and can replay PC/CC state at loop boundaries.
- It already has focused TypeScript tests for SMF tick placement, save flow, CC dedupe, PRBY parsing, and writer quirks.

### Why poryaaaa is worse today

- `plugin/m4a_gui.cpp` saves with `ppq = 480`, not 96.
- `plugin/recorder/smf_writer.cpp` converts raw sample time to ticks using seconds, BPM, and PPQ, with no mid2agb-specific grid quantization.
- `plugin/m4a_plugin.c` advances the recorder sample clock every block, even when recording is disarmed, so disarmed time becomes exported empty MIDI time.
- Host tempo is sampled every process block and deduped only by exact double equality in recorder state.
- Tempo events and MIDI events are converted using the current BPM at each event rather than integrating elapsed ticks across a tempo map.
- Music tracks are intentionally not sorted because the current dependency's sorter corrupts GBA extended-command CC ordering.
- Held notes are tracked with a set keyed by channel and pitch, so overlapping same-pitch notes on one channel are not represented as separate note lifetimes.

### Why poryaaaa-m4l is not perfect

- `poryaaaa-m4l/package.json` omits several recorder tests from the default `npm test` script.
- `poryaaaa-m4l/source/audio/poryaaaa~/recorder/export_capture_tests.cpp` accepts non-monotonic beat snapshots; the JS writer later sorts events, but first-note anchor selection still uses event order.
- Tempo automation is explicitly unsupported in poryaaaa-m4l's SMF writer v0.
- Live loop state is ignored for SMF loop markers unless explicit marker fields are supplied.
- `midi_buffer.h` has a stale PRBY magic-byte comment, although implementation and JS parsing use the correct ASCII bytes.

## mid2agb Constraint

The local mid2agb source at `../ccomidi/mid2agb` scales MIDI ticks like this:

- event time: `(24 * g_clocksPerBeat * event.time) / g_midiTimeDiv`
- note duration: `(24 * g_clocksPerBeat * event.param2) / g_midiTimeDiv`
- default `g_clocksPerBeat = 1`, so the default target is 24 clocks per quarter note
- `-X` changes this to 48 clocks per quarter note

For a 96 PPQ SMF at default mid2agb settings, meaningful event starts and durations should land on multiples of 4 MIDI ticks to avoid integer truncation. A 96 PPQ export is still the right base because it matches poryaaaa-m4l and Live display expectations, but poryaaaa should add an explicit "mid2agb grid" quantization policy instead of merely changing the header.

## File Responsibilities

- `plugin/recorder/smf_writer.cpp`: SMF tick conversion, sorting, state replay, event coalescing, and held-note flush.
- `plugin/recorder/smf_writer.h`: options for PPQ, quantization mode, and export anchoring.
- `plugin/recorder/recorder_core.cpp`: captured recorder state, sample clock, tempo dedupe, and snapshot shape.
- `plugin/recorder/recorder_core.h`: snapshot/options API exposed to GUI and plugin code.
- `plugin/m4a_engine_recorder.cpp` and `plugin/m4a_engine_recorder.h`: C API bridge from CLAP/engine code into the C++ recorder.
- `plugin/m4a_gui.cpp`: Save SMF UI options and status text.
- `plugin/m4a_plugin.c`: process-time recorder advancement, CLAP beat timeline capture, and transport tempo/loop capture.
- `test/test_recorder_mid2agb.cpp` or an equivalent focused test file: recorder SMF structural tests and mid2agb conversion regression tests.
- `CMakeLists.txt`: register the focused recorder test target if a new test file is added.
- `/Users/sallegrezza/dev/cProjects/poryaaaa-m4l/source/audio/poryaaaa~/CMakeLists.txt`: eventually stop shadowing poryaaaa's recorder C++ and link or include the shared module instead.
- `/Users/sallegrezza/dev/cProjects/poryaaaa-m4l/code-src/ccomidi_recorder.ts`: keep Live API/save orchestration, but stop owning final SMF writer policy after the shared C++ writer catches up.
- `/Users/sallegrezza/dev/cProjects/poryaaaa-m4l/code-src/recorder_smf_writer.ts`: current behavior reference and eventual deletion/retirement target.

## Task 1: Lock In mid2agb Export Tests

**Files:**
- Create: `test/test_recorder_mid2agb.cpp`
- Modify: `CMakeLists.txt`

- [ ] Add a test helper that writes a short recorder snapshot to a temporary `.mid`.
- [ ] Parse the generated SMF header and assert `ticksPerBeat == 96`.
- [ ] Add a one-note test at beat 0 with a one-quarter duration and assert all emitted music-track deltas are non-negative.
- [ ] Add a quantization test where near-grid events snap to the expected 96 PPQ tick values.
- [ ] Add a mid2agb scaling test that validates exported note starts and durations land on multiples of 4 MIDI ticks for default 24-clock mid2agb mode.
- [ ] Add a dense CC/PC test that proves repeated same-tick state events are coalesced except for ordered GBA extended-command CC pairs `0x1E` then `0x1D`.
- [ ] Add a same-pitch overlap test that documents the current failure or expected new behavior before changing held-note tracking.
- [ ] Register the test target in `CMakeLists.txt`.
- [ ] Run `cmake --build build --target poryaaaa_unit_tests`.
- [ ] Run `./build/poryaaaa_unit_tests`.
- [ ] Run the new recorder test target directly.

## Task 2: Add Beat-Stamps To The Shared Recorder Model

**Files:**
- Modify: `plugin/recorder/recorder_core.h`
- Modify: `plugin/recorder/recorder_core.cpp`
- Modify: `plugin/m4a_engine_recorder.h`
- Modify: `plugin/m4a_engine_recorder.cpp`
- Modify: `plugin/m4a_plugin.c`
- Modify: `test/test_recorder_mid2agb.cpp`

- [ ] Extend the recorder event model so each MIDI event can carry either a host beat stamp or the current sample-time stamp.
- [ ] Add a C API entry point that can push a MIDI event with a precomputed beat stamp.
- [ ] In `plugin/m4a_plugin.c`, read `CLAP_TRANSPORT_HAS_BEATS_TIMELINE` and convert `song_pos_beats` from CLAP fixed point into double beats.
- [ ] Compute per-event beat positions with `event_beats = block_start_beats + sample_in_block * tempo / (60.0 * sample_rate)` when both beats and tempo are available.
- [ ] Preserve the sample-time path when the CLAP host does not provide a beat timeline.
- [ ] Prefer `loop_start_beats` and `loop_end_beats` for recorder loop metadata when the CLAP host provides beat timeline loop data; fall back to the existing seconds loop path otherwise.
- [ ] Add tests proving beat-stamped events round to expected 96 PPQ ticks without depending on sample rate.
- [ ] Add tests proving the sample-time fallback still works when no host beat timeline is present.
- [ ] Run `cmake --build build --target poryaaaa_unit_tests`.
- [ ] Run `./build/poryaaaa_unit_tests`.

## Task 3: Switch poryaaaa SMF Exports To 96 PPQ

**Files:**
- Modify: `plugin/m4a_gui.cpp`
- Modify: `plugin/recorder/smf_writer.cpp`
- Modify: `plugin/recorder/smf_writer.h`

- [ ] Change the default save PPQ from 480 to 96.
- [ ] Add a named default constant such as `kDefaultRecorderPpq = 96` in the recorder API instead of repeating the literal in GUI code.
- [ ] Keep `SmfWriteOptions.ppq` for tests and future compatibility, but clamp invalid values to the default constant.
- [ ] Re-run the header test and verify it fails before the implementation and passes after the implementation.
- [ ] Run `cmake --build build --target poryaaaa_unit_tests`.
- [ ] Run `./build/poryaaaa_unit_tests`.

## Task 4: Add mid2agb Grid Quantization

**Files:**
- Modify: `plugin/recorder/smf_writer.cpp`
- Modify: `plugin/recorder/smf_writer.h`
- Modify: `test/test_recorder_mid2agb.cpp`

- [ ] Add a writer option for mid2agb quantization using default 24 clocks per quarter note.
- [ ] For 96 PPQ and 24 clocks per quarter note, snap emitted event ticks and note durations to a 4-tick grid.
- [ ] Keep GBA extended-command CC pair ordering stable when both events land on the same quantized tick.
- [ ] Ensure quantization never produces a negative delta.
- [ ] Ensure note durations that would quantize to zero become one mid2agb clock, represented as 4 MIDI ticks at 96 PPQ.
- [ ] Run the quantization and scaling tests.
- [ ] Run `cmake --build build --target poryaaaa_unit_tests`.
- [ ] Run `./build/poryaaaa_unit_tests`.

## Task 5: Rebase Recording Time At Export

**Files:**
- Modify: `plugin/recorder/smf_writer.cpp`
- Modify: `plugin/recorder/smf_writer.h`
- Modify: `test/test_recorder_mid2agb.cpp`

- [ ] Add export anchoring equivalent to poryaaaa-m4l's first-note mode: tick 0 can correspond to the first real Note On.
- [ ] Preserve pre-anchor PC/CC state by clamping it to tick 0, so channel setup still survives.
- [ ] Keep explicit loop marker positions relative to the same anchor.
- [ ] Add a test where a long disarmed or silent period before the first note does not create a huge `.s` file.
- [ ] Add a test where pre-note PC/CC setup appears at tick 0.
- [ ] Run the new tests.
- [ ] Run `cmake --build build --target poryaaaa_unit_tests`.
- [ ] Run `./build/poryaaaa_unit_tests`.

## Task 6: Fix Tempo Handling Policy

**Files:**
- Modify: `plugin/recorder/recorder_core.cpp`
- Modify: `plugin/recorder/recorder_core.h`
- Modify: `plugin/recorder/smf_writer.cpp`
- Modify: `plugin/m4a_plugin.c`
- Modify: `test/test_recorder_mid2agb.cpp`

- [ ] Stop recording identical or near-identical host tempo samples every process block; use an epsilon or only record actual transport tempo changes.
- [ ] Choose one export policy for tempo automation: either integrate ticks across the tempo map correctly, or reject/flatten tempo automation with a clear status message.
- [ ] When beat-stamped recorder events are present, prefer beat stamps for event placement and use tempo only for conductor metadata.
- [ ] Add a constant-tempo test where sample-to-tick conversion is stable.
- [ ] Add a tempo-change test that captures the chosen policy.
- [ ] Run the tempo tests.
- [ ] Run `cmake --build build --target poryaaaa_unit_tests`.
- [ ] Run `./build/poryaaaa_unit_tests`.

## Task 7: Replace Dependence On Unsafely Sorting MidiFile Tracks

**Files:**
- Modify: `plugin/recorder/smf_writer.cpp`
- Modify: `test/test_recorder_mid2agb.cpp`

- [ ] Build per-channel pending descriptors before creating MidiFile events.
- [ ] Stable-sort descriptors by tick and insertion order.
- [ ] Coalesce same-tick CC/PC noise before writing MidiFile events.
- [ ] Preserve GBA extended-command CC pair order.
- [ ] Keep conductor sorting separate from music event ordering.
- [ ] Add a regression test that proves no negative deltas are emitted even when input recorder events arrive slightly out of order.
- [ ] Run the sorting/coalescing tests.
- [ ] Run `cmake --build build --target poryaaaa_unit_tests`.
- [ ] Run `./build/poryaaaa_unit_tests`.

## Task 8: Fix Held-Note Lifetime Tracking

**Files:**
- Modify: `plugin/recorder/smf_writer.cpp`
- Modify: `test/test_recorder_mid2agb.cpp`

- [ ] Replace the held-note set with a per-channel, per-pitch count or stack.
- [ ] Ensure overlapping same-pitch Note On events require matching Note Off events before the note is considered closed.
- [ ] Flush every still-open note at export end with deterministic order.
- [ ] Run the same-pitch overlap test.
- [ ] Run `cmake --build build --target poryaaaa_unit_tests`.
- [ ] Run `./build/poryaaaa_unit_tests`.

## Task 9: Add mid2agb Round-Trip Validation

**Files:**
- Modify: `test/test_recorder_mid2agb.cpp`
- Modify: `CMakeLists.txt`

- [ ] Detect local `../ccomidi/mid2agb/mid2agb` or build it as a test fixture when available.
- [ ] For generated recorder MIDI fixtures, run mid2agb and assert the output `.s` file is produced.
- [ ] Add a size guard for the `.s` output so obvious timing blowups fail the test.
- [ ] Skip gracefully when the local mid2agb executable is unavailable.
- [ ] Run the round-trip test with mid2agb present.
- [ ] Run `cmake --build build --target poryaaaa_unit_tests`.
- [ ] Run `./build/poryaaaa_unit_tests`.

## Task 10: Port poryaaaa-m4l To The Shared Recorder/Writer

**Files:**
- Modify in sibling repo: `/Users/sallegrezza/dev/cProjects/poryaaaa-m4l/package.json`
- Modify in sibling repo: `/Users/sallegrezza/dev/cProjects/poryaaaa-m4l/source/audio/poryaaaa~/CMakeLists.txt`
- Modify in sibling repo: `/Users/sallegrezza/dev/cProjects/poryaaaa-m4l/source/audio/poryaaaa~/poryaaaa~.cpp`
- Modify in sibling repo: `/Users/sallegrezza/dev/cProjects/poryaaaa-m4l/source/audio/poryaaaa~/recorder/midi_buffer.h`
- Modify in sibling repo: `/Users/sallegrezza/dev/cProjects/poryaaaa-m4l/source/audio/poryaaaa~/recorder/export_capture_tests.cpp`
- Modify in sibling repo: `/Users/sallegrezza/dev/cProjects/poryaaaa-m4l/code-src/recorder_smf_writer.ts`
- Modify in sibling repo: `/Users/sallegrezza/dev/cProjects/poryaaaa-m4l/code-src/ccomidi_recorder.ts`

- [ ] Add the omitted recorder-focused TypeScript tests to `npm test`.
- [ ] Add a default command or script that runs the C++ recorder CTest suite, not only the TypeScript tests.
- [ ] Add a guard or sanitizer for non-monotonic beat snapshots before first-note anchor selection.
- [ ] Fix the PRBY magic-byte comment to match ASCII file bytes.
- [ ] Decide whether explicit marker fields remain the only loop-marker source, or whether Live loop state should generate markers by default.
- [ ] Keep the constant-tempo limitation documented until tempo automation is implemented.
- [ ] Keep M4L JavaScript responsible for Live API queries, export range parsing, loop marker parsing, output path management, and status messages.
- [ ] Move final SMF byte generation from TypeScript to poryaaaa's shared C++ writer after behavioral parity tests pass.
- [ ] Remove wrapper-local recorder shadowing from poryaaaa-m4l's CMake once the shared poryaaaa recorder covers the M4L behavior.
- [ ] Keep the M4L `beats <float>` message as the host timing adapter into the shared recorder model.

## Completion Criteria

- poryaaaa's recorder can store host beat stamps when CLAP provides `CLAP_TRANSPORT_HAS_BEATS_TIMELINE`.
- poryaaaa still falls back to sample-time recorder export when the CLAP host does not provide a beats timeline.
- poryaaaa writes recorder SMFs with a 96 PPQ header.
- Exported note starts and durations are quantized for default mid2agb conversion.
- Exporting after a long silent/disarmed lead-in does not generate an erroneously large `.s`.
- Same-tick CC/PC cleanup reduces noise without corrupting GBA extended-command pairs.
- Held-note flush handles overlapping same-pitch notes correctly.
- poryaaaa-m4l can use poryaaaa's shared recorder/writer while keeping JS-owned Live API and save orchestration.
- Unit tests pass: `cmake --build build --target poryaaaa_unit_tests` and `./build/poryaaaa_unit_tests`.
- The recorder mid2agb validation test either passes with local mid2agb or skips with a clear message when mid2agb is unavailable.
