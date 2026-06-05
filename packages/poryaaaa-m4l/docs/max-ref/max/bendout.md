# bendout

_max · MIDI_

> Send MIDI pitch bend messages

Transmits MIDI pitchbend values to a MIDI device.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Pitch Bend Amount |
| in1 | MIDI Channel |

## Arguments

- **port** (`symbol`) _(optional)_ — MIDI port (a-z)
  Specifies the port for transmitting MIDI pitch bend messages. Channel numbers greater than 16 received in the right inlet will be wrapped around to stay within the 1-16 range. If there is no argument, bendout initially transmits out port a, on MIDI channel 1.
- **port-and-channel** (`list`) — MIDI port (a-z) and channel
  and int A letter and number combination (separated by a space) indicates a port and a specific MIDI channel on which to transmit pitch bend messages. Channel numbers greater than 16 will be wrapped around to stay within the 1-16 range.
- **midi-device** (`symbol`) _(optional)_ — MIDI device name
  The name of a MIDI output device may be used as the first argument to specify the port. The name 'none' can be used to prevent the object from transmitting MIDI messages on any port.
- **channel** (`int`) — Extended MIDI channel number
  A number alone can be used in place of a letter and number combination. The exact meaning of the channel number argument depends on the channel offset specified for each port in the MIDI Setup dialog.

## Messages

- `int(input: int)` — Send MIDI pitch bend data
  In left inlet: The number is transmitted as a MIDI pitch bend value on the specified channel and port. Numbers are limited between 0 and 127.
- `float(input: float)` — Send MIDI pitch bend data
  Converted to int.
- `anything(MIDI-port: list)` — See the port message
- `in1(MIDI-channel: int)` — Set MIDI channel
  In right inlet: The number is stored as the channel number on which to transmit the pitch bend messages.
- `port(MIDI-port: symbol)` — Set the MIDI port
  In left inlet: The word port, followed by a letter a-z or the name of a MIDI output port or device, specifies the port used to transmit MIDI messages. The word port is optional and may be omitted. The name 'none' can be used to prevent the object from transmitting MIDI messages on any port.

## GUI behaviors

- `(mouse)` — Select a MIDI port
  Double-clicking on a bendout object shows a pop-up menu for choosing a MIDI port or device.

## Attributes

- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### outport

> About Port and Channel Arguments for MIDI Input

### basic

```
Example — [bendout 1]
  fan-in:
    in0 ← [number] ← [slider]
    in1 ← [number]    # set MIDI channel
```

## See also

`bendin`, `midiout`, `xbendout`, `xbendin`
