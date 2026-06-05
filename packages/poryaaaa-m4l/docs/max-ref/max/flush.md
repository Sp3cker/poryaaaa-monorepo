# flush

_max · Notes_

> Output MIDI note-offs for held notes

Outputss note-off messages for any held note-ons. flush keeps track of all note-ons passed through it, and produces note-off messages for any held notes when it receives a bang message.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Pitch Input, bang Flushes Held Notes |
| in1 | Velocity Input |
| out0 | Pitch Output |
| out1 | Velocity Output |

## Messages

- `bang` — Send note-off message for held notes
  Immediately sends note-offs for any pitches that have passed through as note-ons but not as note-offs by sending 0 out its right outlet followed by a pitch value out its left outlet.
- `int(pitch: int)` — MIDI pitch value
  The number is treated as the pitch value of a pitch-velocity pair and the note is sent out.
- `clear` — Clears all received note data
  In left inlet: Erases any numbers held by flush, without sending any note-offs.
- `in1(velocity: int)` — MIDI velocity value
  The number is stored as the velocity to be paired with numbers received in the left inlet.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in1` — Velocity Input

### basic

```
Example — [flush]
  fan-in:
    in0 ← [button]    # bang tells flush to turn the notes off
    in0 ← [message "$1 64"]
  fan-out:
    out0 → [noteout]:in0
    out0 → [borax]:in0
    out1 → [noteout]:in1
    out1 → [borax]:in1
```

## See also

`bag`, `borax`, `makenote`, `midiflush`, `offer`, `stripnote`, `sustain`
