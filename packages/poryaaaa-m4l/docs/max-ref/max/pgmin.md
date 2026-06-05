# pgmin

_max · MIDI_

> Receive MIDI program changes

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | port Message Sets MIDI Input Port/Device |
| out0 | Program Change |
| out1 | MIDI Channel |

## Arguments

- **port** (`symbol`) _(optional)_ — MIDI port ID
  Specifies the port (a-z) from which to receive incoming program change messages. If there is no argument, pgmin receives from all channels on all ports.
- **device** (`symbol`) _(optional)_ — MIDI input device
  The name of a MIDI input device may be used as the first argument to specify the port. The name 'all' can be used to enable the reception of MIDI messages from any port. The name 'none' can be used to prevent the object from receiving MIDI messages from any port.
- **port-channel** (`list`) — MIDI port and channel
  A letter (a-z) and number combination (separated by a space) indicates a port and a specific MIDI channel on which to receive program change messages. Channel numbers greater than 16 will be wrapped around to stay within the 1-16 range.
- **channel** (`int`) — Extended MIDI channel
  A number alone can be used in place of a letter and number combination. The exact meaning of the channel number argument depends on the channel offset specified for each port in the MIDI Setup dialog.

## Messages

- `anything(port: list)` — Select MIDI input device/port
  Performs the same function as port but without need for the word, "port".
- `port(port: symbol)` — Select MIDI input device/port
  The word port, followed by a letter a-z or the name of a MIDI input port or device, sets the port from which the object receives incoming program change messages. The word port is optional and may be omitted. The name 'all' can be used to enable the reception of MIDI messages from any port. The name 'none' can be used to prevent the object from receiving MIDI messages from any port.

## GUI behaviors

- `(mouse)` — Select MIDI input device/port
  Double-clicking on a pgmin object shows a pop-up menu for choosing a MIDI port or device.
- `(MIDI)` — Receive MIDI data
  The pgmin object receives its input from a MIDI program message received from a MIDI input device.

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
Example #1 — [pgmin]
  fan-out:
    out0 → [number]:in0
    out1 → [number]:in0    # Program number / MIDI channel
```

```
Example #2 — [pgmin 33]  receive program changes from device with MIDI offset channel 33
  fan-out:
    out0 → [number]:in0
```

## See also

`midiin`, `pgmout`
