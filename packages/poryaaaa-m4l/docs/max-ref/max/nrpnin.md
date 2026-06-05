# nrpnin

_max · MIDI_

> Output received NRPN values

Output the value from a specific Non-Registered Parameter Number (NRPN) and MIDI channel.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Raw MIDI Bytes From midiin |
| out0 | Parameter Value |
| out1 | Parameter Change Delta |
| out2 | Non-Registered Parameter Number |
| out3 | MIDI Channel |

## Arguments

- **param-channel** (`list`) _(optional)_ — Parameter number and channel
  The initial argument is a single non-registered parameter number (in the range 0-16383) to be recognized by nrpnin. The parameter number can also be expressed as a symbol formatted 'MSB:LSB' (for instance, '1:0' would specify parameter 128). If there is no parameter number, or if the argument is a negative number, nrpnin recognizes all non-registered parameter numbers. If a single parameter number is specified in the argument, the outlet which normally sends the parameter number is unnecessary, and is not created.

 Following the controller number argument is a single channel number on which to receive parameter messages. If the channel argument is not present, nrpnin receives parameter messages on all channels. In order for this argument to be used, a parameter number argument must precede it. To specify a channel number without specifying a parameter number, use -1 for the parameter number. The channel number is clipped to the range 1-16.

## Messages

- `int(input: int)` — Evaluate as 14-bit MIDI NRPN data.
  The numbers are individual bytes of a MIDI message stream, received from an object such as midiin.
- `list(input: list)` — Evaluate as 14-bit MIDI NRPN data.
  The numbers are bytes of a MIDI message stream, received from an object such as midiin.
- `set([parameter: int])` — Set the current parameter number.
  The message set without any arguments, or with an argument of -1, will cause nrpnin to output incoming 14-bit MIDI non-registered parameter messages from any non-registered parameter. An argument between 0 and 16383 ((1 nrpnin to only output incoming 14-bit MIDI non-registered parameter messages from the parameter specified.
  The set message also accepts an argument list comprising most signficant byte (MSB) followed by least significant byte (LSB). The 7-bit MSB will be bitshifted and ORd with the 7-bit LSB to specify a 14-bit parameter number. For instance, set 1 0 would specify non-registered parameter number 128.
  The parameter number argument can also be expressed as a symbol formatted 'MSB:LSB'. For instance, set 1:0 would specify non-registered parameter number 128.

## Attributes

- `@default` (int)
- `@label` (symbol)
- `@save` (int)
- `@style` (symbol)

## Help patcher examples

### basic

```
Example #1 — [nrpnin 145 1]
  fan-in:
    in0 ← [midiin] ← [umenu] ← [midiinfo]
    in0 ← [message "set 0 81"]    # set parameter number as a list (MSB LSB)
    in0 ← [message "set 155"]    # set parameter number as a 14-bit value
  fan-out:
    out0 → [number]:in0    # value (14 bit)
    out1 → [number]:in0    # delta
```

```
Example #2 — [nrpnin @hires 0]  when hires mode is off, the NRPN value is limited to a 7-bit value on CC 6.
  fan-in:
    in0 ← [midiin] ← [umenu] ← [midiinfo]
  fan-out:
    out0 → [number]:in0    # value
    out1 → [number]:in0    # delta
    out2 → [number]:in0    # parameter (14-bit)
    out3 → [number]:in0    # channel
```

## See also

`midiin`, `ctlin`, `nrpnout`, `xctlin`, `xbendin`, `xnotein`, `rpnin`, `rpnout`
