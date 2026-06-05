# ddg.mono

_msp · MIDI_

> Monophonic Synth Controller for Virtual Synths

ddg.mono provides MIDI message handling for virtual monophonic synths in Max. It implements last/high/low note priority and legato/retrigger phrasing.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | int | MIDI Note in |
| in1 | int | MIDI Velocity in |
| out0 | int | MIDI Note out |
| out1 | int | MIDI Velocity out |

## Arguments

- **note priority** (`int`) _(optional)_ — Note priority mode
  An optional integer argument in the range 0 - 2 can be used to specify note priority. The mode settings are:

 mode 0 (retrigger): Retrigger on each keypress (default).

 mode 1 (legato): When a note is held, new notes ignore velocity.

 mode 2 (laststep): Send a noteoff message on last note only.

## Messages

- `int(MIDI-note-value: int)` — Function depends on inlet
  In first inlet: The number is treated as a pitch value for a MIDI note-on message. Output of the received number and its corresponding velocity value is dependent on the mode attribute and the triggering modes (set using the legato or retrig messages).
  In second inlet: The number is stored as a velocity to be paired with pitch numbers received in the left inlet.
- `clear` — Clear all notes
  The clear message will send a note-off message to stop any notes which are currently playing.
- `in1(MIDI-velocity-value: int)` — Store as a velocity to pair with pitch values
  In right inlet: The number is stored as a velocity to be paired with pitch numbers received in the left inlet.
- `laststep` — Send a note-off for the last note played
  The laststep message will cause the ddg.mono object to send a note-off message for the last note played.
- `legato` — Send output only when a new note is played
  The legato message will cause the ddg.mono object to send its output only when a new note is played.
- `retrig` — Send output on each key press (retrigger)
  The retrig message will cause the ddg.mono object to send its output on each key press (i.e., retriggering previously played notes).

## Attributes

- `@legatomode` (atom_long) — Legato mode
  Sets the legato mode.
- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `legatomode` — seen as: `legatomode $1`
- `mode` — seen as: `mode $1`

## Help patcher examples

### basic

```
Example — [ddg.mono 0]
  fan-in:
    in0 ← [number] ← [kslider]
    in0 ← [message "clear"]    # clear = forced note-off / Messages
    in0 ← [message "legatomode $1"]
    in0 ← [message "mode $1"]
    in1 ← [number] ← [kslider]
  fan-out:
    out0 → [number]:in0    # note
    out0 → [button]:in0
    out1 → [number]:in0    # velocity
    out1 → [button]:in0
```

## See also

`kslider`, `midiin`, `midiparse`, `notein`
