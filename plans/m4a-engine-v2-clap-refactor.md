# M4AEngine v2 Refactor - CLAP First

## Goal

Refactor the existing `M4AEngine` API so the CLAP plugin uses the v2
`M4ADriver` + `HwAudio` path through `m4a_engine_*`, without preserving the v1
engine implementation or adding a new runtime abstraction.

This pass is CLAP-first. Max external and renderer migration can follow after
the CLAP target builds and tests pass.

## Settled Decisions

- Keep `M4AEngine` as the shared public engine API. Do not add `M4ARuntime` or
  another wrapper layer.
- Keep `M4AEngine` stack-allocatable:
  - callers keep using `M4AEngine engine;`
  - lifecycle remains `m4a_engine_init(&engine, sr)` and
    `m4a_engine_destroy(&engine)`
- Change `m4a_engine_init()` to return `bool`.
- `M4AEngine` internally owns:
  - `M4ADriver *driver`
  - `HwAudio *hw`
- `M4AEngine` keeps `ToneData *voiceGroup` as a public pointer.
- Remove recorder ownership from `M4AEngine`.
- Remove v1 tracks/channels/reverb from `M4AEngine`.
- Remove v1 fallback behavior. `m4a_engine_process()` always uses v2.
- Remove `m4a_channel.*` and `m4a_reverb.*` from shipped CLAP builds once
  callers/tests stop depending on them.
- Keep the current event model:
  - plugin processes events at their CLAP sample offsets
  - plugin renders spans between events
  - `M4AEngine` does not own a timestamped event queue
- CLAP is our caller and is responsible for chunking render spans correctly.
- Re-export the process chunk limit from `m4a_engine.h`:
  - `#define M4A_ENGINE_MAX_PROCESS_FRAMES 2048`
  - guard against drift with a static assertion in `m4a_engine.c` against
    `M4A_RECOMMENDED_MAX_ADVANCE_FRAMES`
- `m4a_engine_process()` overwrites output buffers.
- No throw/assert/failure path in audio rendering.
- `m4a_engine_reset()` returns `bool` and is destroy+init. It preserves nothing
  except the sample rate needed to reinitialize.
- `m4a_engine_set_voicegroup()` assumes non-null input. Passing NULL is
  undefined behavior by contract. Callers must check before calling.
- `m4a_engine_set_voicegroup()` centralizes safe non-null swaps:
  - all sound off
  - set `engine->voiceGroup`
  - set v2 driver voicegroup
  - refresh v2 track voices
- Program changes before a voicegroup is set are no-ops.
- Out-of-range track indices remain no-ops.
- Keep v1 public constant names such as `MAX_TRACKS` for now.
- Keep `m4a_engine.h` light:
  - forward-declare `M4ADriver`
  - forward-declare `HwAudio`
  - tests that need internals include v2 internal headers directly
- Expose always-compiled test/debug accessors:
  - `M4ADriver *m4a_engine_driver(M4AEngine *engine)`
  - `const M4ADriver *m4a_engine_driver_const(const M4AEngine *engine)`
  - `HwAudio *m4a_engine_hw_audio(M4AEngine *engine)`
  - `const HwAudio *m4a_engine_hw_audio_const(const M4AEngine *engine)`
- Keep `M4AEngineXcmdFn` as the public callback type. Adapt it internally to
  v2's driver callback.
- Remove `m4a_engine_tick()` after tests migrate.
- Keep temporary compatibility alias:
  - `m4a_engine_set_song_volume()` calls `m4a_engine_set_volume()`

## Product Control Surface

Use one user-facing volume:

- `m4a_engine_set_volume(M4AEngine *engine, uint8_t volume)`
- default volume is `127`
- old user-facing `masterVolume` and `songMasterVolume` concepts are removed
- v2 internal master volume can stay fixed at `15`

Reverb remains user-settable:

- `m4a_engine_set_reverb_amount(M4AEngine *engine, uint8_t amount)`
- default reverb is `0`
- changing reverb amount does not clear the reverb buffer

These remain API hooks but are not normal CLAP/config UI state:

- `m4a_engine_set_max_pcm_channels(M4AEngine *engine, uint8_t max_channels)`
  - default is `12`
