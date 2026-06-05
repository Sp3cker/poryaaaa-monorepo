# live.miditool.in

_m4l · Live MIDI Objects_

> Retrieve notes and contextual data of MIDI clips in Live. Designed for use in a MIDI Tool Generator or MIDI Tool Transformation.

The live.miditool.in object is used in a MIDI Tool Generator or MIDI Tool Transformation and is responsible for retrieving note and contextual information of the currently selected clip in Live. In combination with the live.miditool.out object, you can develop a generative or transformative process that can be controlled and triggered from the Tool Tabs.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | bang | Trigger output |
| out0 | — | Dictionary containing the notes from Live |
| out1 | — | Dictionary containing the note context |
| out2 | — | Notifications from Live |

## Messages

- `bang` — Triggers the retrieval of the clip data and starts or continues an iteration of an apply cycle.
  A bang will continue or start an iteration of the apply cycle. In either case, a dictionary of notes is provided from the left outlet, and another dictionary of contextual information is provided from the right outlet.

## GUI behaviors

- `(MIDI)` — TEXT_HERE

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out2` — Notifications from Live

### dictionary format

> Dictionary format
>
> This patch describes the format that live.miditool.in outputs
>
> live.miditool.in only functions when used inside of a Max for Live Generator or Transformation. You will need to copy the Max patch code into a Transformation or Generator device in Live.

> The dictionary that it outputs contains an array all of the notes that were selected in the clip. Each note is a dictionary that has several keys associated to data that represnet properties of a note. It will seem familiar if you have used the Notes API before. These properties include: note_id: [int] the unique note identifier. pitch: [int] the MIDI note number, 0...127, 60 is C3. start_time: [float] the note start time in beats of absolute clip time. duration: [float] the note length in beats. velocity: [float] the note velocity, 0 ... 127. mute: [bool] 1 = the note is deactivated. probability: [float] the chance that the note will be played: 1.0 = the note is always played; 0.0 = the note is never played. velocity_deviation: [float] the range of velocity values at which the note can be played: 0.0 = no deviation; the note will always play at the velocity specified by the velocity property -127.0 to 127.0 = the note will be assigned a velocity value between velocity and velocity + velocity_deviation, inclusive; if the resulting range exceeds the limits of MIDI velocity (0 to 127), then it will be clamped within those limits. release_velocity: [float] the note release velocity.

```
Example — [live.miditool.in]  live.miditool.in outputs a dictionary of note information when it receives a bang.
  fan-in:
    in0 ← [button]    # bang causes live.miditool.in to output two dictionaries dictionaries
  fan-out:
    out0 → [dict.view]:in0
    out0 → [dict.unpack notes:]:in0
```

### context (2)

> Note Context
>
> The live.miditool.in object provides a "context" dictionary that contains useful information for building your own MIDI Tools.
>
> live.miditool.in only functions when used inside of a Max for Live Generator or Transformation. You will need to copy the Max patch code into a Transformation or Generator device in Live.

> We can use the grid information from the context outlet also. In this example, we can randomly offset notes within the current grid interval. There is also an intensity control which determines how far within the grid interval a note could possibly shift.

> Here we unpack the grid key from the context. If the grid is enabled we use the grid resolution (in beats), or fall back to quarter notes.

> This subpatch here is responsible for processing the notes and applying the shift. In combination with array.map, we can pass each note through this subpatch as input, and replace it in the original array by passing it back to the object.

```
Example — [live.miditool.in]
  fan-in:
    in0 ← [t b f] ← [live.dial] ← [loadmess 0.5]
  fan-out:
    out0 → [dict.unpack notes:]:in0
    out1 → [dict.unpack grid:]:in0
```

### context (1)

> Note Context
>
> The live.miditool.in object provides a "context" dictionary that contains useful information for building your own MIDI Tools.
>
> live.miditool.in only functions when used inside of a Max for Live Generator or Transformation. You will need to copy the Max patch code into a Transformation or Generator device in Live.

> The right "context" outlet supplies you with a dictionary containing contextual information about the MIDI clip. You can use this alter the behaviour of a transformation or generator, or to make it adaptive with aspects such as the grid size, or scale. The example below will generate an arpeggio in the scale of Live (if there is one) with each note being exactly one division of the set grid (if there is one).

> This portion of the patch creates a note dictionary for a quarter note, where the pitch is randomly selected from the possible scale intervals of the currently selected scale in Live.

```
Example — [live.miditool.in]
  fan-in:
    in0 ← [t b i clear] ← [live.dial]
  fan-out:
    out0 → [t b]:in0
    out1 → [p RandomScaleDegree]:in1
