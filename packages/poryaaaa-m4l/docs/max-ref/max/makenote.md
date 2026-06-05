# makenote

_max · Notes_

> Generate a note-on/note-off pair

Outputs a MIDI note-on message paired with a velocity value followed by a note-off message after a specified amount of time. This allows for generative MIDI output without having to manage note-off generation.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Pitch |
| in1 | Velocity |
| in2 | Duration |
| out0 | Pitch Output |
| out1 | Velocity Output (Note-on Velocity, Then 0) |

## Arguments

- **velocity** (`number`) _(optional)_ — Set initial velocity
  Sets an initial velocity value (int or float) to be paired with incoming pitch numbers. If there is no argument, the initial velocity is 0.
- **duration** (`number`) _(optional)_ — Set initial duration
  Sets an initial note duration (time before a note-off is sent out), in any of Max's time units. A simple int or float will set the duration in milliseconds. If the second argument is not present, the note-off follows the note-on immediately.
- **channel** (`number`) _(optional)_ — Set initial MIDI channel
  Sets an initial MIDI channel number. If a third argument is used, the makenote object will have an additional fourth inlet (which specifies MIDI channel number) and an additional third outlet (which specifies MIDI output channel).

## Messages

- `int(input: int)` — MIDI note information
  In first inlet: The number is treated as a pitch value for a MIDI note-on message. It is paired with a velocity value and the numbers are sent out the outlets. After a certain time, a note-off message (a note-on with a velocity of 0) is sent out for that pitch.
  In second inlet: The number is stored as a velocity to be paired with pitch numbers received in the left inlet.
  In third inlet: The number is stored as the duration that makenote waits before a note-off message is sent out.
  In fourth inlet: The number specifies a MIDI output channel. The fourth inlet will only be present if the makenote object is initialized with three arguments.
- `float(input: float)` — MIDI note information
  In first inlet: The number is treated as a pitch value for a MIDI note-on message. It is paired with a velocity value and the numbers are sent out the outlets. After a certain time, a note-off message (a note-on with a velocity of 0) is sent out for that pitch.
  In second inlet: The number is stored as a velocity to be paired with pitch numbers received in the left inlet.
  In third inlet: The number is stored as the duration that makenote waits before a note-off message is sent out.
  In fourth inlet: The number specifies a MIDI output channel. The fourth inlet will only be present if the makenote object is initialized with three arguments.
- `list(input: list)` — Set all MIDI note information
  In left inlet: The second number is treated as the velocity and is sent out the right outlet. The first number is treated as the pitch and is sent out the left outlet. A corresponding note-off message is sent out later.
  If the makenote object is instantiated with three arguments, a four-item list can be used which contains an additional fourth element specifying the MIDI channel number, which is sent out the rightmost outlet of the object.
- `anything(input: list)` — Set all MIDI note information
  Performs the same function as list.
- `clear` — Clear all stored notes
  Erases all notes currently held by makenote, without sending note-offs.
- `clock(clock-name: symbol)` — Select a clock source
  The word clock, followed by the name of an existing setclock object, sets the makenote object to be controlled by that setclock object rather than by Max’s internal millisecond clock. The word clock by itself sets the makenote object back to using Max’s regular millisecond clock.
- `stop` — Send note-off messages
  Causes makenote to send out immediate note-offs for all pitches it currently holds.

## Attributes

- `@category` (symbol)
- `@default` (atom, size 2)
- `@label` (symbol)
- `@style` (symbol)
- `@units` (atom, size 7)

## Help patcher examples

### Repeatmodes

```
Example #1 — [makenote 127 2000 @repeatmode 4]
  fan-in:
    in0 ← [message "60"]
    in0 ← [message "62"]    # repeated notes will be ignored until the previously scheduled note-off message is sent.
  fan-out:
    out0 → [pack]:in0
    out1 → [pack]:in1
```

```
Example #2 — [makenote 127 2000 @repeatmode 3]
  fan-in:
    in0 ← [message "62"]    # like @repeatmode 2, but repeating a note will simply reschedule the note-off without retriggering a note-on message.
    in0 ← [message "60"]
  fan-out:
    out0 → [pack]:in0
    out1 → [pack]:in1
```

```
Example #3 — [makenote 127 2000 3 @repeatmode 2]  use an optional third argument for specifying the MIDI channel for the note on and the note off messages
  fan-in:
    in0 ← [message "60"]
    in0 ← [message "62"]    # send only one note off message at the end of the last note
  fan-out:
    out0 → [pack]:in0
    out1 → [pack]:in1
    out2 → [number]:in0    # midi channel
```

```
Example #4 — [makenote 127 2000 @repeatmode 1]
  fan-in:
    in0 ← [message "60"]
    in0 ← [message "62"]    # if the note was already playing, send a note off and retrigger the note
  fan-out:
    out0 → [pack]:in0
    out1 → [pack]:in1
```

```
Example #5 — [makenote 127 2000]
  fan-in:
    in0 ← [message "62"]    # normal mode
    in0 ← [message "60"]
  fan-out:
    out0 → [pack]:in0
    out1 → [pack]:in1
```

### more

```
Example #1 — [makenote 96 4n]
  fan-in:
    in0 ← [kslider]
    in1 ← [kslider]
    in2 ← [umenu]    # duration may also be specified in any time value
  fan-out:
    out0 → [noteout]:in0
    out1 → [noteout]:in1
```

```
Example #2 — [makenote 128 500 1]
  fan-in:
    in0 ← [random 128] ← [button] ← [metro 100]
    in3 ← [+ 1] ← [random 16] ← [button]
  fan-out:
    out0 → [noteout]:in0
    out1 → [noteout]:in1
    out2 → [noteout]:in2
```

### basic

```
Example — [makenote 60 1000]
  fan-in:
    in0 ← [number] ← [kslider]    # int in left sets pitch and starts a note.
    in0 ← [message "clear"]    # Cancel future note-offs
    in0 ← [message "stop"]    # Send all note-offs out now
    in1 ← [number]    # velocity
    in2 ← [number]    # duration in ms
  fan-out:
    out0 → [number]:in0    # Pitch
    out1 → [number]:in0    # Velocity
```

## See also

`flush`, `midiout`, `noteout`, `nslider`, `stripnote`, `transport`, `xnoteout`