- `m4a_engine_set_analog_filter(M4AEngine *engine, bool enabled)`
  - default is `false`

## CLAP Scope For This Pass

Update only the CLAP-facing path enough that `poryaaaa.clap` no longer
dual-drives v1 and v2.

Primary files:

- `plugin/m4a_engine.h`
- `plugin/m4a_engine.c`
- `plugin/m4a_engine_recorder.h`
- `plugin/m4a_engine_recorder.cpp`
- `plugin/m4a_plugin.h`
- `plugin/m4a_plugin.c`
- `plugin/m4a_params.c`
- `plugin/m4a_gui.cpp` if needed for removed controls/state
- `CMakeLists.txt`
- `test/test_engine.c`

Do not migrate the Max external or renderer in this pass unless the build
requires a small mechanical compatibility update. Leave a follow-up note for
those targets.

## Phase 1 - Reshape `M4AEngine`

Change `plugin/m4a_engine.h`:

- Include `<stdbool.h>`.
- Forward-declare `M4ADriver` and `HwAudio`.
- Define `M4A_ENGINE_MAX_PROCESS_FRAMES 2048`.
- Replace the v1-heavy struct with the new public shape:

```c
struct M4AEngine {
	ToneData *voiceGroup;
	M4ADriver *driver;
	HwAudio *hw;
	float sampleRate;
	uint8_t volume;
	uint8_t reverbAmount;
	M4AEngineXcmdFn xcmd_fn;
	void *xcmd_ctx;
};
```

- Keep `MAX_TRACKS` public, mapped to the v2 track count.
- Add public setters:
  - `bool m4a_engine_init(M4AEngine *engine, float sampleRate);`
  - `bool m4a_engine_reset(M4AEngine *engine);`
  - `void m4a_engine_set_volume(M4AEngine *engine, uint8_t volume);`
  - `void m4a_engine_set_reverb_amount(M4AEngine *engine, uint8_t amount);`
  - `void m4a_engine_set_max_pcm_channels(M4AEngine *engine, uint8_t max_channels);`
  - `void m4a_engine_set_analog_filter(M4AEngine *engine, bool enabled);`
- Keep `m4a_engine_set_song_volume()` as a temporary alias.
- Add test/debug accessors for driver and hw.
- Remove declarations tied only to v1 internals after tests migrate:
  - `m4a_engine_lpf_reset()`
  - `m4a_engine_tick()`
  - `m4a_track_vol_pit_set()` from the public engine header if it only serves
    v1 tests.

## Phase 2 - Rewrite `m4a_engine.c` To Delegate To v2

Replace v1 implementation in `plugin/m4a_engine.c` with v2 delegation.

Implementation outline:

- Include v2 headers in `.c`, not public header:
  - `m4a/m4a_driver.h`
  - `m4a/m4a_internal.h` if needed for `M4A_MAX_TRACKS`
  - `hw_audio/hw_audio.h`
- Add static assertion:

```c
_Static_assert(M4A_ENGINE_MAX_PROCESS_FRAMES == M4A_RECOMMENDED_MAX_ADVANCE_FRAMES,
               "M4A_ENGINE_MAX_PROCESS_FRAMES must match v2 driver queue limit");
```

- `m4a_engine_init()`:
  - `memset(engine, 0, sizeof(*engine))`
  - set `sampleRate`
  - set `volume = 127`
  - set `reverbAmount = 0`
  - allocate driver with `m4a_driver_create(sampleRate)`
  - allocate chip with `hw_audio_create(sampleRate)`
  - on allocation failure, destroy any partial state, zero engine, return false
  - set defaults:
    - `m4a_set_master_volume(driver, 15)`
    - `m4a_set_song_volume(driver, 127)`
    - `m4a_set_reverb_amount(driver, 0)`
    - `m4a_set_max_pcm_channels(driver, 12)`
    - `m4a_set_analog_filter(driver, false)`
- `m4a_engine_destroy()`:
  - tolerate null/zeroed fields
  - destroy driver and hw
  - zero the struct
- `m4a_engine_reset()`:
  - save `sampleRate`
  - destroy
  - init with saved sample rate
  - return init result
- `m4a_engine_set_xcmd_callback()`:
  - store public callback/ctx on engine
  - install an internal adapter into `m4a_driver_set_xcmd_callback()`
