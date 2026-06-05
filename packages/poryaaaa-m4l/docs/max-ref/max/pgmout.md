# pgmout

_max · MIDI_

> Send MIDI program changes

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Program Change |
| in1 | MIDI Channel |

## Arguments

- **port** (`symbol`) _(optional)_ — MIDI port ID
  Specifies the port for transmitting MIDI program change messages. When a letter argument is present, channel numbers greater than 16 received in the right inlet will be wrapped around to stay within the 1-16 range. If there is no argument, pgmout initially transmits out port a, on MIDI channel 1.
- **port-channel** (`list`) — MIDI port and channel
  A letter and number combination (separated by a space) indicates a port and a specific MIDI channel on which to transmit program change messages. Channel numbers greater than 16 will be wrapped around to stay within the 1-16 range.
- **device** (`symbol`) _(optional)_ — MIDI output device
  The name of a MIDI output device may be used as the first argument to specify the port. The name 'none' can be used to prevent the object from transmitting MIDI messages on any port.
- **channel** (`int`) — Extended MIDI channel
  A number alone can be used in place of a letter and number combination. The exact meaning of the channel number argument depends on the channel offset specified for each port in the MIDI Setup dialog.

## Messages

- `int(input: int)` — Send MIDI program change
  In left inlet: The number has 1 subtracted from it and then is transmitted as a program change value on the specified channel and port. Numbers are limited between 1 and 128, and are sent out as program changes 0 to 127.
- `float(input: float)` — Send MIDI program change
  Converted to int.
- `anything(port: list)` — Set the MIDI output device
  Performs the same function as port but without need for the word, "port".
- `in1(channel: int)` — Set MIDI channel
  In right inlet: The number is stored as the channel number on which to transmit the program change messages.
- `port(port: symbol)` — Set the MIDI output device
  The word port, followed by a letter a- z or the name of a MIDI output port or device, specifies the port used to transmit the MIDI messages. The word port is optional and may be omitted. The name 'none' can be used to prevent the object from transmitting MIDI messages on any port.
- `list(input: list)` — Send MIDI program change
  In left inlet: The first number is the program number +1, and the second number is the channel of a MIDI program change message transmitted on the specified channel and port.

## GUI behaviors

- `(mouse)` — Select a MIDI output device
  Double-clicking on a pgmout object shows a pop-up menu for choosing a MIDI port or device.

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
Example #1 — [pgmout]
  fan-in:
    in0 ← [random 128] ← [metro 100] ← [toggle]    # MIDI symphony
    in1 ← [+ 1] ← [random 16] ← [metro 100]
```

```
Example #2 — [pgmout]
  fan-in:
    in0 ← [number] ← [dial]
    in1 ← [number] ← [dial]    # set MIDI channel
```

## See also

`midiout`, `pgmin`
