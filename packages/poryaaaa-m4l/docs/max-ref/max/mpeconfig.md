# mpeconfig

_Max · MIDI_

> Configure a MIDI device that supports Multidimensional Polyphonic Expression (MPE) messages

The mpeconfig object is used to set up and configure zones for interpreting incoming MIDI to MPE devices.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | MPE Configuration Messages |
| out0 | MIDI Output |
| out1 | midievent Message Output |

### Port details

**`out0` (MIDI Output):** Out left outlet: MIDI messages are sent out as individual bytes.

**`out1` (midievent Message Output):** Out right outlet: A formatted midievent message for use with the vst~ object.

## Messages

- `bang` — Define a zone
  The bang message causes a zone to be defined based on the current settings of masterchan and chanrange. You can have a maximum of 7 zones.
- `clear` — Clear all zones
  The clear message clears all currently configured zones and returns them to their default state.
- `createzone(master-channel: int, note-bend-range: int)` — Single-step zone definition
  The word createzone, followed by a list of two integers that define the Master channel and the Note bend range, will define a zone. No bang message is required when using this message. You can have a maximum of 7 zones configured in any way you wish amongst the 16 channels.
- `masterbendrange(max-bend-range: int)` — Set the master bend range
  The word masterbendrange, followed by an integer that specifies the maximum pitch bend range in semitones, will set the pitch bend range for all zones. that define the Master channel and the Note bend range, will define a zone. No bang message is required when using this message.
- `notebendrange(max-bend-range: int)` — Set the note bend range for a master channel
  The word notebandrange, followed by an integer that specifies the maximum pitch bend range in semitones, will set the pitch bend range for a specified master channel.
  Note: If no master channel is set using a @masterchan attribute, the notebandrange message must always be preceded by a masterchan message to select a master channel, and followed by a bang message to define the zone.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `masterchan` — seen as: `masterchan 1, chanrange 4`, `masterchan 3, masterbendrange 24, bang`, `masterchan 3, notebendrange 24, bang`

## Help patcher examples

### basic

> @masterchan and @chanrange can be set as attributes (for notebendrange, bang, and masterbendrange messages)

```
Example — [mpeconfig @masterchan 1 @chanrange 15]  MIDI output -- send to a MIDI controller and/or mpeparse (to set up zones for interpreting incoming MIDI)
  fan-in:
    in0 ← [message "clear"]    # clear all zones
    in0 ← [message "masterchan 1, chanrange 4"]    # one-step zone definition (masterchan, chanrange) no bang is required after this message
    in0 ← [button]    # bang defines a zone, based on the current masterchan and chanrangesettings
    in0 ← [message "createzone 1 15"]
    in0 ← [message "masterchan 3, masterbendrange 24, bang"]    # set pitch bend range for global pitch bend messages
    in0 ← [message "masterchan 3, notebendrange 24, bang"]    # set pitch bend range for note-specific pitch bend messages
  fan-out:
    out0 → [mpeformat]:in0
    out1 → [message "midievent 177 6 4"]:in1    # The rightmost outlet of the mpeconfig object converts MIDI input into properly formatted midievent messages for use with the vst~ object.
```

## See also

`midiin`, `midiformat`, `midiparse`, `mpeformat`, `mpeparse`, `polymidiin`
