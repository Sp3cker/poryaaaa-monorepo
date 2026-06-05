# midiin

_max · MIDI_

> Output raw MIDI data

Listens to a specified MIDI port and output the raw MIDI data received.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | port Message Sets MIDI Input Port/Device |
| out0 | Raw MIDI Messages |

## Arguments

- **port** (`symbol`) _(optional)_ — MIDI port ID
  Specifies the port (a-z) from which to receive incoming MIDI messages. If there is no argument, midiin receives from port a (or the first input port listed in the MIDI Setup dialog.)
- **device** (`symbol`) _(optional)_ — MIDI input device
  The name of a MIDI input device may be used as the first argument to specify the port. The name 'none' can be used to prevent the object from receiving MIDI messages from any port.

## Messages

- `anything(port: list)` — Set the MIDI input device
  Performs the same functions as port.
- `port(port: symbol)` — Set the MIDI input device
  The word port, followed by a letter a- z or the name of a MIDI input port or device, sets the port from which the object receives incoming MIDI messages. The word port is optional and may be omitted. The name 'none' can be used to prevent the object from receiving MIDI messages from any port.

## GUI behaviors

- `(mouse)` — Select MIDI device
  Double-clicking on a midiin object shows a pop-up menu for choosing a MIDI port or device.
- `(MIDI)` — Output raw MIDI bytes
  The midiin object receives all MIDI messages from a MIDI input device.

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

### basic

> midiin produces MIDI messages as a series of individual bytes, and is useful as input to objects like midiparse, seq, xnotein, xbendin and midiselect

```
Example #1 — [midiin b]  double click to select port
  fan-out:
    out0 → [print port B]:in0
```

```
Example #2 — [midiin a]
  fan-out:
    out0 → [print port A]:in0
```

```
Example #3 — [midiin]
  fan-in:
    in0 ← [umenu] ← [midiinfo] ← [loadmess 0]    # Select an input port for midiin
  fan-out:
    out0 → [print]:in0
```

## See also

`xmidiin`, `midiformat`, `midiinfo`, `midiformat`, `midiparse`, `mpeconfig`, `mpeformat`, `mpeparse`, `noteout`, `polymidiin`, `sxformat`, `xbendout`, `xnoteout`, `rtin`, `sysexin`, `xnotein`, `xbendin`
