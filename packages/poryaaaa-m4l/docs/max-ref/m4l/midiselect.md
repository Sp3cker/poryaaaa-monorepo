# midiselect

_max · MIDI_

> Select and interpret raw MIDI data

midiselect receives raw MIDI bytes and separates the input stream. MIDI data to be selected for output is set using object attributes. There is a separate data outlet associated with each MIDI data selection attribute. Any input data which is unselected will be sent out the object's eighth outlet.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Raw MIDI Data |
| out0 | Note-on and Note-off (list: Pitch, Velocity) |
| out1 | Poly Key Pressure (list: Key, Value) |
| out2 | Control Change (list: Controller Number, Value) |
| out3 | Program Change |
| out4 | Aftertouch |
| out5 | Pitch Bend |
| out6 | MIDI Channel |
| out7 | Unselected Raw MIDI Data |

## Messages

- `bang` — Clear partial MIDI messages
  Clears the midiselect object's memory of any partial MIDI messages received up to that point.
- `int(byte: list)` — Evaluate and output MIDI messages
  Numbers received in the inlet (usually from a seq or midiin object) are treated as bytes of a MIDI message. The status byte and the filtering attributes determine the outlet which will be used to output the data bytes.
- `float(byte: float)` — Evaluate and output MIDI messages
  Floating-point numbers received in the inlet are converted to integer values and treated as bytes of a MIDI message. The status byte and the filtering attributes determine the outlet which will be used to output the data bytes.

## Attributes

- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `ch` — seen as: `ch 1 3 4`, `ch all`
- `note` — seen as: `note 60 63 67 70 74`, `note all`, `note none`

## Help patcher examples

### dynamic selection

```
Example — [midiselect @ch 1]
  fan-in:
    in0 ← [message "note 60 63 67 70 74"]    # select specific notes
    in0 ← [message "ch 1 3 4"]    # select only channels 1, 3 and 4
    in0 ← [message "ch all"]    # select every channels
    in0 ← [message "note all"]    # select every notes
    in0 ← [message "note none"]    # does not select any notes
    in0 ← [midiin]
  fan-out:
    out0 → [button]:in0
    out1 → [button]:in0
    out2 → [button]:in0
    out3 → [button]:in0
    out4 → [button]:in0
    out5 → [button]:in0
    out6 → [button]:in0
    out7 → [button]:in0
```

### basic

```
Example — [midiselect @note all @ch 1 @ctl 1 3 4 @pgm 1]
  fan-in:
    in0 ← [message "176, 1, 65"]    # Control Change 1 ch 2 / Control Change 1 ch 1
    in0 ← [message "160, 60, 50"]    # Poly Key Pressure
    in0 ← [message "224, 95, 95"]    # Pitch Bend
    in0 ← [message "176, 55, 65"]    # Program Change / Control Change 4 ch 1 / Control Change 55 ch 1
    in0 ← [message "192, 75"]
    in0 ← [message "208, 85"]    # After Touch
    in0 ← [message "145, 80, 90"]    # note on channel 2
    in0 ← [message "144, 60, 70"]    # note on
    in0 ← [message "176, 3, 66"]    # Control Change 3 ch 1
    in0 ← [message "176, 4, 67"]
    in0 ← [message "177, 1, 66"]
    in0 ← [message "177, 2, 66"]    # Control Change 2 ch 2
  fan-out:
    out0 → [unpack]:in0
    out1 → [unpack]:in0
    out2 → [unpack]:in0
    out3 → [number]:in0
    out4 → [number]:in0
    out5 → [number]:in0
    out6 → [number]:in0
    out7 → [print rawData @popup 1]:in0
```

## See also

`midiparse`, `borax`, `midiformat`, `midiin`, `midiinfo`
