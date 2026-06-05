# mtr

_max · Sequencing_

> Record and sequence messages

Records messages and provides sequenced playback.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Control Input for All Tracks |
| in1 | Track 1 Record/Control Input |
| out0 | next Outputs list with Track and Duration |
| out1 | Track 1 Play Output |

## Arguments

- **tracks** (`int`) _(optional)_ — Number of tracks
  Specifies the number of tracks in mtr. The number of tracks determines the number of inlets and outlets in addition to the leftmost inlet and outlet. Up to 128 tracks are possible. If there is no argument, there will only be one track.

## Messages

- `bang` — Send dictionary data out left outlet
  Just like the info message, sending a bang to the left inlet of mtr, results in a dictionary being sent out the left outlet with information about the current state of the object. Both global and track specific informtion is included in the dictionary. An entry that begins with "global_" is the global setting for the specified attribute. If the same attribute has a track specific setting, that value will be listed under the track information.
- `int(input: int)` — Store the message on a track
  In any inlet other than the left inlet: If the track is currently being recorded, numbers received in that track's inlet are combined with a delta time (the amount of time elapsed since the previous event) and stored in mtr.
- `float(input: float)` — Store the message on a track
  In any inlet other than the left inlet: If the track is currently being recorded, numbers received in that track's inlet are combined with a delta time (the amount of time elapsed since the previous event) and stored in mtr.
- `list(input: list)` — Store the message on a track
  In any inlet other than the left inlet: If the track is currently being recorded, lists received in that track's inlet are stored in mtr, preceded by the delta time (the amount of time elapsed since the previous event). The maximum allowed length for a list is 4096 items.
