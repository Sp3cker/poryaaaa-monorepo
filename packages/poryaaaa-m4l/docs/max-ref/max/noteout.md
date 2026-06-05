# noteout

_max · MIDI_

> Transmit MIDI note messages

Transmits note-on and note-off messages to a MIDI device.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Pitch |
| in1 | Velocity (0 for Note-off) |
| in2 | MIDI Channel |

## Arguments

- **port** (`symbol`) _(optional)_ — MIDI port ID
  Specifies the port (a-z) for transmitting MIDI note messages. Channel numbers greater than 16 received in the right inlet will be wrapped around to stay within the 1-16 range. If there is no argument, noteout initially transmits out port a, on MIDI channel 1.
- **port-channel** (`list`) — MIDI port and channel
  A letter and number combination (separated by a space) indicates a port and a specific MIDI channel on which to transmit note messages. Channel numbers greater than 16 will be wrapped around to stay within the 1-16 range.
- **device** (`symbol`) _(optional)_ — MIDI output device
  The name of a MIDI output device may be used as the first argument to specify the port. The name 'none' can be used to prevent the object from transmitting MIDI messages on any port.
- **channel** (`int`) — Extended MIDI channel
  A number alone can be used in place of a letter and number combination. The exact meaning of the channel number argument depends on the channel offset specified for each port in the MIDI Setup dialog.

## Messages

- `int(input: int)` — Store pitch and cause output
  In left inlet: The number is the pitch value of a MIDI note message transmitted on the specified channel and port. Numbers are limited between 0 and 127.
- `float(input: float)` — Function depends on inlet
  Converted to int.
- `anything(port: list)` — Set the MIDI output device
  Performs the same function as port but without need for the word, "port".
- `in1(velocity: int)` — Store velocity value
  In middle inlet: The number is stored as the velocity of a note message, to be used with pitch values received in the left inlet. Numbers are limited between 0 and 127. 0 is considered a note-off message, 1-127 are note-on messages.
- `in2(channel: int)` — Store the MIDI channel for output
  In right inlet: The number is stored as the channel number on which to transmit the note-on messages.
- `port(port: symbol)` — Set the MIDI output device
  In left inlet: The word port, followed by a letter a-z or the name of a MIDI output port or device, specifies the port used to transmit the MIDI messages. The word port is optional and may be omitted. The name 'none' can be used to prevent the object from transmitting MIDI messages on any port.

## GUI behaviors

- `(mouse)` — Select a MIDI output device
  Double-clicking on a noteout object shows a pop-up menu for choosing a MIDI port or device.

## Attributes

- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### outport

> About Port and Channel Arguments for MIDI Input

```
Example #1 — [noteout]  use the middle outlet to connect to the MIDI object
  fan-in:
    in0 ← [umenu] ← [midiinfo] ← [message "1"]    # the midiinfo object can be used to fill a menu with the names of all the current output devices / click to get list of ports
```

```
Example #2 — [noteout]  cannot change channel number
  fan-in:
    in0 ← [message "port "from Max 1""]    # change port
    in0 ← [message "port b"]
    in0 ← [message "port a"]
```

```
Example #3 — [noteout b 6]  sends channel 6 on the device abbreviated with b
  (no patch cords)
```

```
Example #4 — [noteout a]  Examples:
  (no patch cords)
```

```
Example #5 — [noteout 17]
  (no patch cords)
```

```
Example #6 — [noteout a 1]  sends on channel 1 on the device with a channel offset of 16 / sends on channel 1 on the device abbreviated with a / sends all channels on the device abbreviated with a
  (no patch cords)
```

### basic

```
Example — [noteout 1]  Double click to see available MIDI ports
  fan-in:
    in0 ← [makenote 127 200] ← [kslider]
    in0 ← [message "60 0"]    # note-off
    in0 ← [message "60 80"]    # note-on
    in1 ← [makenote 127 200] ← [kslider]
    in2 ← [number] ← [dial]
```

## See also

`ctlout`, `midiout`, `notein`, `nslider`, `xbendout`, `xnoteout`
