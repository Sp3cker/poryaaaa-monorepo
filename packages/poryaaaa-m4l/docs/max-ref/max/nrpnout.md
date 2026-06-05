# nrpnout

_max · MIDI_

> Format 14-bit MIDI NRPN messages

Format 14-bit MIDI Non-Registered Parameter Number messages to be transmitted using the midiout object.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Parameter Value (0-16383 or list MSB LSB) |
| in1 | Parameter Delta Change (0-127) |
| in2 | Non-Registered Parameter Number (0-16383) |
| in3 | MIDI Channel |
| out0 | Raw MIDI Bytes to midiout |

## Arguments

- **parameter-channel** (`list`) _(optional)_ — Parameter number and channel
  The initial argument is an initial non-registered parameter number (in the range 0-16383) to be used in parameter changes messages formatted by nrpnout. Non-registered parameter numbers are automatically limited between 0 and 16383. The parameter number can also be expressed as a symbol formatted 'MSB:LSB' (for instance, '1:0' would specify parameter 128). If there is no controller number specified, the initial controller number is 0.

 Following the controller number argument is an initial value for the channel number with which to format control messages. If the channel argument is not present, nrpnout initially formats control messages on channel 1. In order for this argument to be used, a controller number argument must precede it. The channel number is clipped to the range 1-16.

## Messages

- `bang` — Output most recent value
  Sends out a non-registered parameter message using the numbers currently stored in nrpnout
- `int(input: int)` — Send 14-bit non-registered parameter value
  The number is a 14-bit non-registered parameter value to be formatted into a complete MIDI non-registered parameter number message by nrpnout.
- `list(msb: int, lsb: int)` — Send 14-bit non-registered parameter value
  A pair of two 7-bit values, most significant byte (MSB) followed by least significant byte (LSB). The 7-bit MSB will be bitshifted and ORd with the 7-bit LSB to specify a 14-bit non-registered parameter value. The 14-bit value will be formatted into a complete MIDI non-registered parameter number message by nrpnout.
- `in1(delta: int)` — Send 7-bit non-registered parameter delta
  In middle-left inlet: The delta will be formatted into a complete MIDI non-registered parameter number message by nrpnout. Deltas outside of the range -127-127 will be ignored.
- `in2(parameter: int)` — Set the active non-registered parameter number
  In middle-right inlet: The number is stored as the non-registered parameter number of the messages transmitted by nrpnout. Parameter numbers outside of the range 0-16383 will be ignored.
- `in3(channel: int)` — Set the MIDI output channel
  In right inlet: The number is stored as the MIDI channel for the continuous controller message sent out by nrpnout. Channel numbers will be clipped to stay within the 1-16 range.
- `set(parameter: list)` — Set the current parameter number.
  An argument between 0 and 16383 ((1 nrpnout.
  The set message also accepts an argument list comprising most signficant byte (MSB) followed by least significant byte (LSB). The 7-bit MSB will be bitshifted and ORd with the 7-bit LSB to specify a 14-bit parameter number. For instance, set 1 0 would specify non-registered parameter number 128.
  The parameter number argument can also be expressed as a symbol formatted 'MSB:LSB'. For instance, set 1:0 would specify non-registered parameter number 128.
  Non-registered parameter numbers outside of the range 0-16383 will be ignored.

## Attributes

- `@default` (int)
- `@label` (symbol)
- `@save` (int)
- `@style` (symbol)

## Help patcher examples

### hires

```
Example — [nrpnout @hires 0]
  fan-in:
    in0 ← [number]    # output value (7-bit)
    in1 ← [number]    # delta value
    in2 ← [number]    # parameter (14-bit)
    in3 ← [number]    # channel
  fan-out:
    out0 → [midiout]:in0
```

### basic

```
Example #1 — [nrpnout @running 1]  when in @running mode, MIDI data is reduced by eliminating redundant status bytes
  fan-in:
    in0 ← [number]    # output value
    in1 ← [number]    # delta value
    in2 ← [number]    # parameter (14-bit)
    in3 ← [number]    # channel
  fan-out:
    out0 → [midiout]:in0
```

```
Example #2 — [nrpnout 110 1]
  fan-in:
    in0 ← [number]    # set value as a 14-bit value (0-16383)
    in0 ← [pak]    # set value as an MSB/LSB list
  fan-out:
    out0 → [midiout]:in0
```

## See also

`midiout`, `ctlout`, `nrpnin`, `xctlout`, `xbendout`, `xnoteout`, `rpnin`, `rpnout`
