# HW PSG Shared Frame Sequencer Plan

## Goal

Implement shared PSG frame-sequencer infrastructure in `HwPsgSynth` before
adding audible SQ1 sweep behavior.

This phase should establish the hardware timing grid used by mGBA for PSG
length, sweep, and envelope clocks, while keeping existing audio behavior
unchanged except for NR52 power-reset bookkeeping.

## Reference

Use mGBA GBA-mode behavior as the primary reference because the target game is
played on mGBA.

GBA.emu is useful as a secondary reference for the same dispatch table, but it
uses a different step-storage convention.

Reference facts:

- mGBA GBA PSG frame sequencer runs at 512 Hz.
- mGBA uses `DMG_SM83_FREQUENCY = 0x400000`, `FRAME_CYCLES = 0x400000 >> 9`,
  and GBA `timingFactor = 4`, giving `4 * 8192 = 32768` GBA cycles per frame
  sequencer tick.
- mGBA stores `frame = 0` on full reset and pre-increments before dispatch:
  `frame = (frame + 1) & 7`.
- Therefore the first dispatched frame step after full reset is `1`.
- mGBA stores `frame = 7` on `NR52` disabled-to-enabled transitions, so the
  first dispatched frame step after PSG power re-enable is `0`.
- Dispatch table:
  - steps `2` and `6`: SQ1 sweep opportunity
  - steps `0`, `2`, `4`, and `6`: length-counter opportunity
  - step `7`: envelope opportunity

## Scope

Phase 1 implements the shared frame sequencer and test/debug visibility only.

In scope:

- Add frame-sequencer state to `HwPsgSynth`.
- Drive the sequencer from chip-internal rendered sample time inside
  `hw_psg_render()`.
- Use mGBA-style pre-increment dispatch.
- Preserve sequencer phase across render-rate changes.
- Reset the sequencer on `hw_psg_init()`.
- Reset the sequencer on `NR52` master off-to-on transitions.
- Clear/reset PSG channel state on `NR52` master-disable.
- Add no-op length/sweep/envelope dispatch hooks that increment debug counters.
- Add tests for timing, dispatch table, chunk invariance, NR52 reset, and
  render-rate continuity.

Out of scope for Phase 1:

- Audible SQ1 sweep frequency changes.
- Length-counter behavior.
- Hardware-envelope behavior.
- Full GB power-up register quirks.

## Design

Add state to `HwPsgSynth`:

```c
uint8_t frame_seq_step;
double frame_seq_accum;
uint64_t frame_seq_ticks;
uint64_t frame_seq_length_ticks;
uint64_t frame_seq_sweep_ticks;
uint64_t frame_seq_envelope_ticks;
```

Use a constant equivalent to mGBA's 512 Hz frame event:

```c
#define HW_PSG_FRAME_SEQ_HZ 512.0
#define HW_PSG_FRAME_SEQ_STEPS 8
```

Advance during `hw_psg_render()`:

```c
frame_seq_accum += HW_PSG_FRAME_SEQ_HZ / (double)psg->render_rate;
while (frame_seq_accum >= 1.0) {
	frame_seq_accum -= 1.0;
	hw_psg_tick_frame_sequencer(psg);
}
```

The tick helper should mirror mGBA's convention:

```c
psg->frame_seq_step = (psg->frame_seq_step + 1) & 7;
switch (psg->frame_seq_step) {
case 2:
case 6:
	hw_psg_frame_sweep(psg);
	/* fall through to length */
case 0:
case 4:
	hw_psg_frame_length(psg);
	break;
case 7:
	hw_psg_frame_envelope(psg);
	break;
}
```

For Phase 1, `hw_psg_frame_sweep`, `hw_psg_frame_length`, and
`hw_psg_frame_envelope` should increment debug counters only.

## NR52 Behavior

Current `hw_psg_apply_event()` handles `M4A_REG_NR52` by updating
`master_enabled`.

Change it to detect transitions:

- enabled -> disabled:
  - set `master_enabled = false`
  - clear/reset PSG channel state, including future SQ1 sweep state
  - seed the frame sequencer for the next mGBA-style NR52 re-enable
- disabled -> enabled:
  - set `master_enabled = true`
  - reset frame sequencer state to mGBA NR52 re-enable convention
- enabled -> enabled:
  - leave frame sequencer phase intact
- disabled -> disabled:
  - leave disabled state stable

Full init reset convention:

```c
frame_seq_step = 0;
frame_seq_accum = 0.0;
```

With pre-increment dispatch, the first dispatched step is `1`.

NR52 disabled-to-enabled convention:

```c
frame_seq_step = 7;
frame_seq_accum = 0.0;
```

With pre-increment dispatch, the first dispatched step is `0`.

## Debug/Test API

Add a small read-only introspection API in `hw_psg.h`:

```c
typedef struct {
	uint8_t frame_step;
	double frame_accum;
	uint64_t frame_ticks;
	uint64_t length_ticks;
	uint64_t sweep_ticks;
	uint64_t envelope_ticks;
} HwPsgFrameSequencerDebug;

void hw_psg_get_frame_sequencer_debug(const HwPsgSynth *psg,
                                      HwPsgFrameSequencerDebug *out);
```

Keep this always available. This repo does not expose `hw_audio` as a stable
public SDK, and direct introspection is the cleanest way to test a no-op timing
subsystem without inventing audible side effects.

## Tests

Add chip-level tests, likely in `test/test_engine.c` under `HW_AUDIO_V2`.

### 1. mGBA Reset Convention

Create `HwPsgSynth` directly at render rate `131072`.
Render exactly one 512 Hz tick worth of internal samples:

```c
131072 / 512 = 256 samples
```

Assert:

- `frame_ticks == 1`
- `frame_step == 1`
- no length/sweep/envelope opportunity has fired yet

### 2. Eight-Step Dispatch Table

Render eight ticks at `131072 Hz`:

```c
8 * 256 = 2048 samples
```

Assert:

- `frame_ticks == 8`
- `frame_step == 0`
- `length_ticks == 4`
- `sweep_ticks == 2`
- `envelope_ticks == 1`

### 3. Chunk Invariance

Render the same 2048 internal samples in:

- one `hw_psg_render()` call
- many small calls, such as 17-sample chunks plus a tail

Assert final debug state is identical.

### 4. NR52 Reset

Use `HwAudio` or direct `HwPsgSynth` events.

Advance to a nonzero frame step, write:

```c
M4A_REG_NR52 = 0x00
M4A_REG_NR52 = 0x80
```

Render one frame-sequencer tick and assert:

- `frame_ticks == 1` after reset-relative counters clear
- `frame_step == 0`
- `length_ticks == 1`
- `sweep_ticks == 0`
- `envelope_ticks == 0`

### 5. Render-Rate Continuity

Create `HwPsgSynth` at `131072 Hz`.
Render partway through a frame tick, then call:

```c
hw_psg_set_render_rate(&psg, 262144.0f);
```

Continue rendering enough samples to cross the next sequencer boundary.

Assert:

- frame tick count continues monotonically
- frame accumulator was not reset by the render-rate change
- final step matches the expected continued phase

## Phase 2 Follow-Up

After Phase 1 passes, implement SQ1 sweep on top of the tested frame
sequencer.

Expected SQ1 sweep state:

- raw NR10 fields: period/time, direction, shift
- sweep timer
- sweep enabled flag
- sweep occurred/subtract-used flag
- sweep shadow frequency / real frequency

SQ1 sweep should be driven only from frame steps `2` and `6`.
Driver writes to `NR13`/`NR14` must remain distinct from chip-side
sweep-modified frequency, matching the plan's driver-to-chip contract.
