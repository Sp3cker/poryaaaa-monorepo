# midiinfo

_max · MIDI_

> Fill a pop-up menu with MIDI device names

Outputs a series of messages which will set up a pop-up menu to a list of MIDI output devices when a bang is received. A number in midiinfo's right inlet creates a list of MIDI input devices.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | bang, int List Outputs, int Sets Menu Item |
| in1 | bang, int List Inputs, int Sets Menu Item |
| out0 | Connect to menu Object |

## Messages

- `bang` — Retrieve MIDI output devices
  In left inlet: Same as int, but doesn't send a set message after setting the umenu items. The equivalent message to bang for retrieving input device names is -1 in the right inlet.
- `int(index: int)` — Function depends on inlet
  In left inlet: Causes midiinfo to send out a series of messages containing the names of the current MIDI output devices. Those messages can be used to set the individual items of a pop-up umenu object connected to the midiinfo object's outlet. The number received in the midiinfo object's left inlet is then sent in a set message to set the currently displayed umenu item. In right inlet: Causes midiinfo to send out a series of messages containing the names of the current MIDI input devices. Those messages can be used to set the individual items of a pop-up umenu object connected to the midiinfo object's outlet. The number received in the midiinfo object's right inlet is then sent in a set message to set the currently displayed umenu item, unless the number is less than zero, in which case no set message is sent.
- `controllers(index: int)` — Retrieve MIDI controller list
  In left inlet: Causes midiinfo to send out a series of messages containing the names of all MIDI controllers (devices that transmit MIDI) in the current MIDI setup. Those messages can be used to set the individual items of a pop-up umenu object connected to the midiinfo object's outlet. The word controllers may be followed by a number, which sets the pop-up umenu to that item number after the menu items have been created.

## Attributes

- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in1` — bang, int List Inputs, int Sets Menu Item

### basic

```
Example #1 — [midiinfo]
  fan-in:
    in0 ← [message "controllers"]    # Create menu of output devices / Create menu of controllers (input devices)
  fan-out:
    out0 → [umenu]:in0    # Connect middle outlet of umenu to a MIDI input or output object
```

```
Example #2 — [midiinfo]
  fan-in:
    in0 ← [button]
    in0 ← [message "1"]
  fan-out:
    out0 → [umenu]:in0
```

## See also

`midiin`, `midiout`, `umenu`
