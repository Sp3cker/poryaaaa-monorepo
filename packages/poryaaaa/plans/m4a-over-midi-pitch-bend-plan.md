# M4A-Over-MIDI Pitch Bend Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make live poryaaaa playback, poryaaaa recording, and later `mid2agb` export agree on pitch bend by treating MIDI pitch bend as an M4A-over-MIDI carrier.

**Architecture:** MIDI pitch bend `data2` is the M4A-centered bend byte. ccomidi emits `data2 = m4aBend + 64` for M4A bend units `-64..63`; poryaaaa live playback decodes `track->bend = data2 - 64`; poryaaaa recorder stores the original MIDI bytes unchanged so `mid2agb` can perform the same `data2 - 64` export conversion.

**Tech Stack:** C11/C23 poryaaaa engine, C++20 ccomidi, CLAP MIDI events, poryaaaa recorder SMF writer, `mid2agb` M4A export semantics.

---

## Context

The target ABI is M4A, not generic MIDI pitch bend.

Known export behavior:

- `packages/ccomidi/mid2agb/midi.cpp` reads pitch bend as two bytes and stores the second byte in `event.param2`.
- `packages/ccomidi/mid2agb/agb.cpp` emits `BEND c_v%+d` using `event.param2 - 64`.
- Therefore a recorded MIDI event with `0xE0, data1=0, data2=62` exports as `BEND c_v-2`.

Current live playback mismatch:

- `packages/poryaaaa/plugin/m4a_plugin.c` currently converts `0xE0` to signed 14-bit MIDI:
  ```c
  int16_t bend = ((int16_t)msg[2] << 7 | msg[1]) - 8192;
  m4a_engine_pitch_bend(&data->engine, channel, bend);
  ```
- `packages/poryaaaa/plugin/m4a/m4a_track.c` currently stores `bend >> 7`, which is generic MIDI-ish and lets `data1` affect audio.
- poryaaaa recorder currently pushes raw MIDI bytes after engine dispatch:
  ```c
  m4a_recorder_push_beats(data->recorder, recorder_beats, msg[0], msg[1], msg[2]);
  ```
  Keep that raw recorder behavior.

Reference already present in the monorepo:

- `packages/poryaaaa-m4l/source/midi/ccomidi/ccomidi_bend.cpp` already implements M4A bend-unit packing:
  ```c++
  const long value14 = 8192 + clamp_bend_units(bend) * 128;
  ```
- `packages/poryaaaa-m4l/source/midi/ccomidi/tests/test_ccomidi_parser.cpp` already verifies:
  - `-64 -> data2 0`
  - `0 -> data2 64`
  - `+63 -> data2 127`

Do not change recorder storage to subtract 64. That would double-apply the conversion when exported through `mid2agb`.

## Desired Contract

For M4A bend value `m4aBend`:

```text
ccomidi / Max source: data2 = clamp(m4aBend, -64, 63) + 64
poryaaaa live audio: track->bend = data2 - 64
poryaaaa recorder:   stores original 0xE0, data1, data2 unchanged
mid2agb export:      emits BEND c_v(data2 - 64)
engine pitch math:   effectiveBend = track->bend * track->bendRange
```

Examples:

```text
user bend -2,  BENDR 2  -> data2 62 -> track->bend -2  -> effective -4
user bend 0,   BENDR 2  -> data2 64 -> track->bend 0   -> effective 0
user bend -32, BENDR 2  -> data2 32 -> track->bend -32 -> effective -64
```

## File Map

