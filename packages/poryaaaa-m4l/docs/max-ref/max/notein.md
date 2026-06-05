# notein

_max · MIDI_

> Receive MIDI note messages

Receives its input from a MIDI note-on or note-off message sent by a MIDI input device.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | port Message Sets MIDI Input Port/Device |
| out0 | Pitch |
| out1 | Velocity |
| out2 | MIDI Channel |

## Arguments

- **port** (`symbol`) _(optional)_ — MIDI port ID
  Specifies the port (a-z) from which to receive incoming note messages. If there is no argument, notein receives from all channels on all ports.
- **device** (`symbol`) _(optional)_ — MIDI input device
  The name of a MIDI input device may be used as the first argument to specify the port. The name 'all' can be used to enable the reception of MIDI messages from any port. The name 'none' can be used to prevent the object from receiving MIDI messages from any port.
- **port-channel** (`list`) — MIDI port and channel
  A letter and number combination (separated by a space) indicates a port and a specific MIDI channel on which to receive note messages. Channel numbers greater than 16 will be wrapped around to stay within the 1-16 range.
- **channel** (`int`) — Extended MIDI channel
  A number alone can be used in place of a letter and number combination. The exact meaning of the channel number argument depends on the channel offset specified for each port in the MIDI Setup dialog.

## Messages

- `anything(port: list)` — Set the MIDI input device
  Performs the same function as port without the word, "port".
- `port(port: symbol)` — Set the MIDI input device
  The word port, followed by a letter a-z or the name of a MIDI input port or device, sets the port from which the object receives incoming note messages. The word port is optional and may be omitted. The name 'all' can be used to enable the reception of MIDI messages from any port. The name 'none' can be used to prevent the object from receiving MIDI messages from any port.

## GUI behaviors

- `(mouse)` — Select a MIDI input device
  Double-clicking on a notein object shows a pop-up menu for choosing a MIDI port or device.
- `(MIDI)` — Output received MIDI note messages
  The notein object receives its input from a MIDI note-on or note-off message received from a MIDI input device.

## Attributes

- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in0` — port Message Sets MIDI Input Port/Device

### basic

```
Example — [notein]
  fan-out:
    out0 → [number]:in0
    out1 → [number]:in0    # Pitch / Velocity
    out2 → [number]:in0    # MIDI Channel
```

## See also

`ctlin`, `midiin`, `noteout`, `nslider`, `rtin`, `xbendin`, `xnotein`