- MIDI/event methods forward to v2 methods:
  - `m4a_note_on`
  - `m4a_note_off`
  - `m4a_program_change`
  - `m4a_cc`
  - `m4a_pitch_bend`
  - `m4a_all_notes_off`
  - `m4a_all_sound_off`
- `m4a_engine_program_change()` no-ops if `engine->voiceGroup` is null.
- `m4a_engine_note_on()` no-ops if `engine->voiceGroup` is null.
- `m4a_engine_set_voicegroup()` assumes non-null:
  - `m4a_engine_all_sound_off(engine)`
  - set `engine->voiceGroup`
  - `m4a_driver_set_voicegroup(engine->driver, voiceGroup)`
  - `m4a_driver_refresh_voices(engine->driver)`
- `m4a_engine_process()`:
  - one v2 render/consume cycle
  - assumes caller chunked to `M4A_ENGINE_MAX_PROCESS_FRAMES`
  - overwrites `outL/outR`
  - calls:
    - `m4a_advance(engine->driver, frames)`
    - `hw_audio_render_events(engine->hw, m4a_get_pending_writes(...), m4a_get_pcm_ring(...), outL, outR, frames)`
    - `m4a_consume_writes(engine->driver)`

## Phase 3 - Remove Recorder From Engine

Recorder ownership has already moved toward `M4APluginData`. Finish the split:

- Remove `void *recorder` from `M4AEngine`.
- Remove legacy `m4a_engine_recorder_*` wrappers.
- Keep the pure recorder API:
  - `M4ARecorder *m4a_recorder_create(void)`
  - `m4a_recorder_destroy`
  - `m4a_recorder_push`
  - `m4a_recorder_set_tempo`
  - `m4a_recorder_advance`
  - `m4a_recorder_reset`
  - `m4a_recorder_set_sample_rate`
  - `m4a_recorder_event_count`
  - `m4a_recorder_duration_seconds`
  - `m4a_recorder_update_loop`
  - `m4a_recorder_save_smf`
- Update includes so CLAP uses the recorder API directly.

## Phase 4 - Update CLAP Plugin Data

Change `plugin/m4a_plugin.h`:

- Keep `M4AEngine engine`.
- Keep `M4ARecorder *recorder`.
- Remove plugin-owned fields for removed controls:
  - `masterVolume`
  - `songMasterVolume`
  - `analogFilter`
  - `maxPcmChannels`
- Add/keep:
  - `uint8_t volume`
  - `uint8_t reverbAmount`
- Keep `programParams[MAX_TRACKS]`.

Change `plugin/m4a_plugin.c`:

- `plugin_init` defaults:
  - `volume = 127`
  - `reverbAmount = 0`
- `plugin_activate`:
  - check `m4a_engine_init(&data->engine, sample_rate)`
  - fail activation cleanly if init fails
  - set xcmd callback through `m4a_engine_set_xcmd_callback`
  - set volume and reverb through engine APIs
  - when a voicegroup loads, call `m4a_engine_set_voicegroup`
  - call `m4a_params_sync_to_engine(data)` afterward
- Remove direct v2 objects from `M4APluginData` if they are no longer needed:
  - `M4ADriver *m4a_v2`
  - `HwAudio *hw_v2`
- Remove dual dispatch:
  - call only `m4a_engine_note_on`
  - call only `m4a_engine_note_off`
  - call only `m4a_engine_program_change`
  - call only `m4a_engine_cc`
  - call only `m4a_engine_pitch_bend`
  - call only `m4a_engine_set_tempo_bpm`
- Keep the existing CLAP sample-accurate event loop.
- Keep chunking in `plugin_process`, but chunk against
  `M4A_ENGINE_MAX_PROCESS_FRAMES`.
- Replace the inline v2 render path with:

```c
m4a_engine_process(&data->engine, outL + off, outR + off, (int)chunk);
```

- `plugin_stop_processing()`:
  - call `m4a_engine_all_sound_off(&data->engine)`
- `plugin_reset()`:
  - call `m4a_engine_reset(&data->engine)`
  - if reset succeeds, reapply:
    - xcmd callback
    - volume
    - reverb
    - voicegroup if loaded
    - program params
    - tempo if needed
