# mpeparse

_Max · MIDI_

> Interpret raw MPE MIDI data

Separates raw Multidimensional Polyphonic Expression (MPE) MIDI bytes into standard message types.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | MIDI Message Input |
| out0 | Note-on and Note-off (list: Pitch, Velocity) |
| out1 | Poly Key Pressure (list: Key, Value) |
| out2 | Control Change (list: Controller Number, Value) |
| out3 | Program Change |
| out4 | Aftertouch |
| out5 | Pitch Bend |
| out6 | Voice Number (-1 if Global) |
| out7 | Zone First Channel |
| out8 | Zone Index (-1 if Global) |
| out9 | mpeevent Message (list: mpeevent, Zone First Channel, Zone Index, Voice Number, Channel Number, MIDI Message Number, Data) |

## Messages

- `bang` — Clear partial MIDI messages
  Clears the mpeparse object's memory of any partial MPE MIDI message received up to that point.
- `int(byte: int)` — Evaluate and output MPE MIDI messages
  Numbers received in the inlet are treated as bytes of a MIDI message (usually from a midiin or polymidiin object). The status byte determines the outlet which will be used to output the data bytes.
- `float(byte: float)` — Evaluate and output MPE MIDI messages
  Converted to int.
- `mpeevent(MPE-messages: list)` — Parse MPE messages
  The word 'mpeevent' followed by 6 integers, which specify the Zone First Channel, Zone Index, Voice Number, Channel Number, MIDI Message Number, and Data.

## Attributes

- `@hires` (int) — High-resolution Pitch Bend
  The hires attribute is used to support high-resolution pitch bend scaling. When the attribute is set to 0, mpeparse will accept and output pitch bend integer values in the standard MIDI range of 0 to 127. When the attribute is set to 1, it accepts high resolution MIDI data and outputs float values in the range of -1 to 1. When the attribute is set to 2, it accepts high resolution MIDI data and outputs integer values in the range of -8192 to 8191 (standard 14-bit MIDI high resolution pitch bend range).
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### basic

> The rightmost outlet of the mpeparse object converts MIDI input into properly formatted MPE MIDI messages for use with the vst~ object.

> In Max for Live, MPE notes rotate over MIDI channels. Each new note will show a different value at MIDI Message byte 1.

```
Example — [mpeparse]
  fan-in:
    in0 ← [midiin]    # hook up your MPE controller
    in0 ← [message "160, 60, 50"]    # note on
    in0 ← [message "224, 95, 95"]    # pitch bend
    in0 ← [message "176, 55, 65"]    # poly key pressure
    in0 ← [message "192, 75"]    # program change / control change
    in0 ← [message "208, 85"]    # after touch
    in0 ← [message "144, 60, 70"]
    in0 ← [message "177, 1, $1"]    # MPE message
  fan-out:
    out0 → [unpack i i]:in0
    out1 → [unpack i i]:in0
    out2 → [unpack i i]:in0
    out3 → [number]:in0
    out4 → [number]:in0
    out5 → [number]:in0
    out6 → [number]:in0
    out7 → [number]:in0
    out8 → [number]:in0
    out9 → [route mpeevent]:in0
```

## See also

`midiin`, `midiformat`, `midiparse`, `mpeconfig`, `mpeformat`, `polymidiin`
