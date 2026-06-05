# midiparse

_max · Notes_

> Interpret raw MIDI data

Separates raw MIDI bytes into standard message types. This object works particularly well formatting the output of the midiin and seq objects.

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
| out6 | MIDI Channel |
| out7 | midievent Message |

## Messages

- `bang` — Clear partial MIDI messages
  Clears the midiparse object's memory of any partial MIDI message received up to that point.
- `int(byte: int)` — Evaluate and output MIDI messages
  Numbers received in the inlet are treated as bytes of a MIDI message (usually from a seq or midiin object). The status byte determines the outlet which will be used to output the data bytes.
- `float(byte: float)` — Evaluate and output MIDI messages
  Converted to int.

## Attributes

- `@hires` (int) — High-resolution Pitch Bend
  The hires attribute is used to support high-resolution pitch bend scaling. When the attribute is set to 0 (default), midiparse will accept and output pitch bend integer values in the standard MIDI range of 0 to 127. When the attribute is set to 1, it accepts high resolution MIDI data and outputs float values in the range of -1 to 1. When the attribute is set to 2, it accepts high resolution MIDI data and outputs integer values in the range of -8192 to 8191 (standard 14-bit MIDI high resolution pitch bend range).
- `@introduced` (symbol)
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### high resolution pitch bend

```
Example — [midiparse]
  fan-in:
    in0 ← [message "224, $1, $2"]
    in0 ← [message "144, 60, 70"]
    in0 ← [getattr hires]    # The hires attribute is used to support high-resolution pitch bend scaling. When the attribute is set to 0, midiparse will accept and output pitch bend integer values in the standard MIDI range of 0 to 127. When the attribute is set to 1, it accepts high resolution MIDI data and outputs float values in the range of -1 to 1. When the attribute is set to 2, it accepts high resolution MIDI data and outputs integer values in the range of -8192 to 8191 (standard 14-bit MIDI high resolution pitch bend range).
    in0 ← [attrui @hires]
  fan-out:
    out0 → [midiformat]:in0
    out1 → [midiformat]:in1
    out2 → [midiformat]:in2
    out3 → [midiformat]:in3
    out4 → [midiformat]:in4
    out5 → [flonum]:in0    # Pitch Bend
    out5 → [midiformat]:in5
    out6 → [midiformat]:in6
    out7 → [message "midievent 224 0 63"]:in1
```

Attributes demonstrated: `@hires`

### basic

```
Example — [midiparse]
  fan-in:
    in0 ← [message "160, 60, 50"]    # note on channel 2
    in0 ← [message "224, 95, 95"]    # pitch bend / after touch
    in0 ← [message "176, 55, 65"]    # poly key pressure
    in0 ← [message "192, 75"]    # program change / control change
    in0 ← [message "208, 85"]
    in0 ← [message "145, 80, 90"]
    in0 ← [message "144, 60, 70"]
  fan-out:
    out0 → [unpack i i]:in0
    out1 → [unpack i i]:in0
    out2 → [unpack i i]:in0
    out3 → [number]:in0
    out4 → [number]:in0
    out5 → [number]:in0
    out6 → [number]:in0
    out7 → [message "midievent 160 60 5"]:in1    # The rightmost outlet of the midiparse object converts MIDI input into properly formatted midievent messages for use with the vst~ object.
```

## See also

`borax`, `midiin`, `midiinfo`, `midiparse`, `midiselect`, `mpeconfig`, `mpeformat`, `mpeparse`, `noteout`, `polymidiin`, `sxformat`, `xbendout`, `xnoteout`