- Remove v1 calls:
  - direct `data->engine.masterVolume`
  - direct `data->engine.songMasterVolume`
  - direct `data->engine.analogFilter`
  - direct `data->engine.maxPcmChannels`
  - direct `data->engine.reverb`
  - `m4a_reverb_*`
  - `m4a_engine_lpf_reset`

## Phase 5 - Update CLAP Params And State

Change `plugin/m4a_params.c`:

- Keep program params as plugin-owned mirror.
- Replace direct voicegroup checks with `data->engine.voiceGroup`, since that
  remains public.
- Program replay uses `m4a_engine_program_change()` only.
- Remove direct v2 program calls.

Update CLAP state in `plugin/m4a_plugin.c`:

- Bump state version.
- Stop saving/restoring:
  - `masterVolume`
  - `songMasterVolume`
  - `analogFilter`
  - `maxPcmChannels`
- Keep saving/restoring:
  - project root
  - voicegroup name
  - `reverbAmount`
  - `volume`
  - program params
  - recorder path/armed state if still desired
- Old state compatibility for removed fields is intentionally dropped.

Update config parsing:

- Remove normal support for:
  - `master_volume`
  - `song_volume`
  - `analog_filter`
  - `max_pcm_channels`
- Keep support for:
  - project root
  - voicegroup
  - reverb
  - volume
  - standalone audio output if it still lives in this config path

## Phase 6 - Tests For CLAP-First Pass

Rewrite or delete v1-shaped tests in `test/test_engine.c`:

- `test_engine_init`
  - rewrite to check the new `M4AEngine` shape and v2-owned internals
- `test_xcmd_subcommands`
  - rewrite as an `M4AEngine` wrapper test, or rely on existing v2 XCMD tests
  - inspect state via `m4a_engine_driver(&engine)` when needed
- `test_basic_audio`
  - rewrite to use only `M4AEngine`
  - assert `m4a_engine_process()` produces nonzero output
- `test_polyphony_stealing`
  - rewrite against `M4ADriver` through `m4a_engine_driver()` if coverage is
    still needed
  - otherwise delete if existing v2 polyphony coverage is equivalent

Most later tests already target v2 directly and should not be rewritten unless
their compile assumptions break.

Mechanical test updates:

- Check `m4a_engine_init()` return values.
- Replace `m4a_engine_tick()` usage with process/advance semantics, then remove
  `m4a_engine_tick()`.
- Include v2 internal headers in tests that inspect driver internals.

## Phase 7 - Build System Cleanup For CLAP Target

Update `CMakeLists.txt` for the CLAP target:

- Remove v1-only sources from CLAP build once no longer referenced:
  - `plugin/m4a_channel.c`
  - `plugin/m4a_reverb.c`
- Keep v2 driver and hw audio sources.
- Keep recorder sources as plugin-owned recorder implementation, not engine
  ownership.
- Leave renderer/Max-specific build cleanup for follow-up if it is not needed
  to make CLAP and unit tests pass.

Do not edit vendor/submodule trees.

## Validation

Required for this CLAP-first pass:

```bash
cmake --build build --target poryaaaa_unit_tests
./build/poryaaaa_unit_tests
cmake --build build --target poryaaaa
git diff --check
```

If the CLAP target builds but standalone/renderer break because they still use
removed fields, make the smallest compatibility update needed or leave a clear
follow-up only if those targets are not built by the CLAP target.

## Known Follow-Ups

- Migrate the Max external to the same `m4a_engine_*` surface:
  - remove `analogfilter`
  - remove `maxpcm`
  - replace dual-volume naming with one `volume`
  - check `m4a_engine_init()` return
  - chunk against `M4A_ENGINE_MAX_PROCESS_FRAMES`
- Migrate `poryaaaa_render`:
  - remove `--solo`
  - remove max-channel option
  - remove analog-filter option
  - keep musical controls such as reverb
  - use `m4a_engine_process()` only
- Remove `M4A_DRIVER_V2` / `HW_AUDIO_V2` options after all shipped targets are
  unconditionally v2-backed.
- Delete `m4a_channel.*` and `m4a_reverb.*` from the repo once no tests or
  non-CLAP targets include them.
