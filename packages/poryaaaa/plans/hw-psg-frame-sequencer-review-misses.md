# HW PSG Frame Sequencer Review Misses

This note records details missed during the first shared-frame-sequencer
implementation so future behavioral changes avoid the same failure modes.

## Sample Boundary Ordering

The first implementation advanced `hw_psg_advance_frame_sequencer()` at the
top of each `hw_psg_render()` sample loop.

That passed counter-only tests, but it was not a safe foundation for audible
length, sweep, or envelope behavior. mGBA samples current audio first, then
runs the frame event. Once frame hooks mutate SQ1 frequency or channel enable
state, advancing before rendering would make the first audible effect land one
internal sample early.

Future tests for audible frame-sequencer behavior should include a boundary
assertion: the sample at the frame boundary still uses pre-tick state, and the
state change affects the following internal sample.

## NR52 Does Not Clear Wave RAM

The first implementation treated NR52 power-off as a broad PSG clear and
zeroed `wave_ram`.

That is too broad. mGBA clears channel/register state on NR52 disable, but it
does not clear channel 3 wave RAM. GBA.emu's register reset also covers the
NRxx register block and leaves the wave RAM address range outside that clear.

Future NR52 reset work should distinguish channel runtime/register state from
wave RAM contents. Wave RAM must survive NR52 disable/re-enable.

## Transition Stability

The initial tests covered full init, off-to-on, disabled render, and dispatch
counts, but they did not pin stable no-op NR52 transitions:

- `NR52 = 0x80` while already enabled must not reset frame phase or counters.
- `NR52 = 0x00` while already disabled must remain stable.
- NR52 power cycling must preserve wave RAM.

These are foundation tests, not sweep-specific tests, because later sweep work
will rely on NR52 transition behavior to decide when chip-owned state is reset.