- `anything(input: list)` — Store the message on a track
  In any inlet other than the left inlet: If the track is currently being recorded, symbols received in that track's inlet are stored in mtr, preceded by the delta time (the amount of time elapsed since the previous event).
  Although mtr can record individual bytes of MIDI messages received from midiin, it stores each byte with a separate delta time, and does not format the MIDI messages the way seq does. If you want to record complete MIDI messages and edit them later, seq is better suited for the task. On the other hand, mtr is perfectly suited for recording sequences of numbers, lists, or symbols from virtually any object in Max: specialized MIDI objects such as notein or pgmin, user interface objects such as number box, slider, and dial, or any other object.
  In order for a file to be read into mtr for playback, it must be in the proper format. An mtr multi-track sequence can even be typed in a text file, provided it adheres to the format. The contents of the different tracks are listed in order in an mtr file, and the format of each track is as follows. Note that a semicolon (;) ends each line.
  Line 1: track ; (Track in which to store subsequent data)
  Line 2, etc.: ;
  Last line: end; (End of this track's data)
- `addevent(absolute time-data: list)` — Add an event at a specific time
  The word addevent followed by a list, absolute time and data, adds an event to a track at a specific time. For example, the message "addevent 1000 1" will add the event 1 at time 1000. This message can be sent while a track is recording, playing, or stopped. The message will only work in track-specific inlets, and cannot be sent to the left inlet.
- `clear(tracks: list)` — Clear the contents of tracks
  In left inlet: Erases the contents of mtr. The word clear, followed by one or more track numbers, clears those tracks.
  In other inlets: Erases the track that corresponds to the inlet.
- `cleareventat(absolute time-matching criteria: list)` — Clear an event at a specific time
  The word cleareventat followed by a list, absolute time and matching criteria, clears all events that exactly match the criteria at the specific time. Unlike deleteeventat, cleareventat does not delete the time occupied by the event. For example, if events are at time 1000, 2000, and 3000, and the event at 2000 is cleared, the next event remains at time 3000. This message can be sent while a track is recording, playing, or stopped. The message will only work in track-specific inlets, and cannot be sent to the left inlet.
- `definelengthandstop(tracks: list)` — Stop recording and set track length
  In left inlet: Stops recording and sets the track’s length to the current recording time. The stop message, by contrast, does not set the track’s length; the total recorded length (available in the info dictionary) is defined by both messages and consists of the time between the start of the recording and the time of the final event recorded before stop or definelengthandstop is received. The word definelengthandstop, followed by one or more tracks, stops and sets the length for those tracks.
  In other inlets: Stops and sets the length of the track that corresponds to the inlet.
- `delay(delay-time: int)` — Set an initial delta time value
  In left inlet: The word delay, followed by a number, sets the first delta time value of each track to that number, so that all tracks begin playing back that amount of time after the play message is received.
  In other inlets: Sets the initial delta time of the track that corresponds to the inlet.
- `deleteeventat(absolute time-matching criteria: list)` — Delete an event at a specific time
  The word deleteeventat followed by a list, absolute time and matching criteria, deletes all events that exactly match the criteria at the specific time. The time occupied by the event is also deleted. "deleteeventat 1000" will delete all events at absolute time 1000 in the track. The message "deleteeventat 1000 3" will delete an event at absolute time 1000 containing 3. The event "3 4 5" will not be deleted because it does not match exactly. This message can be sent while a track is recording, playing, or stopped. The message will only work in track-specific inlets, and cannot be sent to the left inlet.
- `dictionary(dictionary name: symbol)` — Send a dictionary to mtr
  The message dictionary, followed by a name, will load that specific dictionary into mtr, including all events from each track sequence.
- `dump` — Send dictionary data out left outlet
  The message dump, when sent to any inlet of mtr, sends a dictionary out the left outlet of mtr. Unlike the dictionary sent out from the info or bang messages, this dictionary contains individual events from each track sequence.
- `first(wait-time: int)` — Set playback wait time
  In left inlet: The word first, followed by a number, causes mtr to wait that amount of time after a play message is received before playing back. Unlike delay, first does not alter the delta time value of the first event in a track, it just waits a certain time (in addition to the first delta time) before playing back from the beginning.
- `info` — Send dictionary data out left outlet
  Just like bang, sending the info message to the left inlet of mtr, results in a dictionary being sent out the left outlet with information about the current state of the object. Both global and track specific informtion is included in the dictionary. An entry that begins with "global_" is the global setting for the specified attribute. If the same attribute has a track specific setting, that value will be listed under the track information.
- `mute(tracks: list)` — Mute output
  In left inlet: Causes mtr to stop producing output, while still continuing to "play" (still moving forward in the sequence). The word mute, followed by one or more tracks, mutes those tracks.
  In other inlets: Mutes the track that corresponds to the inlet.
- `next(tracks: list)` — Output the next message
  In left inlet: Causes each track to output only the next message in its recorded sequence. When a next message is received, the track number, delta time, and absolute time of each message being output are sent out the leftmost outlet as a list. The word next, followed by one or more track numbers, outputs the next message stored in those tracks.
  In other inlets: Outputs the next message stored on the track that corresponds to the inlet.
- `play(tracks: list)` — Play back messages at their normal rate
  In left inlet: Plays back all messages recorded earlier, sending them out the corresponding outlets in the same rhythm and at the same speed they were recorded. The word play, followed by one or more track numbers, begins playing those tracks.
  In other inlets: Plays back all messages on the track that corresponds to the inlet. When the play message is sent to specific track inlets you can give it two optional arguments: number of iterations and timescale. 100 is the original timescale. The message "play 3 200" will play the track three times, at twice the speed.
- `playat(position: float)` — Start playback at a specific point (0-1)
  In left inlet: Starts playback of all messages at a specific point, which is specified in a normalized range from 0-1, where 0 is the beginning and 1 is the end. Messages are sent out the corresponding outlets in the same rhythm and at the same speed they were recorded.
  In other inlets: Starts playback of all messages on the track that corresponds to the inlet. Playback stars at a specific point, which is specified in a normalized range from 0-1.
- `playatms(milliseconds: float)` — Start playback at a specific point - milliseconds
  In left inlet: Starts playback of all messages at a specific point, which is specified in milliseconds. Messages are sent out the corresponding outlets in the same rhythm and at the same speed they were recorded.
  In other inlets: Starts playback of all messages on the track that corresponds to the inlet. Playback stars at a specific point, which is specified in milliseconds.
- `read(filename: symbol)` — Load a saved mtr file
  In left inlet: Calls up the standard Open Document dialog box, so that a previously saved file can be read into mtr. Only .txt, .pat, and .json files can be loaded.
  In other inlets: Opens a file containing only the track that corresponds to the inlet.
- `record(tracks: list)` — Start recording
  In left inlet: Begins recording all messages received in the other inlets. The word record, followed by one or more track numbers, begins recording those tracks.
  In other inlets: Begins recording messages on the track that corresponds to the inlet.
- `rewind(tracks: list)` — Return to the start of the sequence/track
  In left inlet: Resets mtr to the beginning of its recorded sequence. This command is used to return to the beginning of the sequence when stepping through messages with next. To return to the beginning of a sequence while playing or recording, just repeat the play or record message. When mtr is playing or recording, a stop message should precede a rewind message. The word rewind, followed by one or more track numbers, returns to the beginning of those tracks.
  In other inlets: Returns the pointer to the beginning of the track that corresponds to the inlet.
- `stop(tracks: list)` — Stop recording or playback
  In left inlet: Stops mtr when it is recording or playing. The word stop, followed by one or more track numbers, stops those tracks.
  In other inlets: Stops the track that corresponds to the inlet.
- `timescale(timescale: float)` — Sets the timescale of a track
  In left inlet: Sets the timescale for all tracks. 100 is the original timescale, whereas 200 would be twice as fast. This message can be set while a track is playing. Please note that when a track is played again, the timescale is reset to 100. For this reason, use of this message is strongly discouraged in favor of the trackspeed attribute, which does not reset.
  In other inlets: Sets the timescale for the track that corresponds to the inlet.
- `touch(off/on: int)` — Touch automation recording for a specific inlet
  Turn touch automation recording off/on for the track that corresponds to the inlet. Sending the message "touch 1" to a specific inlet while a track is playing, overwrites the existing recording with new data received in the track’s inlet. Timing is unaffected. Sending the message "touch 0" to a specific inlet ends the touch automation recording, and any events from that point on in the track play as usual. The touch message only works when a track is already playing.
  This message is for individual track inlets. To enable/disable touch automation recording on multiple tracks at once, use the touchenable and touchdisable messages, which are sent to the left inlet of mtr.
- `touchdisable(tracks: list)` — Disable touch automation recording
  In left inlet: Turns touch automation recording off for the specified tracks. Sending the message "touchdisable 2 3" ends touch automation recording on tracks 2 and 3. Any events from that point on in the tracks play as usual. This message is used in conjunction with the touchenable message. Timing is unaffected. The touchdisable message only works when a track is already playing.
- `touchenable(tracks: list)` — Enable touch automation recording
  In left inlet: Turns touch automation recording on for the specified tracks. Sending the message "touchenable 2 3" while a track is playing, overwrites the existing recording for tracks 2 and 3 with new data received in the tracks' inlets. Use the message touchdisable to stop touch automation recording. Timing is unaffected. The touchenable message only works when a track is already playing.
- `unmute(tracks: list)` — Unmute output
  In left inlet: Undoes any previously received mute messages. The word unmute, followed by one or more track numbers, unmutes those tracks.
  In other inlets: Unmutes the track that corresponds to the inlet.
- `write(filename: symbol)` — Write the sequence to disk
  In left inlet: Calls up the standard Save As dialog box, allowing the contents of mtr to be saved as a separate file. If the specified filename (given as an argument) ends in .json, a JSON format file is saved.
  In other inlets: Writes a file containing only the track that corresponds to the inlet.
- `writejson([filename: symbol])` — Write the sequence to disk in JSON format
  In left inlet: Calls up the standard Save As dialog box, allowing the contents of mtr to be saved as a separate file, in JSON format. Unlike the write message, writejson writes times as floats, preserving the timing of events precisely.
  In other inlets: Writes a file, in JSON format, containing only the track that corresponds to the inlet.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `abc` — seen as: `abc`
- `length` — seen as: `length $1`, `length 0, record`
- `speed` — seen as: `speed $1`
- `trackspeed` — seen as: `trackspeed $1`

## Help patcher examples

### playback

> current playback position

```
Example — [mtr 4 @embed 1]  this mtr object contains a pre-recorded sequence (stored with @embed 1)
  fan-in:
    in0 ← [message "stop"]    # control playback
    in0 ← [message "delay 1000."]
    in0 ← [message "play"]
    in0 ← [message "first 0."]
    in0 ← [message "delay 0."]    # set a delay time at the beginning of the sequence
    in0 ← [attrui @loop]    # set looping (propogated to each of the tracks in the object)
    in0 ← [attrui @mode]
    in0 ← [r mtr-p]
    in0 ← [message "playat 0.5"]
    in0 ← [message "playatms 7500"]    # enable mode attribute to output the previous value on restart, when the selection startpoint is non-zero / start playback at a specific point
    in0 ← [message "first 1000."]    # add a wait period before the sequence starts playback
  fan-out:
    out1 → [multislider]:in0
    out2 → [multislider]:in0
    out3 → [multislider]:in0
    out4 → [multislider]:in0
```

Attributes demonstrated: `@loop`, `@mode`

### storage

```
Example — [mtr 3]
  fan-in:
    in0 ← [message "writejson"]    # write the contents of mtr to a JSON file
    in0 ← [message "play"]
    in0 ← [message "read"]    # read a file in either text or JSON format
    in0 ← [message "record"]
    in0 ← [message "stop"]
    in0 ← [attrui @embed]    # set the embed attribute to 1 to store the mtr contents within the patcher file
    in0 ← [message "write"]    # write the contents of mtr to a text file
    in1 ← [slider] ← [mtr 3]
    in2 ← [slider] ← [mtr 3]
    in3 ← [slider] ← [mtr 3]
  fan-out:
    out1 → [slider]:in0
    out2 → [slider]:in0
    out3 → [slider]:in0
```

Attributes demonstrated: `@embed`

### stepping

```
Example #1 — [mtr 1 @embed 1 @nextmode 1]
  fan-in:
    in0 ← [message "next"]    # output the current event and advance to the next event
    in0 ← [message "rewind"]    # return to the beginning of the sequence
  fan-out:
    out0 → [message ""]:in1    # track delta-time absolute-time
    out1 → [flonum]:in0    # When nextmode = 1 (loop mode), a next message after the last event will immediately loop back to the first event.
```

```
Example #2 — [mtr 1 @embed 1 @nextmode 0]
  fan-in:
    in0 ← [message "next"]    # output the current event and advance to the next event
    in0 ← [message "rewind"]    # return to the beginning of the sequence
  fan-out:
    out0 → [message ""]:in1    # track delta-time absolute-time
    out1 → [flonum]:in0    # When nextmode = 0, a next message after the last event will produce an event with a delta time of -1.
```

### binding

```
Example — [mtr 3 @embed 1 @bindto 1 s1 2 s2 3 s3]  set bindings as typed-in arguments using the @bindto attribute as track number - scripting name pairs
  fan-in:
    in0 ← [message "play"]
    in0 ← [message "stop"]
    in0 ← [message "record"]    # Play the embedded sequence
```

### editing

```
Example — [mtr 2 @length 3000. @loop 1]
  fan-in:
    in0 ← [message "clear"]
    in0 ← [message "play"]    # play sequence
    in0 ← [message "stop"]
    in1 ← [message "addevent 0. 0., addevent 2000. 1."]
    in1 ← [message "addevent 1000. 0.5"]
    in1 ← [message "cleareventat 1000."]
    in1 ← [message "deleteeventat 1000."]
    in2 ← [message "clear"]
    in2 ← [message "addevent $1 $2"]
  fan-out:
    out1 → [dial]:in0
    out2 → [dial]:in0
```

### time

```
Example — [mtr 3 @loop 1 @embed 1]
  fan-in:
    in0 ← [message "speed $1"]
    in0 ← [message "trackspeed $1"]
    in0 ← [message "stop"]
    in0 ← [message "play"]
    in0 ← [message "length $1"]
    in1 ← [message "trackspeed $1"]
    in2 ← [message "length $1"]
    in3 ← [message "timescale $1"]    # the timescale message also changes the playback speed of a sequence or track, but uses % values.
  fan-out:
    out1 → [p drawred]:in0
    out2 → [p drawgreen]:in0
    out3 → [p drawblue]:in0
```

### dictionaries

> The info dictionary contains information about the sequence and all of its track.

```
Example #1 — [mtr 1]
  fan-in:
    in0 ← [dict @embed 1] ← [button]    # ramp
    in0 ← [dict @embed 1] ← [button]    # in a locked patcher, double-click these dict objects to see their contents / send a 'dump' formatted dictionary into mtr to replace its sequence / sine
    in0 ← [message "play"]
  fan-out:
    out1 → [multislider]:in0
```

```
Example #2 — [mtr 2 @embed 1]
  fan-in:
    in0 ← [button]    # bang to output an 'info' dictionary
    in0 ← [message "info"]    # ...or use the 'info' message
    in0 ← [message "dump"]    # output a 'dump' dictionary
  fan-out:
    out0 → [dict.route type:info]:in0
```

### sync

> Changing the transport tempo will change the playback speed of a sync'd mtr object
>
> NOTE:
>
> When an mtr object is recording in sync mode, the event timing is stored in ticks rather than milliseconds. Turning sync mode on and off when recoding will produce timing irregularities.

```
Example — [mtr 1 @embed 1 @sync 1 @autostart 1 @autostarttime 0. @loop 1]
  fan-in:
    in0 ← [attrui @transport]    # set name of transport to use (defaults to internal)
    in0 ← [message "play"]
    in0 ← [attrui @quantize]
    in0 ← [message "stop"]    # control playback
    in0 ← [attrui @loop]    # set looping
  fan-out:
    out1 → [multislider]:in0
```

Attributes demonstrated: `@loop`, `@quantize`, `@tempo`, `@transport`

### recording

> send touch 1 while playing to overwrite the recording with new data, send touch 0 to end overwriting -- when touch is on, existing events at the point where they would be replayed are erased and new data is recorded

```
Example — [mtr 3 @loop 1]
  fan-in:
    in0 ← [message "stop"]
    in0 ← [message "length 0, record"]
    in0 ← [message "play"]
    in0 ← [message "definelengthandstop"]
    in0 ← [attrui @loop]    # set looping for all tracks
    in0 ← [message "touch $1"]
    in1 ← [slider] ← [mtr 3 @loop 1]
    in2 ← [slider] ← [mtr 3 @loop 1]
    in3 ← [slider] ← [mtr 3 @loop 1]
  fan-out:
    out1 → [slider]:in0
    out2 → [slider]:in0
    out3 → [slider]:in0
```

Attributes demonstrated: `@loop`

### basic

> record messages of any type to track inputs

> record messages of any type

```
Example — [mtr 3]
  fan-in:
    in0 ← [message "stop"]
    in0 ← [message "play"]
    in0 ← [message "clear"]
    in0 ← [message "record"]    # control all tracks
    in1 ← [number]    # integer
    in1 ← [flonum]    # float
    in1 ← [button]    # bang
    in2 ← [message "abc"]    # symbol
    in2 ← [message "1 2 3"]    # list
    in3 ← [slider] ← [mtr 3]
    in3 ← [message "clear"]
    in3 ← [message "play"]
    in3 ← [message "stop"]
    in3 ← [message "record"]    # control individual tracks
    in3 ← [message "mute"]
    in3 ← [message "unmute"]
  fan-out:
    out1 → [message ""]:in1
    out2 → [message ""]:in1    # messages play back from individual track outlets / messages play back from track outlets / messages play back from track outlets
    out3 → [slider]:in0
    out3 → [message ""]:in1
```

## See also

`multislider`, `seq`, `rslider`, `slider`
