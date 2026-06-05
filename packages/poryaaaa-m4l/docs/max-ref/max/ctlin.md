# ctlin

_max · MIDI_

> Output received MIDI control values

Output the value from a specific controller number and MIDI channel.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | port Message Sets MIDI Input Port/Device |
| out0 | Controller Value |
| out1 | Controller Number |
| out2 | MIDI Channel |

## Arguments

- **port** (`symbol`) _(optional)_ — MIDI port ID
  Specifies the port (a-z) from which to receive incoming control messages. If there is no letter present as an argument, ctlin can receive from all ports.
- **device** (`symbol`) _(optional)_ — MIDI input device
  The name of a MIDI input device may be used as the first argument to specify the port. The name 'all' can be used to enable the reception of MIDI messages from any port. The name 'none' can be used to prevent the object from receiving MIDI messages from any port.
- **ctrllr-channel** (`list`) _(optional)_ — Controller and channel
  Following the (optional) port argument, the next argument is a single controller number to be recognized by ctlin. If there is no controller number, or if the argument is a negative number, ctlin recognizes all controller numbers. If a single controller number is specified in the argument, the outlet which normally sends the controller number is unnecessary, and is not created.

 Following the controller number argument is a single channel number on which to receive control messages. If the channel argument is not present, ctlin receives control messages on all channels. In order for this argument to be used, a controller number argument must precede it. To specify a channel number without specifying a controller number, use -1 for the controller number.

 If a single channel number is specified as an argument, the outlet which normally sends the channel number is unnecessary, and is not created. If a port has been specified with a letter argument, channel numbers greater than 16 will be wrapped around to stay within the 1-16 range. If no port argument is present, a channel number can be used in place of a letter and number combination. The exact meaning of the channel number argument depends on the channel offset specified for each port in the MIDI Setup dialog.

## Messages

- `anything(port: list)` — Set the MIDI input device
  Performs the same function as port.
- `port(port: symbol)` — Set the MIDI input device
  The word port, followed by a letter a- z or the name of a MIDI input port or device, sets the port from which the object receives incoming control messages. The word port is optional and may be omitted. The name 'all' can be used to enable the reception of MIDI messages from any port. The name 'none' can be used to prevent the object from receiving MIDI messages from any port.
- `set(controller: int)` — Set a single controller for output
  The word set, followed by a number from 0 to 127, specifies a single controller number to be paid attention to by the ctlin object. If the ctlin object was created with no argument, set with an argument of -1 will cause the object to listen for all controller numbers (otherwise, set -1 will be ignored).

## GUI behaviors

- `(mouse)` — Select a MIDI input device
  Double-clicking on a ctlin object shows a pop-up menu for choosing a MIDI port or device.
- `(MIDI)` — Output control change message
  ctlin receives its input from a MIDI control change message received from a MIDI input device.

## Attributes

- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### inport

> cannot change channel number
>
> click to get list of ports
>
> the midiinfo object can be used to fill a menu with the names of all the current input devices
>
> use the middle outlet to connect to the MIDI object

> About Port and Channel Arguments for MIDI Input

### more

```
Example #1 — [ctlin a 1]  or use port abbreviation
  fan-in:
    in0 ← [umenu] ← [midiinfo] ← [message "1"]    # set port / click to get list of ports
  fan-out:
    out0 → [number]:in0
    out1 → [number]:in0
```

```
Example #2 — [ctlin 33 1]
  fan-in:
    in0 ← [message "set 64"]    # change the controller number
  fan-out:
    out0 → [number]:in0
```

### basic

```
Example #1 — [ctlin -1 4]  -1 argument receives all controller numbers on a specific channel
  fan-out:
    out0 → [number]:in0    # Control value
    out1 → [number]:in0    # Controller number
```

```
Example #2 — [ctlin]  Double click to set port
  fan-out:
    out0 → [number]:in0    # Control value
    out1 → [number]:in0    # Controller number
    out2 → [number]:in0    # MIDI channel
```

## See also

`bendin`, `ctlout`, `midiin`, `notein`, `rtin`, `xbendin`