- `packages/poryaaaa/plugin/m4a_plugin.c`: CLAP MIDI event dispatch for live plugin playback; decode `0xE0` using `msg[2] - 64`; leave recorder push raw.
- `packages/poryaaaa/plugin/m4a_engine.h`: update pitch-bend API comment/signature to say the argument is decoded M4A bend units.
- `packages/poryaaaa/plugin/m4a_engine.c`: forward decoded M4A bend units to the driver.
- `packages/poryaaaa/plugin/m4a/m4a_driver.h`: update driver pitch-bend signature/comment to M4A bend units.
- `packages/poryaaaa/plugin/m4a/m4a_track.c`: store decoded M4A bend units directly, no `>> 7`.
- `packages/poryaaaa/cmd/poryaaaa_render.c`: decode CLI-renderer MIDI pitch bend from MSB/data2 with `data2 - 64` before calling engine/driver.
- `packages/poryaaaa-m4l/source/audio/poryaaaa~/poryaaaa~.cpp`: decode Max external pitch bend from `d2 - 64` before calling engine/driver.
- `packages/poryaaaa/test/test_engine.c`: add focused engine tests for direct bend storage and `BENDR` multiplication.
- `packages/ccomidi/src/core/`: only add bend packing here if standalone ccomidi has a true M4A pitch-bend dial/control path. Do not alter raw MIDI pass-through.
- `packages/ccomidi/src/tests/test_sender_core.cpp`: add tests only if standalone ccomidi starts emitting M4A pitch bend from an internal dial/control.

---

### Task 1: Lock poryaaaa Driver Pitch Bend To M4A Units

**Files:**

- Modify: `packages/poryaaaa/plugin/m4a/m4a_driver.h`
- Modify: `packages/poryaaaa/plugin/m4a/m4a_track.c`
- Modify: `packages/poryaaaa/test/test_engine.c`

- [ ] **Step 1: Add a failing driver-level test**

Add this test near the other `M4A_DRIVER_V2` engine tests in `packages/poryaaaa/test/test_engine.c`:

```c
static void test_v2_pitch_bend_uses_m4a_units(void)
{
	printf("Testing v2 pitch bend M4A units...\n");

	M4ADriver *drv = m4a_driver_create(44100.0f);
	ASSERT(drv != NULL, "driver created");
	if (!drv) return;

	m4a_pitch_bend(drv, 0, -2);
	ASSERT_EQ(drv->tracks[0].bend, -2, "pitch bend stores decoded M4A units");
	ASSERT_EQ(drv->tracks[0].bendRange, 2, "default bend range remains 2");

	m4a_pitch_bend(drv, 0, -64);
	ASSERT_EQ(drv->tracks[0].bend, -64, "pitch bend accepts M4A minimum");

	m4a_pitch_bend(drv, 0, 63);
	ASSERT_EQ(drv->tracks[0].bend, 63, "pitch bend accepts M4A maximum");

	m4a_driver_destroy(drv);
}
```

Call it from `main()` inside the `#if defined(M4A_DRIVER_V2)` block:

```c
	test_v2_pitch_bend_uses_m4a_units();
```

- [ ] **Step 2: Run the focused poryaaaa test and verify it fails**

Run from `packages/poryaaaa`:

```bash
cmake --build build --target poryaaaa_unit_tests
./build/poryaaaa_unit_tests
```

Expected before implementation: the new test fails because `m4a_pitch_bend(drv, 0, -2)` stores `-1` or `0` depending on compiler shift behavior, not `-2`.

- [ ] **Step 3: Change the driver API to decoded M4A bend units**

In `packages/poryaaaa/plugin/m4a/m4a_driver.h`, replace:

```c
void m4a_pitch_bend(M4ADriver *drv, int track, int16_t bend);
```

with:

```c
/* Decoded M4A BEND units: -64..+63. BENDR multiplication happens in pitch math. */
void m4a_pitch_bend(M4ADriver *drv, int track, int8_t bend);
```

In `packages/poryaaaa/plugin/m4a/m4a_track.c`, replace the function with:

```c
void m4a_pitch_bend(M4ADriver *drv, int track, int8_t bend) {
    if (!drv) return;
    if (track < 0 || track >= M4A_MAX_TRACKS) return;
    M4ADriverTrack *t = &drv->tracks[track];
    t->bend = bend;
    m4a_trk_vol_pit_set(t);
    refresh_cgb_pitches(drv, track);
    refresh_pcm_pitches(drv, track);
}
```

Do not change `m4a_trk_vol_pit_set()` pitch math. It must keep:

```c
int32_t bend = (int32_t)track->bend * track->bendRange;
```

- [ ] **Step 4: Run the focused poryaaaa test and verify it passes**

Run from `packages/poryaaaa`:

```bash
cmake --build build --target poryaaaa_unit_tests
./build/poryaaaa_unit_tests
```

Expected: `test_v2_pitch_bend_uses_m4a_units` passes.

