# borax

_max · Data, Timing_

> Report note-on and note-off information

Acquires and outputs comprehensive information regarding note-on and note-off events. Information includes note counts, event details and time between note events.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Pitch (Also Delta Message) |
| in1 | Velocity |
| in2 | Reset, Turns Off All Sounding Notes |
| out0 | Number of Notes Since Last Reset |
| out1 | Voice Allocation Number |
| out2 | Number of Notes Currently Active |
| out3 | Pitch |
| out4 | Velocity |
| out5 | Duration Count |
| out6 | Duration Output |
| out7 | Note Delta Count |
| out8 | Delta Time |

## Messages

- `bang` — Clear data and send note-offs
  In right inlet: Resets borax by sending note-offs for all notes currently being held, erasing the borax object's memory of all notes received, and setting its counters and its clock to 0.
- `int(pitch: int)` — Store MIDI pitch data
  In left inlet: The number is the pitch value of a MIDI note-on message or note-off message (note-on with a velocity of 0). The pitch is paired with the velocity in the middle inlet. borax ignores note-on messages for pitches it is already holding, and ignores note-off messages for pitches that have already been turned off. If the note is not a duplicate, borax sends out the pitch and velocity values, as well as other information.
- `delta` — Output delta-time and delta-count
  Causes the delta time (the time elapsed since the last note-on) and the delta count (the number of delta times that have been reported) to be sent out.
- `in1(velocity: int)` — Store MIDI velocity data
  In middle inlet: The number is stored as the velocity, to be paired with pitch numbers received in the left inlet.
- `list(pitch: int, velocity: int)` — Store MIDI pitch and velocity
  The second number is stored as the velocity, and the first number is used as the pitch, of a pitch-velocity pair. If the note is not a duplicate, borax sends out the pitch and velocity values, as well as other information.

## Attributes

- `@default` (int)
- `@label` (symbol)

## Help patcher examples

### basic

```
Example — [borax]
  fan-in:
    in0 ← [notein]
    in1 ← [notein]
    in2 ← [button]
  fan-out:
    out0 → [number]:in0    # Event number associated with pitch and velocity report
    out1 → [number]:in0    # Voice Allocation Number. Use this as an index to refer to a note when storing the other information from Borax
    out2 → [number]:in0    # Number of notes currently held down
    out3 → [number]:in0    # Pitch of incoming note
    out4 → [number]:in0    # Velocity of incoming note (0 for note-off)
    out5 → [number]:in0    # Event number associated with duration report
    out6 → [number]:in0    # Duration value (sent out with note-off)
    out7 → [number]:in0    # Event number associated with delta-time report
    out8 → [number]:in0    # Delta-time between note-ons
```

## See also

`midiparse`, `poly`
