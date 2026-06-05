# HW PSG SQ1 Sweep State Plan

## Goal

Implement SQ1 NR10 frequency sweep in `HwPsgSynth` on top of the shared PSG
frame sequencer.

The current shared frame sequencer is the timing foundation. Sweep should plug
into the frame step `2`/`6` hook and must preserve the sample-boundary ordering:
render the current internal sample first, then apply the frame event so audible
state changes affect the following internal sample.

## Reference Direction

Primary reference: mGBA GBA-mode behavior.

Secondary reference: GBA.emu. It is useful because its state model is compact
and close to poryaaaa's current `hw_psg.c` shape.

Both references agree on the important ownership point:

- non-trigger NR13/NR14 writes update the current oscillator/register-backed
  frequency only
- NR14 trigger reloads the sweep shadow from the current frequency
- committed sweep updates mutate both the current oscillator frequency and the
  sweep shadow

## Chosen State Model

Use the GBA.emu-style hybrid model:

```c
uint16_t sq1_freq;              /* current oscillator/register-backed frequency */
uint16_t sq1_sweep_shadow_freq; /* sweep calculation shadow */
```

Do not introduce a separate long-lived effective-frequency selector such as:

```c
uint16_t sq1_freq_reg;
uint16_t sq1_sweep_freq;
bool sq1_use_sweep_freq;
```

That split is more architectural, but it would spread frequency-selection
logic across register writes, render, trigger, and sweep. The chosen model
keeps render unchanged: `hw_psg_render()` continues reading `sq1_freq`.

## Frequency Write Semantics

Current poryaaaa behavior:

- NR13 updates low 8 bits of `sq1_freq`
- NR14 updates high 3 bits of `sq1_freq`
- render uses `sq1_freq`

Add sweep behavior without changing non-sweep frequency reads:

```text
NR13 write, no trigger:
  update sq1_freq
  do not reload sq1_sweep_shadow_freq

NR14 write, no trigger:
  update sq1_freq
  do not reload sq1_sweep_shadow_freq

NR14 write with trigger:
  update sq1_freq
  reload sq1_sweep_shadow_freq from sq1_freq
  reset/reload sweep timer state
  run initial overflow precheck if shift != 0

frame sequencer sweep commit:
  compute from sq1_sweep_shadow_freq
  if valid, assign new frequency to both:
    sq1_sweep_shadow_freq = new_freq
    sq1_freq = new_freq
```

This matches mGBA's structure:

- `control.frequency` is the current oscillator/register-backed frequency
- `sweep.realFrequency` is the sweep shadow
- NR13/NR14 ordinary writes update `control.frequency`
- NR14 trigger sets `sweep.realFrequency = control.frequency`
- sweep commits update both fields

It also matches GBA.emu behaviorally:

- `sweep_freq` is the sweep shadow
- oscillator timing reads register-backed `regs[3]/regs[4]`
- committed sweep writes new frequency back into `regs[3]/regs[4]`

## Expected Sweep State Fields

Likely additions to `HwPsgSynth`:

```c
uint16_t sq1_sweep_shadow_freq;
uint8_t  sq1_sweep_time;       /* NR10 pace, with 0 treated as 8 internally */
uint8_t  sq1_sweep_shift;      /* NR10 shift */
bool     sq1_sweep_decrease;   /* NR10 direction */
uint8_t  sq1_sweep_timer;
bool     sq1_sweep_enabled;
bool     sq1_sweep_occurred;   /* needed for mGBA direction-change behavior */
```

Naming can adjust during implementation, but keep the distinction clear:

- `sq1_freq` = current oscillator frequency
- `sq1_sweep_shadow_freq` = sweep calculation shadow

## mGBA Behaviors To Mirror

From local mGBA `src/gb/audio.c`:

- NR10 write decodes sweep shift, direction, and time.
- Sweep time `0` is stored internally as `8`.
- If a negative sweep has occurred, changing direction from decrease to
  increase disables channel 1.
- Trigger sets `sweep.realFrequency = control.frequency`.
- Trigger resets the sweep timer.
- Trigger performs an initial overflow calculation when shift is nonzero.
- Frame steps `2` and `6` decrement the sweep timer if sweep is enabled.
- When the timer reaches zero, sweep update runs and reloads the timer.
- Increase overflow (`>= 2048`) disables SQ1.
- Decrease path can commit non-negative frequency updates.
- Increase path performs the second overflow calculation after committing.

These details should be re-read directly during implementation rather than
implemented from memory.

## Tests To Add First

Start with state/debug-level tests before audible assertions:

1. NR10 decode:
   - pace/time
   - direction
   - shift
   - time `0` becomes internal `8`

2. Trigger initialization:
   - NR13/NR14 establish `sq1_freq`
   - NR14 trigger reloads `sq1_sweep_shadow_freq`
   - sweep timer reloads
   - sweep enabled flag follows mGBA logic

3. Non-trigger frequency writes:
   - NR13/NR14 without trigger update `sq1_freq`
   - they do not reload `sq1_sweep_shadow_freq`

4. Sweep clock cadence:
   - only frame steps `2` and `6` decrement sweep timer
   - other frame steps leave sweep timer unchanged

5. Sweep commit:
   - when timer reaches zero, computed frequency updates both `sq1_freq` and
     `sq1_sweep_shadow_freq`

6. Overflow:
   - increase overflow disables SQ1
   - initial trigger precheck can disable SQ1 when the calculated frequency is
     out of range

7. Boundary ordering:
   - the frame-boundary sample uses pre-sweep frequency
   - the following internal sample sees the swept frequency

## Open Questions

- Exact public/debug API shape for inspecting sweep state in tests.
- Whether to add a dedicated `HwPsgSq1SweepDebug` struct or extend the existing
  frame-sequencer debug API.
- How much of mGBA's negative-sweep direction-change quirk is necessary for GBA
  m4a usage, versus implementing it immediately for parity.
- Whether tests should assert exact mGBA `occurred` behavior now or defer that
  to a later compatibility edge-case pass.

## Current Decision

Proceed with GBA.emu-style storage and mGBA-compatible behavior:

```text
sq1_freq is the current oscillator frequency.
sq1_sweep_shadow_freq is sweep's calculation shadow.
Sweep commits mutate both.
Non-trigger NR13/NR14 writes do not reload the shadow.
Trigger reloads the shadow.
```
