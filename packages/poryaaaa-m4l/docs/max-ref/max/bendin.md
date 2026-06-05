# bendin

_max · MIDI_

> Output MIDI pitch bend values

Outputs pitch bend values received from a MIDI device. The MIDI port and channel can be chosen with messages or by double-clicking on the object.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | port Message Sets MIDI Input Port/Device |
| out0 | Pitch Bend Amount |
| out1 | MIDI Channel |

## Arguments

- **port** (`symbol`) _(optional)_ — MIDI port (a-z)
  Specifies the port from which to receive incoming pitch bend messages. If there is no argument, bendin receives from all channels on all ports.
- **midi-device** (`symbol`) _(optional)_ — MIDI device name
  The name of a MIDI input device may be used as the first argument to specify the port. The name 'all' can be used to enable the reception of MIDI messages from any port. The name 'none' can be used to prevent the object from receiving MIDI messages from any port.
- **port-and-channel** (`symbol`) — MIDI port (a-z) and channel
  A letter and number combination (separated by a space) indicates a port and a specific MIDI channel on which to receive pitch bend messages. Channel numbers greater than 16 will be wrapped around to stay within the 1-16 range.
- **channel** (`int`) — Extended MIDI channel number
  A number alone can be used in place of a letter and number combination. The exact meaning of the channel number argument depends on the channel offset specified for each port in the MIDI Setup dialog.

## Messages

- `anything(arguments: list)` — See the port message
- `port(MIDI-port: symbol)` — Set the MIDI port
  The word port, followed by a letter a-z or the name of an MIDI port or device, sets the port from which the object receives incoming pitch bend messages. The word port is optional and may be omitted. The name 'all' can be used to enable the reception of MIDI messages from any port. The name 'none' can be used to prevent the object from receiving MIDI messages from any port.

## GUI behaviors

- `(mouse)` — Select a MIDI port
  Double-clicking on a bendin object shows a pop-up menu for choosing a MIDI port or device.
- `(MIDI)` — Receive MIDI input
  The bendin object receives its input from a MIDI pitch bend message received from a MIDI input device.

## Attributes

- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in0` — port Message Sets MIDI Input Port/Device

### inport

> cannot change channel number
>
> click to get list of ports
>
> the midiinfo object can be used to fill a menu with the names of all the current input devices
>
> use the middle outlet to connect to the MIDI object

> About Port and Channel Arguments for MIDI Input

### basic

```
Example — [bendin]
  fan-out:
    out0 → [number]:in0    # pitch bend value
    out1 → [number]:in0    # channel number
```

## See also

`bendout`, `ctlin`, `midiin`, `notein`, `rtin`, `xbendout`, `xbendin`
