# detonate

_max · Sequencing_

> Play a score of note events

Provides score playback managed using Max messages. The score may be loaded from a MIDI file, or generated using Max functions. The score is not limited to MIDI notes and values; any information can be stored and played back with detonate.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Control Messages, Time Parameter When Recording |
| in1 | Pitch Parameter When Recording |
| in2 | Vel Parameter When Recording |
| in3 | Dur Parameter When Recording |
| in4 | Chan Parameter When Recording |
| in5 | Track Parameter When Recording |
| in6 | X1 Parameter When Recording |
| in7 | X2 Parameter When Recording |
| out0 | Time Parameter Output |
| out1 | Pitch Parameter Output |
| out2 | Vel Parameter Output |
| out3 | Dur Parameter Output |
| out4 | Chan Parameter Output |
| out5 | Track Parameter Output |
| out6 | X1 Parameter Output |
| out7 | X2 Parameter Output |

## Arguments

- **label** (`symbol`) — A named context
  Supplies a name for the object. Any detonate objects with the same name argument will share the same event data.

## Messages

- `bang` — Output event then move to next
  Performs the same function as next.
- `int(parameter: int)` — Set a note event parameter
  After a record message has been received, all numbers received are treated as parameters of a note event.
  In left inlet: The delta time (delay), in milliseconds, since the previous recorded event. This denotes the "inter-onset interval --the time between the beginnings of notes--which effectively determines the rhythm in which the events are recorded. This need not necessarily be the true time in which they occur; detonate believes any (non-negative) delta time it receives.
  When detonate receives a number in the left inlet while recording, it treats the number as the inter-onset interval (the time elapsed since the previous event), combines it with the numbers most recently received in the other inlets, and records them together as a note event. As with most Max objects, the numbers received in the other inlets are stored for use in subsequent note events triggered by the receipt of a number in the leftmost inlet.
  When detonate has received a follow message, a subsequent number in the 2nd inlet is treated as the key number (pitch) of a note. If the number is the same as the pitch of the current note in the score (or a nearby note), the information recorded for that note--except for the delta time--is sent out.
  When detonate is neither recording nor following, a number in the left inlet has the same effect as the nth message.
- `float(parameter: float)` — Set a note event parameter
  Converted to int.
- `list(event-values: list)` — Set all values
  The first number in the list is used as the delta time, and the other numbers are treated as if they had been received in the other inlets, respectively from left to right.
- `clear` — Clear all stored content
  Erases the contents of detonate.
- `delay(offset: int)` — Offset event positions
  The message delay followed by a number will move the entire sequence of recorded note values over with the first note of the sequence beginning at the specified number.
- `export(time: int, file-format: int)` — Write a Standard MIDI File to disk
  Same as write.
- `follow(pitch: int)` — Begin score following
  Causes detonate to behave like a score reader, comparing incoming pitch information to the events stored in its score. When a key number is received in the 2nd (pitch) inlet, and it is the same as the pitch of the current note in the score, detonate sends out the information recorded for that event--except for the delta time--and then moves ahead to the next note event.
- `followat(pitch: int, velocity: int, channel: int)` — Find and start following from a specific event
  The word followat, followed by a pitch, a velocity, and a MIDI channel number, causes detonate to look for a note event with those attributes in its stored score. If such a note is found, detonate commences score-following from the next event onward. If not, it simply prints detonate: note not found in the Max Console.
- `import(filename: list)` — Load a Standard MIDI File
  Same as read.
- `in1(pitch: int)` — Set key number of a note event
  In 2nd inlet: The number is treated as the key number (pitch) of the note. If no key number has ever been received, 60 is used by default.
- `in2(velocity: int)` — Set the velocity value of a note event
  In 3rd inlet: The velocity of the note. If the velocity is 0--indicating a note-off-- the event will be treated as the end of an earlier note-on the same key, and will determine the duration of that earlier note. If no velocity number has ever been received, it is 64 by default.
- `in3(duration: int)` — Set a duration value of a note event
  In 4th inlet: In lieu of a note-off message, a note duration can be supplied as part of the note-on event. If no duration value has ever been received, and no note-off event is received to end the note, a duration of 10 milliseconds is used by default.
- `in4(channel: int)` — Set a MIDI channel value of a note event
  In 5th inlet: The MIDI channel of the note. If no channel has ever been specified, notes are recorded on channel 1.
- `in5(track: int)` — Set the track of an event
  In 6th inlet: The number of a track on which to record the note event. Overdub recording is not possible with detonate, but each recorded note can be tagged with a track number for storing separate tracks of notes internally. If no track number has ever been received, notes are recorded on track 1.
- `in6(extra: int)` — Set an extra value for a note event
  In 7th inlet: An "extra" number, which can be used for any purpose, attached to the note event. This number can be used to provide an additional event parameter, or to serve as a control value in sync with the note. If no number has ever been received in this inlet, it is recorded as 0 by default.
- `in7(extra-2: int)` — Set a second extra value for a note event
  In right inlet: A second "extra" number.
- `mute(event-parameter: int, parameter-value: int, mute-flag: int)` — Mute specific events
  Permits the selective muting of note events that meet specific criteria. The word mute must be followed by an event parameter number, a parameter value, and a value of 1 or 0 signifying "mute" or "unmute".
- `next` — Output an event then move to next
  Once playback of the score has been started with a start message, next sends out the event information (except the delta time) for the current note in the score, then sends out the delta time for the next note. That delta time can in turn be used as a delay time before sending another next message to detonate. When next is received on the last note of the score, there is no note following that one, so a unique value of -1 is sent out the left outlet to signal the end of the score. If a next message is received while the score is not being played back, detonate simply prints the message not playing in the Max Console.