```

### apply cycle

> Apply Cycle
>
> The data live.miditool.in outputs is in part dependent on a behaviour known as the "apply cycle".
>
> live.miditool.in only functions when used inside of a Max for Live Generator or Transformation. You will need to copy the Max patch code into a Transformation or Generator device in Live.

> The apply cycle is an important workflow to understand for MIDI Tool generators and transformations. When you send a bang to the live.miditool.in object it triggers the beginning of an "apply cycle" which always starts by taking a snapshot of the MIDI editor's content in Live at that time. This snapshot is represented as a dictionary and is output from the object's left outlet. This is a useful behavior to have, because it allows you to structure a MIDI tool so that parameters only affect a given point in time, instead of constantly modifying the MIDI notes in the clip. Let us take the example of a pitch shifter MIDI Tool we'd like to make, which has a "Shift Amount" parameter. This parameter determines in semitones how far you would like to shift the pitch. If you change this parameter, you don't want to take a new snapshot of the MIDI notes each time you adjust the shift amount. You are more likely to want to experiment with the shift amount, and dial in the amount you would like based on when you first began tweaking the parameter. An apply cycle ends when you perform a user interaction with another part of Live such as the browser, transport, session view, etc. or if you perform a user interaction within the MIDI editor itself, such as selecting new notes, adjusting a loop length. In other words, the dictionary that you receive from live.miditool.in is always frozen in time at the point at which you started an apply cycle and these cycles can be started and continued by sending a bang to live.miditool.in. If you are already in an apply cycle, the next bang will cause the object to output the same data it snapshotted the first time. If you adjust a parameter then send a bang to live.miditool.in it means that you can adjust the parameter without constantly overwriting the MIDI editor with some new data. The last point to remember is that pressing the apply button in Live causes a new apply cycle to begin and end in one interaction.

```
Example — [live.miditool.in]
  fan-in:
    in0 ← [t b i] ← [live.dial]    # Change this amount and see the results in the MIDI editor.
  fan-out:
    out0 → [dict.unpack notes:]:in0
```

### generator

> Building a generator
>
> An example of how to build a straightforward generator using live.miditool.in
>
> live.miditool.in only functions when used inside of a Max for Live Generator or Transformation. You will need to copy the Max patch code into a Transformation or Generator device in Live.

```
Example — [live.miditool.in]
  fan-in:
    in0 ← [t b i] ← [live.dial]    # Change the number of notes that are generated.
  fan-out:
    out0 → [t b clear b]:in0    # live.miditool.in outputs a dictionary of note information when it receives a bang. We don't need to use this information, and can instead just convert it to a bang, to trigger a generative process.
```

### transformation

> Building a transformation
>
> An example of how to build a straightforward transformation using live.miditool.in
>
> live.miditool.in only functions when used inside of a Max for Live Generator or Transformation. You will need to copy the Max patch code into a Transformation or Generator device in Live.

> This is an example of MIDI Tool which lengthens or shortens the duration of each note by multiplying the "duration" value by a factor.

> Iterate over the array of note dictionaries, multiplying the duration value by the "Stretch" parameter amount. The array.map helps us do this while also reassembling the array back into place.

```
Example — [live.miditool.in]  live.miditool.in outputs a dictionary of note information when it receives a bang.
  fan-in:
    in0 ← [t b f] ← [live.dial]
  fan-out:
    out0 → [dict.unpack notes:]:in0    # unpack the notes key, giving us an array of note dictionaries.
    out0 → [dict.view]:in0
```

### basic

> When live.miditool.in receives a bang or the apply button is pressed from the Live UI, the left outlet outputs a dictionary containing notes from the MIDI editor. The right outlet outputs a dictionary containing contextual information (grid size, start time of first note, end time of last note, etc.). We can process this data, or generate our own and send it back to live.miditool.out to create a note transformation or generator.

```
Example — [live.miditool.in]
  fan-in:
    in0 ← [button]    # bang causes live.miditool.in to output two dictionaries dictionaries
  fan-out:
    out0 → [dict.view]:in0
    out0 → [live.miditool.out]:in0
    out1 → [dict.view]:in0
```

## See also

`JS API`, `m4l/live_api_overview`, `m4l/live_api`, `m4l/live_miditools`, `Live Object Model`, `live.miditool.out`, `live.object`, `live.path`, `live.observer`
