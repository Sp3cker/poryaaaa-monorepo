# recorder integration plan

## Context

The recorder (`RecorderCore` + `smf_writer`) originated as a separate
`ccomidi_recorder.clap` plugin in the sibling `ccomidi` repo
(`/Users/spencer/dev/cProjects/ccomidi/`). ccomidi commit `e51e1af`
deleted those sources claiming they had "moved to ../poryaaaa/", but
that migration never actually landed in the poryaaaa repo on
this machine. The only canonical copy still alive was inside the M4L
wrapper at
`/Users/spencer/dev/maxProjects/poryaaaa-m4l/source/audio/poryaaaa~/recorder/`.

Goal: integrate the recorder directly into `poryaaaa.clap` so users no
longer chain `ccomidi_recorder.clap` in front of `poryaaaa.clap`. Once
the engine exposes the recorder, the M4L wrapper picks it up for free
through the engine API and the wrapper-side vendored copy can be
deleted.

Current state: vendoring + build wiring + GUI tab seam are scaffolded.
No engine API, no MIDI capture wiring, no functional GUI yet.

## What's scaffolded this turn

Files added:

- `plugin/recorder/recorder_core.h`     (vendored verbatim, `namespace ccomidi`)
- `plugin/recorder/recorder_core.cpp`   (vendored verbatim)
- `plugin/recorder/smf_writer.h`        (vendored verbatim)
- `plugin/recorder/smf_writer.cpp`      (vendored verbatim)
- `plugin/recorder/CMakeLists.txt`      (static lib `recorder`; PUBLIC include = `plugin/`; PRIVATE include = midifile headers; compiles 2 recorder TUs + 6 midifile TUs)
- `third_party/midifile/`               (vendored verbatim from ccomidi's working tree; src/, include/, LICENSE, README, plus build junk that we ignore)
- `recorder integration plan.md`        (this file)

Files modified:

- `CMakeLists.txt`
  - Added `add_subdirectory(plugin/recorder)` after the voicegroup subdir.
  - Appended `recorder` to `target_link_libraries(poryaaaa PRIVATE ...)`.
  - Did **not** link recorder into `poryaaaa_test`, `poryaaaa_render`,
    or `poryaaaa_unit_tests`. Those don't process MIDI and have no
    use for it.
- `plugin/m4a_gui.cpp`
  - Added `static void render_recorder_tab(M4AGuiState *)` with a
    placeholder message (one function, ~5 LOC).
  - Added the matching `BeginTabItem("Recorder")` block in the TabBar
    after the Voices tab (one if-block, 4 LOC).
  - No other changes.

What is **not** scaffolded:

- No engine field, no engine accessor, no MIDI tap, no clock anchor.
- `m4a_plugin.c`, `m4a_engine.{h,c}`, `m4a_channel.{h,c}`, `m4a_tables.{h,c}`, `m4a_reverb.{h,c}`, `m4a_params.{h,c}` are all untouched.
- Recorder is linked into the binaries but **nothing calls it yet**.
  The linker will keep the symbols available via the static lib; if
  link-time DCE strips them, that's fine — the next turn adds real
  call sites.

## What stays for integration

1. **Engine API** — expose the engine-owned `RecorderCore` to consumers:

   - Engine adds a `ccomidi::RecorderCore recorder_;` (or pointer) field
     to `M4AEngine`.
   - Header gets a single accessor:
     `m4a_engine_recorder(M4AEngine *eng) -> ccomidi::RecorderCore *`.
     `m4a_engine.h` is C-facing today; this requires either a forward
     decl + `extern "C"` shim or an opaque `void *` accessor with a
     C++-only inline cast wrapper. **Open: pick one.**
   - Tradeoff vs. flat C facade (`m4a_engine_recorder_push_event`,
     `..._set_tempo`, `..._snapshot`, `..._reset`, `..._arm`, `..._save_smf`):
     more surface, more glue, but keeps `m4a_engine.h` strictly C. M4L
     wrapper already speaks C++ so the opaque-pointer approach is
     cheaper there; `m4a_plugin.c` only needs push/tempo/advance in
     the audio thread, so the flat facade for those three is also fine.

2. **MIDI tap site** — `process_midi_event` and
   `process_clap_note_event` in `m4a_plugin.c`. After dispatching the
   event to the engine, call
   `recorder.push_event_in_block(sample_in_block, status, d1, d2)`
   gated on an `arm` flag. CLAP `note_on`/`note_off` events have to be
   reconstructed into status bytes (0x90 / 0x80 + channel).

3. **Sample-clock anchor** — mirror the M4L wrapper:
   - Atomic `audioSamplesElapsed` counter (or just per-block ticking
     inside `RecorderCore::advance_block`, which already exists).
   - `captureStartSamples` snapshotted on arm rising edge — owned by
     the engine, not the recorder.
   - At top of CLAP `process()`: `recorder.advance_block(frames_count)`
     after processing? Actually the existing API expects
     `push_event_in_block(sampleInBlock, ...)` then `advance_block` at
     end — keep that order.

4. **Tempo source** — CLAP provides transport in
   `clap_process_t::transport`. On any tempo change pulse,
   `recorder.set_tempo_in_block(0, transport->tempo)`. Loop info:
   `recorder.update_loop_from_transport(...)`.

5. **GUI tab body** (`render_recorder_tab` in `m4a_gui.cpp`) — feature
   parity with the M4L wrapper's UI:
   - Arm/disarm toggle (writes engine flag via param or direct setter).
   - Clear button → `recorder.reset()` via engine API.
   - Filename input (`InputText`, persisted via CLAP state).
   - Save SMF button → call `write_smf1(path, recorder.snapshot(), opts)`.
   - Status line: "Buffered: N events (T.Ts)" from
     `midi_event_count()` and `duration_seconds()`.

6. **CLAP state save/restore** — recorder filename and arm state
   serialised through the existing CLAP state extension.

7. **M4L wrapper migration** — once the engine accessor lands and
   `poryaaaa~.cpp` switches to it:
   - Delete `poryaaaa-m4l/source/audio/poryaaaa~/recorder/` entirely.
   - Drop the wrapper-side `recorder` target from
     `poryaaaa-m4l`'s CMakeLists.
   - Wrapper-side tests under `source/audio/poryaaaa~/tests/` that
     hit RecorderCore directly may move into this repo if
     we want, but that's a follow-up.

## Open questions

- **Engine API shape**: opaque pointer + C++-friendly accessor, or
  flat C facade (`m4a_engine_recorder_push_event` etc.)?
  Probably opaque pointer — `m4a_plugin.c` can call
  `recorder->push_event_in_block(...)` if we provide a tiny
  `extern "C"` inline shim in a `m4a_engine_recorder.h` header, OR
  we keep `m4a_plugin.c` C and add three flat C wrappers (push, tempo,
  advance). Both are small.
- **Voicemap injection on save**: should `write_smf1` also embed the
  current voicegroup mapping as a meta event so the SMF round-trips?
  Out of scope this turn — flag for design discussion.
- **Arm flag ownership**: param vs. engine field vs. GUI-only? Param
  exposes it to host automation, which is probably what users want.
- **Sample rate sync**: engine knows sample rate; pass through to
  `recorder.set_sample_rate(...)` on activate.

## Hard guardrails for next turn

- Bottom-up changes only: add to the engine API, never break existing
  signatures.
- `m4a_engine.h` stays C-compatible (every existing consumer is C).
- No edits to `recorder_core.{h,cpp}` or `smf_writer.{h,cpp}` —
  treat them as vendored.
- No edits to vendored midifile — it's third_party and gets the same
  treatment as imgui/clap-sdk.
- Per-block ordering must be: (push events with offsets) →
  `advance_block(frames)`. Don't advance first; the recorder's
  sample-time stamping depends on it.