- [ ] **Step 5: Commit**

```bash
git add packages/poryaaaa/plugin/m4a/m4a_driver.h \
        packages/poryaaaa/plugin/m4a/m4a_track.c \
        packages/poryaaaa/test/test_engine.c
git commit -m "fix(poryaaaa): store pitch bend as M4A units"
```

---

### Task 2: Decode MIDI Pitch Bend At Every poryaaaa Ingress

**Files:**

- Modify: `packages/poryaaaa/plugin/m4a_engine.h`
- Modify: `packages/poryaaaa/plugin/m4a_engine.c`
- Modify: `packages/poryaaaa/plugin/m4a_plugin.c`
- Modify: `packages/poryaaaa/cmd/poryaaaa_render.c`
- Modify: `packages/poryaaaa-m4l/source/audio/poryaaaa~/poryaaaa~.cpp`

- [ ] **Step 1: Update engine API wording and type**

In `packages/poryaaaa/plugin/m4a_engine.h`, replace:

```c
void m4a_engine_pitch_bend(M4AEngine *engine, int trackIndex, int16_t bend);
```

with:

```c
/* Decoded M4A BEND units: -64..+63. MIDI carriers must be decoded by callers. */
void m4a_engine_pitch_bend(M4AEngine *engine, int trackIndex, int8_t bend);
```

In `packages/poryaaaa/plugin/m4a_engine.c`, replace:

```c
void m4a_engine_pitch_bend(M4AEngine *engine, int trackIndex, int16_t bend)
{
	if (trackIndex < 0 || trackIndex >= MAX_TRACKS)
		return;
	m4a_pitch_bend(engine->driver, trackIndex, bend);
}
```

with:

```c
void m4a_engine_pitch_bend(M4AEngine *engine, int trackIndex, int8_t bend)
{
	if (trackIndex < 0 || trackIndex >= MAX_TRACKS)
		return;
	m4a_pitch_bend(engine->driver, trackIndex, bend);
}
```

- [ ] **Step 2: Decode CLAP MIDI pitch bend from data2 only**

In `packages/poryaaaa/plugin/m4a_plugin.c`, replace the `0xE0` case with:

```c
    case 0xE0: /* Pitch Bend: M4A-over-MIDI carrier, data2 is c_v-centered. */
    {
        int8_t bend = (int8_t)((int)msg[2] - 64);
        m4a_engine_pitch_bend(&data->engine, channel, bend);
        break;
    }
```

Leave this recorder code unchanged:

```c
m4a_recorder_push_beats(data->recorder, recorder_beats, msg[0], msg[1], msg[2]);
```

- [ ] **Step 3: Decode CLI renderer MIDI pitch bend from MSB/data1 only**

In `packages/poryaaaa/cmd/poryaaaa_render.c`, replace the render-time pitch bend case:

```c
    case 0xE: /* Pitch Bend — convert MIDI 14-bit unsigned to signed -8192..+8191 */
    {
        int16_t bend = (int16_t)(((int)(ev->data1 << 7) | ev->data0) - 8192);
        m4a_engine_pitch_bend(engine, trackIdx, bend);
#if defined(M4A_DRIVER_V2)
        m4a_pitch_bend(g_v2_drv, trackIdx, bend);
#endif
        break;
    }
```

with:

```c
    case 0xE: /* Pitch Bend: M4A-over-MIDI carrier, data1 is c_v-centered MSB. */
    {
        int8_t bend = (int8_t)((int)ev->data1 - 64);
        m4a_engine_pitch_bend(engine, trackIdx, bend);
#if defined(M4A_DRIVER_V2)
        m4a_pitch_bend(g_v2_drv, trackIdx, bend);
#endif
        break;
    }
```

Do not change raw MIDI reading. `RawMidiEvent ev = { tick, chan, trackIndex, 0xE, data0, msb, 0 };` should remain raw.

- [ ] **Step 4: Decode poryaaaa-m4l external pitch bend from d2 only**

In `packages/poryaaaa-m4l/source/audio/poryaaaa~/poryaaaa~.cpp`, replace:

