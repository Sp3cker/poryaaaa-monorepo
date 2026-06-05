# live.miditool.out

_m4l · Live MIDI Objects_

> Set or replace notes of MIDI clips in Live. Designed for use in a MIDI Tool Generator or MIDI Tool Transformation.

The live.miditool.out object is used in a MIDI Tool Generator or MIDI Tool Transformation and is responsible for setting or replacing notes of the currently selected clip in Live. In combination with the live.miditool.in object, you can develop a generative or transformative process that can be controlled and triggered from the Tool Tabs.

## Messages

- `dictionary(ARG_NAME_0: list)` — Sets the currently selected clip to the notes contained in the dictionary.
  When live.miditool.out receives a dictionary, it will set the currently selected clip to the notes contained in the dictionary. The dictionary must be formatted in the same way as the dictionary sent out from the live.miditool.in object.
  For an AMXD with the "Note Transformation" type, the notes in the dictionary become the new state of the clip. For an AMXD with the "Note Generator" type, the notes in the dictionary are added to the clip and can be superimposed on existing notes.

## Help patcher examples

### dictionary format

> Dictionary format
>
> Outlining the format the live.miditool.out needs and expects in order to work
>
> live.miditool.out only functions when used inside of a Max for Live Generator or Transformation. You will need to copy the Max patch code into a Transformation or Generator device in Live.

> The live.miditool.out object expects a dictionary in order to create or update notes in a MIDI clip. The dictionary that live.miditool.out expects is a lot like the dictionary that is output by live.miditool.in, however, this is different dependning on if you are creating a generator or a transformation. At the highest level, you need a dictionary "notes" key which is associated to an array of dictionaries, where each dictionary represents a note. A note MUST contain pitch, start_time and duration information. Everything else is optional and hass a fallback value. If you are familiar with the notes API, this is similar to the add_new_notes function. note_id: [int] the unique note identifier. pitch: [int] the MIDI note number, 0...127, 60 is C3. start_time: [float] the note start time in beats of absolute clip time. duration: [float] the note length in beats. velocity (optional): [float] the note velocity, 0 ... 127 (100 by default). mute (optional): [bool] 1 = the note is deactivated (0 by default). probability (optional): [float] the chance that the note will be played: 1.0 = the note is always played 0.0 = the note is never played (1.0 by default). velocity_deviation (optional): [float] the range of velocity values at which the note can be played: 0.0 = no deviation; the note will always play at the velocity specified by the velocity property -127.0 to 127.0 = the note will be assigned a velocity value between velocity and velocity + velocity_deviation, inclusive; if the resulting range exceeds the limits of MIDI velocity (0 to 127), then it will be clamped within those limits (0.0 by default). release_velocity (optional): [float] the note release velocity (64 by default). Returns a list of note IDs of the added notes.

```
Example — [live.miditool.out]
  fan-in:
    in0 ← [dict.pack notes:] ← [dict.pack pitch:48 start_time:0.25 duration:1.0] ← [t b]    # We convert the dictionary to a bang, in this case we don't really care about the information that is in the MIDI clip already.
```

### basic

> The live.miditool.out object is responsible for updating the contents of a MIDI clip. As long as you provide a valid dictionary to the inlet, it will work. A valid dictionary contains a "notes" key, and an array of note dictionaries each containing at least a "pitch", "start_time" and "duration" key. You can of course alter other properties of the note such as: - velocity - mute - probability - velocity_deviation - release_velocity A basic generator is show to the right.

> Clear the notes array so it's fresh each time we change something. Bang 16 times (to generate 16 notes).

> This holds an array of note dictionaries (dictionaries that describe the property of each note in a MIDI clip).

```
Example — [live.miditool.out]
  fan-in:
    in0 ← [dict.pack notes:] ← [array @name ---notes]    # Pack the array into a dictionary and associate it with the "notes" key.
```

## See also

`JS API`, `m4l/live_api_overview`, `m4l/live_api`, `m4l/live_miditools`, `Live Object Model`, `live.miditool.in`, `live.object`, `live.path`, `live.observer`
