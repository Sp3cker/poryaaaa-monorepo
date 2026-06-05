# midiformat

_max · Notes_

> Prepare data in the form of a MIDI message

Numbers received in the inlets are used as data for MIDI messages. The data is formatted into a complete MIDI message (with the status byte determined by the inlet) and sent out the outlet as individual bytes.

## Inlets / Outlets

| port | meaning |
|------|---------|
| out0 | MIDI Message Output |
| out1 | midievent Message Output |

## Arguments

- **initial-MIDI-channel-number** (`int`) _(optional)_ — TEXT_HERE
  Sets an initial value for the channel number of the MIDI messages. Numbers greater than 16 are wrapped around to stay between 1 and 16. If there is no argument, the channel number is initially set to 1.
- **initial-MIDI-channel-number** (`float`) _(optional)_ — TEXT_HERE
  Converted to int.

## Messages

- `int(value: int)` — Depends on inlet
  Function depends on inlet. See inlet entries, in1 in2 in3 in4 in5 in6 in7, for descriptions.
- `float(value: float)` — Depends on inlet
  Function depends on inlet. See inlet entries, in1 in2 in3 in4 in5 in6 in7, for descriptions.
- `list(value: int)` — Depends on inlet
  Function depends on inlet. See inlet entries, in1 in2 in3 in4 in5 in6 in7, for descriptions.
- `in1(pitch-value and velocity: list)` — In leftmost inlet: The first number is a pitch value and the second number is a velocity value, to be formatted into a note-on message.
- `in2(aftertouch and pitch-value: list)` — In 2nd inlet: The first number is an aftertouch (pressure) value and the second number is a pitch value (key number), to be formatted into a polyphonic key pressure message.
- `in3(control-value and controller-number: list)` — In 3rd inlet: The first number is a controller number and the second number is a control value, to be formatted into a control message.
- `in4(program-change-value: int)` — In 4th inlet: The value is formatted into a program change message.
- `in5(aftertouch: int)` — In 5th inlet: The value is formatted into an aftertouch (channel pressure) message.
- `in7(MIDI-channel-number: int)` — In rightmost inlet: The number is stored as the channel number of the MIDI messages.
  In rightmost inlet: The number is stored as the channel number of the MIDI messages. The actual value of the status byte is dependent on the channel. Numbers greater than 16 are wrapped around to stay between 1 and 16.

## Attributes

- `@hires` (int) — High-resolution Pitch Bend
  The hires attribute is used to support high-resolution pitch bend scaling. When the attribute is set to 0, the pitch bend inlet will accept integer values in the standard MIDI range of 0 to 127. When the attribute is set to 1, it accepts float values in the audio signal range of -1 to 1. When the attribute is set to 2, it accepts integer values in the range of -8192 to 8191 (standard 14-bit MIDI high resolution pitch bend range).
- `@introduced` (symbol)
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### high resolution pitch bend

```
Example — [midiformat @hires 1]
  fan-in:
    in0 ← [attrui @hires]    # set hires attribute (0-2)
    in0 ← [getattr hires]
    in0 ← [pack]
    in5 ← [multislider] ← [p scaling]    # bend the pitch / p scaling emits: "range 0 127, settype 0" | "range -1 1, settype 1" | "range -8192 8191, settype 0" | "$1, $2 300"
  fan-out:
    out0 → [midiout]:in0    # Output of midiformat typically goes to midiout
    out0 → [print midibyte @popup 1]:in0
    out1 → [message ""]:in1
```

Attributes demonstrated: `@hires`

### basic

> The rightmost outlet of the midiformat object converts MIDI input into properly formatted midievent messages for use with the vst~ object.

```
Example — [midiformat]
  fan-in:
    in0 ← [message "60 0"]
    in0 ← [message "60 127"]    # Note On
    in1 ← [message "60 5"]    # Polyphonic (Key) Aftertouch <pitch, pressure>
    in2 ← [message "1 0"]    # Control Change <controller, value>
    in2 ← [message "1 127"]
    in3 ← [message "1"]    # Program Change
    in4 ← [message "127"]    # Channel Aftertouch
    in5 ← [message "64"]
    in6 ← [umenu]    # Pitch Bend
  fan-out:
    out0 → [thresh]:in0
    out0 → [midiout]:in0    # Output of midiformat typically goes to midiout.
    out0 → [print midibyte @popup 1]:in0    # One byte is 256 values
    out1 → [message "midievent 160 60 5"]:in1
```

## See also

`borax`, `midiin`, `midiinfo`, `midiparse`, `midiselect`, `mpeconfig`, `mpeformat`, `mpeparse`, `noteout`, `polymidiin`, `sxformat`, `xbendout`, `xnoteout`
