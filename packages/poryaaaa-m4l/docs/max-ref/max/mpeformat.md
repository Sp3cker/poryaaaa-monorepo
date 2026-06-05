# mpeformat

_max · MIDI_

> Prepare data in the form of a Multidimensional Polyphonic Expression (MPE) MIDI message

Numbers received in the inlets from midiformat objects are routed as MPE-compatible MIDI messages. In addition, the object outputs mpeevent messages for use with instruments hosted by the vst~ object.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | MIDI Input (Zone Master Channel) |
| in1 | MIDI Input (for MIDI Output Channel 2) |
| in2 | MIDI Input (for MIDI Output Channel 3) |
| in3 | MIDI Input (for MIDI Output Channel 4) |
| in4 | MIDI Input (for MIDI Output Channel 5) |
| in5 | MIDI Input (for MIDI Output Channel 6) |
| in6 | MIDI Input (for MIDI Output Channel 7) |
| in7 | MIDI Input (for MIDI Output Channel 8) |
| in8 | MIDI Input (for MIDI Output Channel 9) |
| in9 | MIDI Input (for MIDI Output Channel 10) |
| in10 | MIDI Input (for MIDI Output Channel 11) |
| in11 | MIDI Input (for MIDI Output Channel 12) |
| in12 | MIDI Input (for MIDI Output Channel 13) |
| in13 | MIDI Input (for MIDI Output Channel 14) |
| in14 | MIDI Input (for MIDI Output Channel 15) |
| in15 | MIDI Input (for MIDI Output Channel 16) |
| out0 | MIDI Output |
| out1 | mpeevent Message Output |

### Port details

**`out0` (MIDI Output):** Out left outlet: MIDI messages are sent out as individual bytes.

**`out1` (mpeevent Message Output):** Out rightmost outlet: The MPE Event Message. The MPE event message is a list composed of the symbol mpeevent, followed by 6 integers which specify the Zone Master Channel, Zone Index, Voice Number, Channel Number, MIDI Message Number, and Data. This message can be sent to a patch encapsulated in a poly~ object using the polymidiin object.

## Arguments

- **channels** (`int`) — Number of MIDI input channels
  An argument can be used to set the number of MIDI input channels. The number of inlets will be one more than the argument value, since the leftmost inlet is a global MIDI input. If no argument is specified, one Zone Master Channel input and 15 channel inputs will be created.

## Messages

- `bang` — bang message to the leftmost inlet will send out the MPE configuration message for the current state of this object out the mpeformat object's left outlet.
- `int(byte: int)` — MIDI message data
  Numbers received in the other inlets are used as data for MIDI messages and routed to the input channel corresponding to the inlet.
- `float(byte: float)` — Converted to int.
- `midievent(ARG_NAME_0: list)` — MIDI event message
  In any inlet: The MIDI Event Message. The message is a list composed of the symbol midievent, followed by a list of integers which specify the MIDI event type and value.
- `mpeevent(MPE message: list)` — MPE MIDI event message
  In any inlet: The MPE event message is a list composed of the symbol mpeevent, followed by 6 integers which specify the Zone Master Channel, Zone Index, Voice Number, Channel Number, MIDI Message Number, and Data.
- `reset` — Reset input channel/global
  The message reset resets an individual voice (if received in a voice inlet) or all voices (if received in the leftmost inlet).

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in2` — MIDI Input (for MIDI Output Channel 3)
> - `in4` — MIDI Input (for MIDI Output Channel 5)
> - `in5` — MIDI Input (for MIDI Output Channel 6)
> - `in6` — MIDI Input (for MIDI Output Channel 7)
> - `in7` — MIDI Input (for MIDI Output Channel 8)
> - `in8` — MIDI Input (for MIDI Output Channel 9)
> - `in9` — MIDI Input (for MIDI Output Channel 10)
> - `in10` — MIDI Input (for MIDI Output Channel 11)
> - `in11` — MIDI Input (for MIDI Output Channel 12)
> - `in12` — MIDI Input (for MIDI Output Channel 13)
> - `in13` — MIDI Input (for MIDI Output Channel 14)
> - `in14` — MIDI Input (for MIDI Output Channel 15)
> - `in15` — MIDI Input (for MIDI Output Channel 16)

### basic

> The @chanrange attribute can be used to set the number of channels following the Zone master channel instead of an argument to the object

> raw MIDI stream out

```
Example — [mpeformat 4 @masterchan 1]  The @masterchan attribute sets the first MIDI channel to be addressed / voice 4 / voice 3 / voice 2 / voice 1 / all voices
  fan-in:
    in0 ← [midiformat] ← [number]    # global pitch bend
    in0 ← [button]    # send an MPE configuration message for the current state of this object
    in1 ← [midiformat]
    in3 ← [midiformat]
  fan-out:
    out0 → [print]:in0
    out0 → [midiout]:in0
    out1 → [message "mpeevent 1 1 1 177 7 70"]:in1    # The rightmost outlet of the mpeformat object converts MIDI input into properly formatted mpeevent messages for use with the vst~ object.
```

## See also

`midiformat`, `midiin`, `midiparse`, `mpeconfig`, `mpeparse`, `polymidiin`