```c++
        case 0xE: {
            int16_t signed14 = (int16_t)(((d2 << 7) | d1) - 8192);
            m4a_engine_pitch_bend(&x->engine, ch, signed14);
#if defined(M4A_DRIVER_V2)
            m4a_pitch_bend(x->m4a_v2, ch, signed14);
#endif
            break;
        }
```

with:

```c++
        case 0xE: {
            int8_t bend = (int8_t)((int)d2 - 64);
            m4a_engine_pitch_bend(&x->engine, ch, bend);
#if defined(M4A_DRIVER_V2)
            m4a_pitch_bend(x->m4a_v2, ch, bend);
#endif
            break;
        }
```

- [ ] **Step 5: Build affected packages**

Run from `packages/poryaaaa`:

```bash
cmake --build build --target poryaaaa_unit_tests poryaaaa poryaaaa_render
./build/poryaaaa_unit_tests
```

Run the package-local M4L build/test command only if this checkout has its M4L build configured. If unavailable, report the exact reason.

- [ ] **Step 6: Commit**

```bash
git add packages/poryaaaa/plugin/m4a_engine.h \
        packages/poryaaaa/plugin/m4a_engine.c \
        packages/poryaaaa/plugin/m4a_plugin.c \
        packages/poryaaaa/cmd/poryaaaa_render.c \
        packages/poryaaaa-m4l/source/audio/poryaaaa~/poryaaaa~.cpp
git commit -m "fix(poryaaaa): decode M4A pitch bend carrier at ingress"
```

---

### Task 3: Preserve Raw Recorder Pitch Bend Bytes

**Files:**

- Modify: `packages/poryaaaa/test/test_recorder_core.cpp`
- Inspect only: `packages/poryaaaa/plugin/m4a_plugin.c`

- [ ] **Step 1: Add a recorder test that documents raw pitch bend storage**

Add this test in `packages/poryaaaa/test/test_recorder_core.cpp` near existing recorder bridge tests:

```c++
void test_recorder_bridge_preserves_pitch_bend_bytes()
{
	M4ARecorder *recorder = m4a_recorder_create();
	ASSERT(recorder != nullptr, "recorder bridge creates recorder for pitch bend test");
	if (!recorder)
		return;

	m4a_recorder_push_beats(recorder, 1.0, 0xE0, 0, 62);

	ccomidi::RecorderCore *core = m4a_recorder_core(recorder);
	ASSERT(core != nullptr, "recorder core is exposed");
	auto snapshot = core->snapshot();

	ASSERT(snapshot.midi.size() == 1, "recorder stores one pitch bend event");
	if (snapshot.midi.size() == 1) {
		ASSERT(snapshot.midi[0].status == 0xE0, "pitch bend status preserved");
		ASSERT(snapshot.midi[0].data1 == 0, "pitch bend LSB preserved");
		ASSERT(snapshot.midi[0].data2 == 62, "pitch bend MSB preserved without subtracting 64");
	}

	m4a_recorder_destroy(recorder);
}
```

Call it from `test_recorder_core_run_all()`:

```c++
	test_recorder_bridge_preserves_pitch_bend_bytes();
```

- [ ] **Step 2: Run recorder/unit tests**

Run from `packages/poryaaaa`:

```bash
cmake --build build --target poryaaaa_unit_tests
./build/poryaaaa_unit_tests
```

Expected: test passes without modifying recorder code. If it fails, inspect the recorder bridge before changing production code.

- [ ] **Step 3: Confirm plugin recorder path is still raw**

Inspect `packages/poryaaaa/plugin/m4a_plugin.c` and verify this remains after the `switch`:

```c
m4a_recorder_push_beats(data->recorder, recorder_beats, msg[0], msg[1], msg[2]);
```

No implementation change should be needed for this task.

- [ ] **Step 4: Commit**

```bash
git add packages/poryaaaa/test/test_recorder_core.cpp
git commit -m "test(poryaaaa): document raw recorder pitch bend storage"
```

---

### Task 4: Add Or Verify ccomidi Bend Packing

**Files:**

- Inspect: `packages/ccomidi/src/plugin/ccomidi_plugin.cpp`
- Inspect: `packages/ccomidi/src/core/sender_core.cpp`
- Optional create: `packages/ccomidi/src/core/bend_encoding.h`
- Optional create: `packages/ccomidi/src/core/bend_encoding.cpp`
- Optional modify: `packages/ccomidi/CMakeLists.txt`
- Optional modify: `packages/ccomidi/src/tests/test_sender_core.cpp`