- `nth(event: int)` — Output note event data for a specified number
  The word nth, followed by a number, sends out the note information of the event in the score indicated by the number. (Events are numbered beginning with 0.) In place of the delta time for the event, the (cumulative) starting time of the event is sent out the left outlet.
- `open` — Open the editing window
  See the (mouse) message.
- `params(tolerance: int, advance: int, octave-match: int)` — Set score following behavior
  The word params, followed by three numbers, modifies the score-following behavior of detonate for cases when the received pitch does not match the pitch of the current note in the score. The first number tells detonate how many errors to tolerate before moving ahead in the score. The second number tells how many milliseconds to move ahead in the score when too many errors have occurred. The third number, if non-zero, tells detonate to treat a received pitch that is an octave too high or too low as if it were a match. For example, the message params 3 1000 1 means to allow three successive errors (with octave displacements considered to be a match) before moving ahead one second in the score and resuming. By default, detonate allows 2 errors before moving ahead 200 milliseconds, and does not consider octave pitch displacements to be a match for the stored note.
- `read(filename: list)` — Load a Standard MIDI File
  The word read by itself opens a dialog for loading in a standard MIDI file as contents of the detonate score. If read is followed by the name of a MIDI file in Max's search path, that file is read in directly without opening a dialog box. The read message can also be followed by a number which--if non-zero--causes the time values in the file to be interpreted as milliseconds rather than as bars, beats and ticks at a certain tempo. If the number is 0 or not present, the times are read as bars and beats.
- `record` — Begin recording of score data
  In left inlet: Begins recording numbers coming in the inlets, treating them as parameters of note events to be recorded. The onset of an event is recorded each time a number is received in the left inlet.
- `restore` — Begin recording of score data
  Same as record.
- `setparam(parameter: number, name: symbol, display-mode: int, minimum: int, maximum: int, default: int, interval: int, scaling: int, note-number: int)` — Set all parameters
  The message setparam followed by nine list elements will set the parameters of the object much like entering the information into the object's inspector. The first element in the list is a number and signifies which parameter to edit. The remaining elements are the desired settings listed, from left to right, as the "parameter name", the "display mode", the "minimum value", the "maximum value", the "default value", the "graph interval", the "default scaling", and the "Display MIDI note number" flag.
- `start` — Begin playback of the score
  Begins playing back the score, by simply sending out the first delta time. Once playback of the score has been started, next messages can be used to send out the next event information.
- `startat(pitch: int, velocity: int, channel: int)` — Find and start playback from a specific event
  The word startat, followed by a pitch, a velocity, and a MIDI channel number, causes detonate to look for a note event with those attributes in its stored score. If such a note is found, detonate sends out the delta time of the next event, and a subsequent next message will refer to that next event. If no such note is found, detonate simply prints detonate: note not found in the Max Console.
- `stop` — Stop recording or playback
  Stops detonate from recording, playing, or following. It is not necessary to stop detonate before switching directly between record, start, and follow.
- `unmute(parameter: int, value: int)` — Unmute specific events
  The word unmute, followed by an event parameter number and a parameter value, undoes an earlier mute of the same criterion. For example, unmute 4 10 has the same meaning as mute 4 10 0.
- `unmuteall` — Unmute all muted events
  Undoes the effects of all previous mute messages.
- `wclose` — Close the editing window
  Closes a previously opened editing window.
- `write([filename: symbol], [time: int], [file-format: int])` — Write a Standard MIDI File to disk
  Opens a dialog for saving the contents of detonate as a standard MIDI file. The word write may optionally be followed by up to two numbers. If the first number is non-zero, the file will be saved with time represented in milliseconds rather than as bars, beats, and ticks in a certain tempo. If the number is 0 or not present, the file is saved as beats. The second number indicates the MIDI file format: 0 (all notes on a single track) o multi-track format, using the track parameter to separate the notes). The contents of detonate are also saved as part of the patch, when the patch is saved.
- `writemax([filename: symbol])` — Write a Max text file to disk
  The message writemax followed by a symbol, saves the patch as a Max text file named after the symbol.

## GUI behaviors

- `(mouse)` — Open the editing window
  Double-clicking on the detonate object in a locked patcher opens a graphical editing window for editing the stored detonate data.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — Time Parameter Output
> - `out1` — Pitch Parameter Output
> - `out2` — Vel Parameter Output
> - `out3` — Dur Parameter Output
> - `out4` — Chan Parameter Output
> - `out5` — Track Parameter Output
> - `out6` — X1 Parameter Output
> - `out7` — X2 Parameter Output
> - `in0` — Control Messages, Time Parameter When Recording
> - `in1` — Pitch Parameter When Recording
> - `in2` — Vel Parameter When Recording
> - `in3` — Dur Parameter When Recording
> - `in4` — Chan Parameter When Recording
> - `in5` — Track Parameter When Recording
> - `in6` — X1 Parameter When Recording
> - `in7` — X2 Parameter When Recording

### basic

> time
>
> time
>
> The detonate object--based on "explode" by Miller Puckette--allows you to input sequences of numbers via inlets. (See "EXPLODE: A User Interface for Sequencing and Score Following" by Puckette in the Proceedings, International Computer Music Conference, 1990, Glasgow). The parameter names for input and output are redefinable. By default, from right to left are: extra 2, extra 1, track, channel, duration, velocity, pitch, and time. For MIDI purposes, these parameter names are useful, but the detonate object can be used with values of any nature as a general-purpose object for the storing and sequencing of numbers. The contents of detonate are saved with its patcher.

```
Example — [detonate]  track / extra 2 / extra 1 / track / duration / pitch / velocity / channel / duration / pitch / velocity / channel / extra 1
  (no patch cords)
```

## See also

`follow`, `seq`
