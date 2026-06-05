# ctlout

_max · MIDI_

> Transmit MIDI controller messages

Transmits MIDI continuous controller values to a MIDI device.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Controller Value |
| in1 | Controller Number |
| in2 | MIDI Channel |

## Arguments

- **port** (`symbol`) _(optional)_ — MIDI port ID
  Specifies the port (a-z) for transmitting MIDI control messages. If there is no argument, ctlout initially transmits out port a, on channel 1. When a port is specified by a letter argument, channel numbers greater than 16 received in the right inlet will be wrapped around to stay within the 1-16 range.
- **device** (`symbol`) _(optional)_ — MIDI output device
  The name of a MIDI output device may be used as the first argument to specify the port. The name 'none' can be used to prevent the object from transmitting MIDI messages on any port.
- **ctrllr-channel** (`list`) _(optional)_ — Controller and channel
  Following the (optional) port argument, the next argument is an initial value for the controller number to be used in control messages transmitted by ctlout. Controller numbers are automatically limited between 0 and 127. If there is no controller number specified, the initial controller number is 1.

 Following the controller number argument is an initial value for the channel number on which to transmit control messages. If the channel argument is not present, ctlout initially transmits control messages on channel 1. In order for this argument to be used, a controller number argument must precede it.

 If a port has been specified with a letter argument, channel numbers greater than 16 will be wrapped around to stay within the 1-16 range. If no port argument is present, the channel number specifies both the port and the channel. The exact meaning of the channel number argument depends on the channel offset specified for each port in the MIDI Setup dialog.

## Messages

- `int(input: int)` — Format and output control message
  In left inlet: The number is used as the control value, and ctlout transmits a MIDI control change message. Numbers are limited between 0 and 127.
- `float(input: float)` — Format and output control message
  Converted to int.
- `anything(port: list)` — Set the MIDI output device
  Performs the same function as port.
- `in1(controller: int)` — Set the active MIDI continuous controller number
  In middle inlet: The number is stored as the controller number of the control change messages transmitted by ctlout. Numbers are limited between 0 and 127.
- `in2(channel: int)` — Set MIDI channel number
  In right inlet: The number is stored as the channel number on which to transmit the control messages.
- `port(port: symbol)` — Set the MIDI output device
  In left inlet: The word port, followed by a letter a-z or the name of a MIDI output port or device, specifies the port used to transmit MIDI control messages. The word port is optional and can be omitted. The name 'none' can be used to prevent the object from transmitting MIDI messages on any port.

## GUI behaviors

- `(mouse)` — Select a MIDI input device
  Double-clicking on a ctlout object shows a pop-up menu for choosing a MIDI port or device.

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
Example — [ctlout 1 1]
  fan-in:
    in0 ← [number] ← [dial]    # int in left inlet sets the controller value and sends a control change out the MIDI port
    in1 ← [number] ← [dial]    # int in middle inlet sets controller number
    in2 ← [number] ← [dial]    # int in right inlet sets MIDI channel (1-16 for port a, 17-32 for port b, etc.)
```

## See also

`bendout`, `ctlin`, `midiout`, `noteout`, `xbendout`