This task is required only if standalone `packages/ccomidi` has, or is about to gain, a UI/control path whose value is a true M4A pitch-bend dial. Do not change raw incoming MIDI pass-through. A host-provided `0xE0` event that already contains `data2 = m4aBend + 64` should pass through unchanged.

- [ ] **Step 1: Inspect whether standalone ccomidi has an internal pitch-bend command**

Run from monorepo root:

```bash
rg -n "Pitch|Bend|0xE0|CommandType::" packages/ccomidi/src
```

Expected current state: `packages/ccomidi/src/plugin/ccomidi_plugin.cpp` forwards incoming `0xE0`; `packages/ccomidi/src/core/command_spec.h` has `BendRange`, not a true `PitchBend` command.

If there is no internal pitch-bend dial/control path, skip the remaining implementation steps and record that ccomidi raw pass-through is correct.

- [ ] **Step 2: If needed, add the same bend encoder used by poryaaaa-m4l**

Create `packages/ccomidi/src/core/bend_encoding.h`:

```c++
#pragma once

#include <cstdint>

namespace ccomidi {

struct BendBytes {
  std::uint8_t lsb;
  std::uint8_t msb;
};

BendBytes encode_bend_units(long bend);

} // namespace ccomidi
```

Create `packages/ccomidi/src/core/bend_encoding.cpp`:

```c++
#include "core/bend_encoding.h"

namespace ccomidi {
namespace {

long clamp_bend_units(long bend) {
  if (bend < -64)
    return -64;
  if (bend > 63)
    return 63;
  return bend;
}

} // namespace

BendBytes encode_bend_units(long bend) {
  const long value14 = 8192 + clamp_bend_units(bend) * 128;
  return {
      static_cast<std::uint8_t>(value14 & 0x7F),
      static_cast<std::uint8_t>((value14 >> 7) & 0x7F),
  };
}

} // namespace ccomidi
```

Add `src/core/bend_encoding.cpp` to `ccomidi_core` in `packages/ccomidi/CMakeLists.txt`:

```cmake
add_library(ccomidi_core
    src/core/sender_core.cpp
    src/core/bend_encoding.cpp
)
```

- [ ] **Step 3: If needed, add ccomidi packing tests**

In `packages/ccomidi/src/tests/test_sender_core.cpp`, add:

```c++
#include "core/bend_encoding.h"
```

Add this test near the other small unit tests:

```c++
void test_bend_units_encode_to_midi_pitch_bend_bytes() {
  const ccomidi::BendBytes down = ccomidi::encode_bend_units(-64);
  ASSERT_EQ(down.lsb, 0, "bend -64 lsb");
  ASSERT_EQ(down.msb, 0, "bend -64 msb");

  const ccomidi::BendBytes center = ccomidi::encode_bend_units(0);
  ASSERT_EQ(center.lsb, 0, "bend 0 lsb");
  ASSERT_EQ(center.msb, 64, "bend 0 msb");

  const ccomidi::BendBytes small = ccomidi::encode_bend_units(-2);
  ASSERT_EQ(small.lsb, 0, "bend -2 lsb");
  ASSERT_EQ(small.msb, 62, "bend -2 msb");

  const ccomidi::BendBytes up = ccomidi::encode_bend_units(63);
  ASSERT_EQ(up.lsb, 0, "bend +63 lsb");
  ASSERT_EQ(up.msb, 127, "bend +63 msb");
}
```

Call it from `main()`:

```c++
  test_bend_units_encode_to_midi_pitch_bend_bytes();
```

- [ ] **Step 4: If a true pitch-bend command exists, emit packed bytes**

Wherever the true M4A pitch-bend control is emitted, use:

```c++
const ccomidi::BendBytes bend = ccomidi::encode_bend_units(m4aBend);
push_midi_event(time, static_cast<std::uint8_t>(0xE0 | output_channel()),
                bend.lsb, bend.msb, outEvents);
```

Do not apply this to raw forwarded host MIDI events.

