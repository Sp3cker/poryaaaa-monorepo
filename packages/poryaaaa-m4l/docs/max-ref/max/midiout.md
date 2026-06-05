# midiout

_max · MIDI_

> Transmit raw MIDI data

Transmits raw MIDI data to a specified port.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Raw MIDI Messages |

## Arguments

- **port** (`symbol`) _(optional)_ — MIDI port ID
  Specifies the port (a-z) for transmitting MIDI data. If there is no argument, midiout transmits out port a (or the first output port listed in the MIDI Setup dialog.)
- **device** (`symbol`) _(optional)_ — MIDI output device
  The name of a MIDI output device may be used as the first argument to specify the port. The name 'none' can be used to prevent the object from transmitting MIDI messages on any port.

## Messages

- `int(byte: int)` — Send a byte to a MIDI device
  The number is transmitted as a byte of a MIDI message to the specified port.
- `float(message: float)` — Send a byte to a MIDI device
  Converted to int.
- `list(bytes: list)` — Send values to a MIDI device
  The numbers are transmitted sequentially as individual bytes of a MIDI message to the specified port.
- `anything(port: list)` — Set the MIDI output device
  Performs the same function as port.
- `port(port: symbol)` — Set the MIDI output device
  The word port, followed by a letter a-z or the name of a MIDI output port or device, specifies the port used to transmit the MIDI messages. The word port is optional and may be omitted. The name 'none' can be used to prevent the object from transmitting MIDI messages on any port.

## GUI behaviors

- `(mouse)` — Select MIDI output device
  Double-clicking on a midiout object shows a pop-up menu for choosing a MIDI port or device.

## Attributes

- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `b` — seen as: `b`

## Help patcher examples

### outport

> About Port and Channel Arguments for MIDI Input

### basic

```
Example — [midiout]
  fan-in:
    in0 ← [message "144, 60, 0"]    # stop the note
    in0 ← [message "144, 60, 60"]    # start a note
    in0 ← [message "port a"]
    in0 ← [message "b"]    # Switch to another port
```

## See also

`midiformat`, `midiin`, `midiinfo`, `midiparse`, `midiselect`, `mpeconfig`, `mpeformat`, `mpeparse`, `noteout`, `polymidiin`, `sxformat`, `xbendout`, `xnoteout`
