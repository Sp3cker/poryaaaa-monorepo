# follow

_max · Sequencing_

> Compare a live performance to a recorded performance

follow records pitches, or you can give it a MIDI file, in which case it looks at the file's note-ons and ignores other events. When it is "following" it outputs the index of the last note matched.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | MIDI Pitch Input, Many Other Messages |
| out0 | Number of Note In Score |
| out1 | MIDI Pitch Output |

## Arguments

- **filename** (`symbol`) _(optional)_ — TEXT_HERE
  The argument is the name of a file containing a previously recorded sequence, to be read into follow automatically when the patch is loaded.

## Messages

- `bang` — Starts playing back the sequence stored in follow.
- `int(input: int)` — When follow is recording, the numbers received in its inlet are recorded as a sequence.
  When follow is recording, the numbers received in its inlet are recorded as a sequence. The numbers may be bytes of MIDI messages (from midiformat or midiin), exactly as with the seq object. However, follow differs from seq in its ability to record individual integers; with follow you can record notes as a single pitch value. Whether the performance is recorded as complete MIDI messages or just as note-on pitches, follow can effectively step through the note-on pitch numbers later, when following a performance.
- `float(input: float)` — Converted to int.
- `append` — Starts recording at the end of the stored sequence, without erasing the existing sequence.
- `delay(onset-time (milliseconds): int)` — The word delay, followed by a number, sets the onset time, in milliseconds, of the first event in the recorded sequence.
- `dump` — Calls up the standard Open Document dialog box, so that a previously recorded sequence or standard MIDI file can be opened as text and displayed in a new Untitled text window.
  Calls up the standard Open Document dialog box, so that a previously recorded sequence or standard MIDI file can be opened as text and displayed in a new Untitled text window. This in fact has no direct effect on the follow object, but does allow you to view or edit a sequence, save your changes in a file, then load the new file into follow with a read message.
- `follow(index: int)` — The follow message is the main feature that distinguishes follow from seq. In effect, follow is like a score reader, comparing a live performance with the one previously stored.

 The word follow, and a number, causes follow to begin comparing incoming numbers to its own stored numbers, beginning at the specified index (the specified event in its own stored sequence).
  The follow message is the main feature that distinguishes follow from seq. In effect, follow is like a score reader, comparing a live performance with the one previously stored.
  The word follow, and a number, causes follow to begin comparing incoming numbers to its own stored numbers, beginning at the specified index (the specified event in its own stored sequence). When follow is following, and a number is received that matches the number recorded in follow, it sends out the index of that number.
  The follow object is a forgiving score reader, and will try to follow along even if the incoming numbers do not exactly match the recorded sequence. If a number arrives that does not match the next number, or either of the two subsequent numbers in the sequence, follow does nothing. If a number arrives that matches a number up to two notes ahead in the sequence, follow assumes that the performer simply missed a note or two, and jumps ahead to the matched number.
- `in1(input: int)` — When follow is following, numbers received in its inlet are compared to the numbers recorded in the sequence. When a number is received that matches the number in the sequence, follow sends out the index of that number.
- `next` — Causes follow to send out the index and the stored number it is currently trying to match, and move on to the next number.
- `print` — Prints the first few events of the recorded sequence in the Max Console.
- `read(filename: list)` — The word read with no arguments puts up a standard Open Document dialog box for choosing a sequence file to load into follow.
  The word read with no arguments puts up a standard Open Document dialog box for choosing a sequence file to load into follow. If read is followed by a symbol filename argument, the named file is located and loaded into follow.
- `record` — Starts recording integers received in the inlet.
- `start(tempo: int)` — The word start by itself has the same effect as bang.
  The word start by itself has the same effect as bang. The word start, followed by a number, plays the stored sequence at a tempo determined by the number. The message start 1024 indicates normal tempo. If the number is 512, follow plays the sequence at half the original recorded speed, start 2048 plays it back at twice the original speed, and so on.
- `stop` — Stops follow from recording, playing, or following.
  Stops follow from recording, playing, or following. A stop message need not be received before switching directly from recording to playing, following to recording, etc.
- `write(filename: list)` — Opens a standard Save As dialog box to save the follow sequence as a file.

## Help patcher examples

### basic

```
Example — [follow follow_sc.midi]  follow.sc.midi contains a C major scale starting on C60
  fan-in:
    in0 ← [button]    # bang starts playback of the score
    in0 ← [message "stop"]    # stop everything
    in0 ← [message "record"]    # start recording the score to follow
    in0 ← [message "delay 0"]    # set onset time of first note
    in0 ← [message "follow 0"]    # takes MIDI pitches as integers (not MIDI raw byte) / start following on nth note / step forward one note
    in0 ← [message "next"]
    in0 ← [message "print"]    # print first few notes
    in0 ← [message "read"]    # read from a text or MIDI file
    in0 ← [message "write"]    # save as a MIDI file
    in0 ← [stripnote]
  fan-out:
    out0 → [number]:in0    # number of current note in score
    out1 → [number]:in0
    out1 → [makenote 60 200]:in0    # use this to send notes back out MIDI port on playback
```

## See also

`seq`, `detonate`