- [ ] **Step 5: Build ccomidi Release and run tests**

Run from `packages/ccomidi`:

```bash
cmake --build build --config Release --target ccomidi_core_tests ccomidi
./build/ccomidi_core_tests
```

Expected: all ccomidi core tests pass.

- [ ] **Step 6: Commit**

If ccomidi code changed:

```bash
git add packages/ccomidi/CMakeLists.txt \
        packages/ccomidi/src/core/bend_encoding.h \
        packages/ccomidi/src/core/bend_encoding.cpp \
        packages/ccomidi/src/tests/test_sender_core.cpp \
        packages/ccomidi/src/core/sender_core.cpp \
        packages/ccomidi/src/plugin/ccomidi_plugin.cpp
git commit -m "fix(ccomidi): pack pitch bend as M4A carrier bytes"
```

If inspection found no internal pitch-bend dial/control path, do not commit anything for this task.

---

### Task 5: End-To-End Export Sanity Check

**Files:**

- Inspect: `packages/ccomidi/mid2agb/midi.cpp`
- Inspect: `packages/ccomidi/mid2agb/agb.cpp`

- [ ] **Step 1: Confirm `mid2agb` semantics are unchanged**

Inspect:

```bash
sed -n '505,514p' packages/ccomidi/mid2agb/midi.cpp
sed -n '515,523p' packages/ccomidi/mid2agb/agb.cpp
```

Expected facts:

```c++
event.param1 = ReadInt8();
event.param2 = ReadInt8();
```

and:

```c++
PrintOp(event.time, "BEND  ", "c_v%+d", event.param2 - 64);
```

- [ ] **Step 2: Create a tiny MIDI fixture or use an existing recorder output**

The fixture must include these pitch-bend bytes on one channel:

```text
0xE0, 0x00, 0x62
```

That represents true M4A bend `-2`.

- [ ] **Step 3: Run `mid2agb` and inspect output**

Run from `packages/ccomidi` or the package that owns the built `mid2agb` binary:

```bash
./build/mid2agb/mid2agb /path/to/pitch-bend-minus-2.mid /tmp/pitch-bend-minus-2.s
rg -n "BEND" /tmp/pitch-bend-minus-2.s
```

Expected:

```asm
BEND  , c_v-2
```

If the local `mid2agb` build path differs, use the package-local build target and report the exact command used.

- [ ] **Step 4: Final validation**

Run:

```bash
git diff --check
```

Run package validations:

```bash
cd packages/poryaaaa
cmake --build build --target poryaaaa_unit_tests poryaaaa poryaaaa_render
./build/poryaaaa_unit_tests

cd ../ccomidi
cmake --build build --config Release --target ccomidi_core_tests ccomidi
./build/ccomidi_core_tests
```

Expected:

- poryaaaa stores live pitch bend as decoded M4A units.
- poryaaaa recorder still stores raw pitch-bend MIDI bytes.
- ccomidi true pitch-bend controls, if present, emit `data2 = bend + 64`.
- `mid2agb` export still emits `BEND c_v(data2 - 64)`.

- [ ] **Step 5: Commit final fixture/docs changes if any**

If this task created a durable fixture or docs update:

```bash
git add <fixture-or-doc-path>
git commit -m "test: cover M4A pitch bend export contract"
```

Do not commit temporary files under `/tmp`.

## Non-Goals

- Do not make poryaaaa generic-MIDI-correct for pitch bend.
- Do not let pitch-bend `data1`/LSB affect M4A live audio.
- Do not subtract 64 in the recorder.
- Do not change `BENDR`; it remains a raw CC-style value that multiplies decoded M4A bend units.
- Do not modify `mid2agb` pitch-bend export unless a test proves the current `event.param2 - 64` behavior changed.

## Completion Criteria

- `data2 = 64` produces live `track->bend = 0`.
- `data2 = 62` produces live `track->bend = -2`.
- `data2 = 32` produces live `track->bend = -32`.
- With `BENDR = 2`, live effective bend for `data2 = 32` is `-64` M4A bend units.
- Recorder output preserves the original pitch-bend bytes.
- `mid2agb` exports the same recorded events as `BEND c_v+0`, `BEND c_v-2`, or `BEND c_v-32` respectively.
