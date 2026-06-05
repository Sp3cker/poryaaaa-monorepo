# midiflush

_max · MIDI_

> Send MIDI note-offs for hanging note-ons

Analyzes a raw MIDI stream (from midiin or seq), counting the number of note-ons received for each note and MIDI channel. When it is sent a bang, MIDI note-off messages are sent for any notes which have not been turned off.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Raw MIDI Data, bang flushes held notes |
| out0 | Raw MIDI Data |

## Messages

- `bang` — Send necessary note-off messages
  When midiflush receives a bang, it outputs MIDI note-off messages for all note-ons which have not been matched by note-offs since the object was created (or the last bang message was sent).
- `int(input: int)` — Evaluate and pass through
  Raw MIDI data from a source such as seq or midiin will be passed through unchanged, while the object observes which note-on messages on each channel have not received matching note-off messages.
- `clear` — Clear stored note-ons
  Erases any note-ons held by midiflush, without sending any note-offs.

## Help patcher examples

### seq example

```
Example — [midiflush]  midiflush goes between seq and midiout
  fan-in:
    in0 ← [button]
    in0 ← [seq]
    in0 ← [message "clear"]    # The "clear" message will cancel any hanging notes in a midiflush.
  fan-out:
    out0 → [midiout]:in0
```

### basic

```
Example — [midiflush]
  fan-in:
    in0 ← [button]    # bang tells flush to turn the notes off
    in0 ← [midiformat] ← [message "$1 64"]
  fan-out:
    out0 → [midiparse]:in0
    out0 → [midiout]:in0
```

## See also

`flush`, `midiin`, `midiinfo`, `midiout`
