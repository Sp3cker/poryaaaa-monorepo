# live.step

_m4l · Live UI Objects_

> Step Sequencer UI object

live.step displays multiple sequences which have multiple steps composed of pitch, velocity, and duration. Two additional steps (Extra 1 and Extra 2) are available for user-defined display.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | — | Message in |
| out0 | list | Step Values (Index Pitch, Velocity, Duration, Extra1, Extra2) |
| out1 | list | Current Loop Points |
| out2 | list | Mouse Over Information |
| out3 | list | dumpout |
| out4 | int | Sequence Index out |

## Messages

- `bang` — Report values for all sequences
  If the target_seq attribute is set to zero, a bang message will send the values associated with the current step of all sequences out the object's left outlet. If the target_seq attribute is set to a value greater than zero, a bang message will send the values associated with the current step of the currently specified sequence out the object's left outlet.
- `int(step-index: int)` — Set the current step for all sequences
  If the target_seq attribute is set to zero, an integer sets the current step of all sequences to the corresponding integer (index numbering starts at 1). If the target_seq attribute is set to a value greater than zero, an integer sets the current step of the currently specified sequence. The values associated with the current step are sent out the object's left outlet.
- `active(0/1: int)` — Deactivate/activate all sequences
  If the target_seq attribute is set to zero, the message active 0 deactivates all sequences. The word active, followed by any non-zero value, will activate all sequences.
  If the target_seq attribute is set to a value greater than zero, the message active 0 deactivates the specified sequence. The word active, followed by any non-zero value, will activate the specified sequence.
- `copy([start-index: int], [stop-index: int])` — Copy all or a part of a sequence
  The word copy will copy the entire sequence. One or two optional integer arguments may be used to specify starting and ending indices (index numbering starts at 1). For example, copy 2 will copy the sequence starting at index 2, while copy 2 5 will copy starting at index 2 and ending at index 5.
- `dictionary(dictionary-name: symbol)` — Set sequences with a dictionary
  Replaces the current sequence or all sequences of live.step by the content of a named dictionary. If multiple sequences are present in the dictionary, every sequences is replaced.
- `direction(direction-value: int)` — Set the playback direction for sequences
  The word direction followed by an integer in the range 0-4, sets the playback direction for sequence playback. The playback options are:
  0: forward
  1: backward
  2: back and forth
  3: Rotate
  4: random
  If the target_seq attribute is set to zero, the integer argument sets the direction of all sequences when live.step object's playback is controlled using the time message.
  If the target_seq attribute is set to a value greater than zero, an integer sets the direction of the specified sequence when live.step object's playback is controlled using the time message.
- `doedit(edit-mode: int)` — Move or rotate stored sequence data
  The word doedit, followed by a number in the range 0-4, provides a simple means to move or rotate the parameters stored in the live.step object. The modes specified by the number arguments are:
  0: transpose the pitch upward.
  1: transpose the pitch downward.
  2: Rotate the steps to the left.
  3: Rotate the steps to the right
  4: Randomize steps depending on the mode attribute. For instance, if the mode is set to velocity, doedit 4 will only randomize the velocity.
  For more advanced editing features, use the up, down, left, right, random, scramble, and sort messages.
- `down([parameter: symbol])` — Decreases the values of a sequence based on target_seq and mode attributes
  The word down will decreases the values of the sequence based on the target_seq and mode attributes. For instance, if the mode attribute is set to velocity, the down message will only decrease the velocity of the currently specified sequence (or all the sequences if the target_seq attribute is set to zero).
  An optional argument may be used with the down message to specify the parameters to be decreased without having to use the mode attribute. The optional arguments are:
  all: decrease all parameters
  pitch: transpose pitches downward
  velocity: decrease velocities
  duration: decrease durations
  extra1: decrease values for the user-definable Extra 1 parameter
  extra2: decrease values for the user-definable Extra 2 parameter
- `dump` — Report all sequence values
  Sends the values (pitch, velocity, duration, extra1, extra2) of all steps of the currently specified sequence out the live.step object's right outlet.
- `dump_to_dict` — Report sequences in dictionary format
  Sends out a dictionary of the current sequence or all sequences (if target_seq is set to 0), via the dump outlet.
- `duration(start-index/values: list)` — Set duration values for the current sequence
  The word duration, followed by an integer that specifies a starting index into a sequence (index numbering starts with 1) and a list of values (in ticks), will set the duration values for the current sequence specified by the target_seq attribute.
- `extra1(start-index/values: list)` — Set extra1 values for the current sequence
  The word extra1, followed by an integer that specifies a starting index into a sequence (index numbering starts with 1) and a list of values, will set the extra1 values for the current sequence specified by the target_seq attribute.
- `extra2(start-index/values: list)` — Set extra2 values for the current sequence
  The word extra2, followed by an integer that specifies a starting index into a sequence (index numbering starts with 1) and a list of values, will set the extra2 values for the current sequence specified by the target_seq attribute.
- `fetch(parameter name: symbol, step number: int)` — Report parameter values for a step in the sequence
  The word fetch, followed by a symbol that specifies a step parameter (pitch, velocity, duration, extra1, or extra2) and an integer that specifies a step number, will send a list out of the live.step object's fourth (dumpout) outlet in the form .
- `fold(0/1: int)` — Toggle folded display mode
  The word fold, followed by a 0 or 1, toggles the folded display mode. When folding is enabled, the live.step object only displays those pitches which are present in the sequence specified by the target_seq attribute rather than all possible pitches. Fold mode displays only the pitches specific to each individual sequence.
  Note: Sequence editing messages such as up, down, and random use the list of pitches displayed in fold state when performing operations (i.e., the random message will only choose randomly from among the pitches displayed on fold mode. These operations are also dependent on the target sequence, as well). for all sequences.
- `fold_pitch(pitches-list: list)` — Set pitches to be displayed in fold mode
  The word fold_pitch, followed by an integer or list of integers that specify MIDI note numbers, sets the pitches to be displayed by the live.step object in fold mode. As with the fold message, the fold_pitch message sets pitches to be displayed in the sequence specified by the target_seq attribute.
- `getactive` — Report the active state of the currently specified sequence
  The message getactive will send the active state of the currently specified sequence out the live.step object's fourth (dumpout) outlet in the form of a list preceded by the word active.
- `getcurrentstep` — Report the current step of the currently specified sequence
  The message getcurrentstep will send the current step of the sequence out the live.step object's fourth (dumpout) outlet in the form of a number preceded by the word currentstep. When working with multiple sequences, the getcurrentstep message will report the current step of the targeted sequence (specified by the target_seq attribute).
- `getdirection` — Report the direction state of the currently specified sequence
  The message getdirection will send the direction state of the currently specified sequence out the live.step object's fourth (dumpout) outlet in the form of a list preceded by the word direction.
- `getduration` — Report all duration values in the currently specified sequence
  The message getduration will send a list of all the duration values in the sequence specified by the target_seq attribute out the live.step object's fourth (dumpout) outlet in the form of a list preceded by the word duration.
- `getextra1` — Report all extra1 values in the currently specified sequence
  The message getextra1 will send a list of all the extra1 values in the sequence specified by the target_seq attribute out the live.step object's fourth (dumpout) outlet in the form of a list preceded by the word extra1.
- `getextra2` — Report all extra2 values in the currently specified sequence
  The message getextra2 will send a list of all the extra2 values in the sequence specified by the target_seq attribute out the live.step object's fourth (dumpout) outlet in the form of a list preceded by the word extra2.
- `getinterval` — Report the interval of the currently specified sequence
  The message getinterval will send the interval of the currently specified sequence out the live.step object's fourth (dumpout) outlet in the form of a list preceded by the word interval.
- `getloop` — Report the loop points in the currently specified sequence
  The message getloop will send the loop points of the currently specified sequence out the live.step object's fourth (dumpout) outlet in the form of a list preceded by the word loop.
- `getmodelist` — Report all display modes
  The message getmodelist will send the list of the display modes out the live.step object's fourth (dumpout) outlet in the form of a list preceded by the word modelist.
- `getnstep` — Report the number of steps in the currently specified sequence
  The message getnstep will send the number of steps in the currently specified sequence out the live.step object's fourth (dumpout) outlet in the form of a list preceded by the word nstep.
- `getpitch` — Report all pitch values in the currently specified sequence
  The message getpitch will send a list of all the pitch values in the sequence specified by the target_seq attribute out the live.step object's fourth (dumpout) outlet in the form of a list preceded by the word pitch.
- `getstep(step-index: int)` — Report all parameter values for a sequence step
  The message getstep will send a list of the values (pitch, velocity, duration, extra1, extra2) of the specified index of the currently specified sequence out the live.step object's fourth (dumpout) outlet in the form of a list preceded by the word step.
- `getvelocity` — Report all velocity values in the currently specified sequence
  The message getvelocity will send a list of all the velocity values in the sequence specified by the target_seq attribute out the live.step object's fourth (dumpout) outlet in the form of a list preceded by the word velocity.
- `getzoom` — Report the zoom pitch values of the currently specified sequence
  The message getzoom will send the zoom pitch values of the currently specified sequence out the live.step object's fourth (dumpout) outlet in the form of a list preceded by the word zoom.
- `init` — Restore and output the initial values entered when Initial Enabled is checked in the live.step object's Inspector
  Restores and outputs the initial values entered when Initial Enabled is checked in the live.step object's Inspector.
- `interval(time-value: list)` — Set the interval of the currently specified sequence
  Sets the interval of the currently specified sequence.
- `left([parameter: symbol])` — Left-shift the sequence values based on the target_seq and mode attributes
  The word left will rotate (left-shift) the values of the sequence based on the target_seq and mode attributes. For instance, if the mode attribute is set to velocity, the left message will only rotate the velocity of the currently specified sequence (or all the sequences if the target_seq attribute is set to zero).
  An optional argument may be used with the left message to specify the parameters to be rotated without having to use the mode attribute. The optional arguments are:
  all: Rotate all parameters to the left
  pitch: Rotate all pitches to the left
  velocity: Rotate all velocities to the left
  duration: Rotate all durations to the left
  extra1: Rotate all user-definable Extra 1 parameters to the left
  extra2: Rotate all user-definable Extra 2 parameters to the left
- `loop(start-index: int, stop-index: int)` — Set loop points for the currently specified sequence
  The word loop, followed by two numbers that specify starting and ending indices, sets the loop points of the currently specified sequence.
- `next` — Report the values for the next step in the sequences
  The next message will send the values associated with the next step of all sequences out the object's left outlet. If the target_seq attribute is set to a value greater than zero, a next message will send the values associated with the next step of the currently specified sequence out the object's left outlet.
- `nstep(number of steps: int)` — Set the number of steps in the target sequence
  Sets the number of steps in the target sequence.
- `paste([parameter: symbol], [start-index: int])` — Paste steps into a sequence
  When the paste message is received without argument, all parameters are pasted at the same location as they were copied. An optional argument to specify parameter type (pitch, velocity, duration, extra1, extra2) can be used to paste only specific items. An additional option number argument specifies the starting index where the copied steps will be pasted (index numbering starts at 1).
- `pitch(start-index/values: list)` — Set pitch values for the current sequence
  The word pitch, followed by an integer that specifies a starting index into a sequence (index numbering starts with 1) and a list of values, will set the pitch values for the current sequence specified by the target_seq attribute.
- `random([parameter: symbol])` — Randomize values in the seqence
  The word random will randomize the values of the sequence based on the target_seq and mode attributes. For instance, if the mode attribute is set to velocity, the random message will only scramble the velocity of the currently specified sequence (or all the sequences if the target_seq attribute is set to zero).
  An optional argument may be used with the random message to specify the parameters to be scrambled without having to use the mode attribute. The optional arguments are:
  all: Randomize all parameters
  pitch: Randomize all pitches
  velocity: Randomize all velocities
  duration: Randomize all durations
  extra1: Randomize all values for the user-definable Extra 1 parameter
  extra2: Randomize all values for the user-definable Extra 2 parameter
- `reset` — Deactivate the current step
  The reset message allows you to deactivate the current step. It is equivalent to setting the current step to 0.
- `right([parameter: symbol])` — Right-shift the sequence values based on the target_seq and mode attributes
  The word right will rotate (right-shift) the values of the sequence based on the target_seq and mode attributes. For instance, if the mode attribute is set to velocity, the left message will only rotate the velocity of the currently specified sequence (or all the sequences if the target_seq attribute is set to zero).
  An optional argument may be used with the right message to specify the parameters to be rotated without having to use the mode attribute. The optional arguments are:
  all: Rotate all parameters to the right
  pitch: Rotate all pitches to the right
  velocity: Rotate all velocities to the right
  duration: Rotate all durations to the right
  extra1: Rotate all values for the user-definable Extra 1 parameter to the right
  extra2: Rotate all values for the user-definable Extra 2 parameter to the right
- `scramble([parameter: symbol], [keep-step-sync: int])` — Randomize the order of sequence values based on target_seq and mode attributes
  The word scramble will randomize the order of the values of the sequence based on the target_seq and mode attributes. For instance, if the mode attribute is set to velocity, the left message will only scramble the velocity of the currently specified sequence (or all the sequences if the target_seq attribute is set to zero).
  An optional argument may be used with the scramble message to specify the parameters to be scrambled without having to use the mode attribute. The optional arguments are:
  all: Randomize the order of all parameters
  pitch: Randomize the order of all pitches
  velocity: Randomize the order of all velocities
  duration: Randomize the order of all durations
  extra1: Randomize the order of all user-definable Extra 1 parameters
  extra2: Randomize the order of all user-definable Extra 2 parameters
  If you are not using mode all, and that you pass 1 as argument, scramble will keep your steps synchronized. For instance, the scramble pitch 1 message will reorder the pitches, but the other parameters will follow.
- `set(step-index: int)` — Set the current step of all the sequences
  If the target_seq attribute is set to zero, the word set, followed by an integer sets the current step of all the sequences to the corresponding integer (index numbering starts at 1).
  If the target_seq attribute is set to a value greater than zero, the word set, followed by an integer sets the current step of the currently specified sequence.
- `setall(value: list)` — Set a value for all layers in the current sequence
  The word setall, followed by a number, will assign that value to every layer in the currently targeted sequence when the the live.step object is in display mode. An optional first argument specifying a specific layer (i.e. setall velocity 85) can be used to set all values in a specify layer. When the editlooponly attribute is set to 1, the setall message will only be applied to indices inside the loop selection.
- `settimeshift(time-shift: float)` — Move to a sequence step relative to the current step
  The word settimeshift, followed by a positive or negative number that specifies an offset, will move to the relative position from the current step in the sequence and first that step (e.g., sending the message settimeshift -1 when the current step number is 3 will cause the content of step 2 to fire.
- `sort([parameter: symbol], [keep-step-sync: int], [direction: int])` — Sort step values of a sequence based on the target_seq and mode attributes
  The word sort will sort the step values of the sequence based on the target_seq and mode attributes. For instance, if the mode attribute is set to velocity, the sort message will only sort the velocity values of the currently specified sequence (or all the sequences if the target_seq attribute is set to zero).
  Optional arguments may be used with the sort message to specify behavior, synchronization, and direction of sortint. The can be used to set the parameters to be sorted without having to use the mode attribute. The optional arguments are:
  all: Sort the order of all parameters
  pitch: Sort the order of the pitches
  velocity: Sort the order of the velocities
  duration: Sort the order of the durations
  extra1: Sort the order of all user-definable Extra 1 parameters
  extra2: Sort the order of all user-definable Extra 2 parameters
  The first integer argument specifies whether or not to keep the step values synchronized - it only works if the mode attribute is not set to all. For instance, the message sort pitch 1 will reorder the steps based on the pitch, but the velocity, duration and user-definable extra1 and extra2 values associated to the pitch will be maintained. An optional second integer argument can be used to set the sorting order:
  1: Ascending (the default) -1: Descending.
- `step(index: int, midi-note: int, velocity: int, duration: int)` — Set the values for a sequence step
  The word step, followed by a list of four integers, sets the values of a specific step. The required arguments are:
  the index (index numbering starts at 1)
  the MIDI note number (0-127)
  the note velocity (0-127)
  the duration (30, 60, 120, 240, 480, 960, 1920, 3840 ticks)
- `time(bar: int, beat: int, unit: int, resolution: int, time-sig-numerator: list, time-sig-denominator: list)` — Specify the timing position in the sequence.
  The word time, followed by a list of 6 numbers, defines the timing position in the sequence. The required arguments are bar, beat, unit, resolution-in-ppq, time-signature-numerator, and time-signature-denominator. The live.step object does not have an internal clock, so you need to send the time message often if you want to use it to drive the live.step object.
- `up([parameter: symbol])` — Increase values of the sequence based on the target_seq and mode attributes
  The word up will increase the values of the sequence based on the target_seq and mode attributes. For instance, if the mode attribute is set to velocity, the up message will only increase the velocity of the currently specified sequence (or all the sequences if the target_seq attribute is set to zero).
  An optional argument may be used with the up message to specify the parameters to be increased without having to use the mode attribute. The optional arguments are:
  all: Increase all parameters
  pitch: Transpose pitches upward
  velocity: Increase velocities
  duration: Increase durations
  extra1: Increase the value of all user-definable Extra 1 parameters
  extra2: Increase the value of all user-definable Extra 2 parameters
- `velocity(start-index/values: list)` — Set velocity values for the current sequence
  The word velocity, followed by an integer that specifies a starting index into a sequence (index numbering starts with 1) and a list of values, will set the velocity values for the current sequence specified by the target_seq attribute.
- `zoom(low-pitch: float, high-pitch: float)` — Sets the the upper and lower displayed range of the currently specified sequence
- `zoom_fit` — Set the pitch display ranges based on low/high note values
  The word zoom_fit will cause the currently displayed pitch range of the live.step object to adjust so that the highest and lowest note values become the upper and lower limits of the display.
- `zoom_in` — Decrease the displayed pitch range by 7 steps
  The word zoom_in will cause the current pitch range of the live.step object display to decrease by a factor of a fifth (7 steps) at the top and bottom, resulting in a "zoom in" effect.
- `zoom_out` — Increase the displayed pitch range by 7 steps
  The word zoom_out will cause the current pitch range of the live.step object display to increase by a factor of a fifth (7 steps) at the top and bottom, resulting in a "zoom out" effect.

## GUI behaviors

- `(mouse)` — Report mouse activity
  Whenever the live.step object is edited using the mouse, the message changed followed by a number in the range 0-2 will be sent out the object's dumpout outlet. The number specifies what portion of the live.step display has been modified, as follows:
  0: The pitch, velocity, duration, extra1, or extra2 settings have been modified.
  1: The looping portion of the UI has been modified.
  2: Edits have been made in the ruler area of the UI.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)
- `@save` (int)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `display_seq` — seen as: `display_seq $1`, `display_seq $1, target_seq $1`
- `editlooponly` — seen as: `editlooponly $1`
- `extra_thickness` — seen as: `extra_thickness $1`
- `loopruler` — seen as: `loopruler $1`
- `mode` — seen as: `mode $1`
- `nseq` — seen as: `nseq $1`
- `target_seq` — seen as: `target_seq $1`, `target_seq 0`, `target_seq 0, direction $1`
- `unitruler` — seen as: `unitruler $1`

## Help patcher examples

### Filling and dumping data

```
Example — [live.step]
  fan-in:
    in0 ← [message "getextra2"]    # "get" messages send a list of all pitch/velocity/duration.... values out the dumpout outlet
    in0 ← [message "getpitch"]
    in0 ← [message "getvelocity"]
    in0 ← [message "getduration"]
    in0 ← [message "getextra1"]
    in0 ← [prepend velocity 1] ← [multislider]
    in0 ← [message "dump"]
    in0 ← [prepend pitch 13] ← [multislider]    # start index
    in0 ← [prepend pitch 1] ← [multislider]
  fan-out:
    out3 → [print]:in0    # start index
```

### Appearance

```
Example — [live.step]
  fan-in:
    in0 ← [attrui @hbgcolor]
    in0 ← [message "unitruler $1"]
    in0 ← [message "loopruler $1"]    # Show/hide the loop ruler
    in0 ← [message "zoom_fit"]    # Zoom to fit the current sequence
    in0 ← [attrui @bordercolor]
    in0 ← [attrui @stepcolor]
    in0 ← [attrui @textcolor]
    in0 ← [attrui @stepcolor2]
    in0 ← [attrui @bgrulercolor]
    in0 ← [message "zoom_in"]
    in0 ← [message "zoom_out"]    # Zoom in/out by plus/minus a perfect fifth
    in0 ← [attrui @whitekeycolor]
    in0 ← [message "extra_thickness $1"]
    in0 ← [attrui @bgcolor]
    in0 ← [attrui @blackkeycolor]
    in0 ← [attrui @loopbordercolor]
    in0 ← [message "mode $1"]
    in0 ← [attrui @bgunitcolor]
    in0 ← [attrui @bgcolor2]
    in0 ← [message "zoom $1 $2"]    # Zoom the display
```

Attributes demonstrated: `@bgcolor`, `@bgcolor2`, `@bgrulercolor`, `@bgunitcolor`, `@blackkeycolor`, `@bordercolor`, `@hbgcolor`, `@loopbordercolor`, `@stepcolor`, `@stepcolor2`, `@textcolor`, `@whitekeycolor`

### Modifying sequences

```
Example — [live.step]
  fan-in:
    in0 ← [message "right"]
    in0 ← [message "right extra1"]
    in0 ← [message "left pitch"]
    in0 ← [message "left"]
    in0 ← [message "editlooponly $1"]
    in0 ← [message "random extra1"]
    in0 ← [message "up"]
    in0 ← [message "random"]
    in0 ← [message "setall 74"]
    in0 ← [message "setall duration 240."]
    in0 ← [message "sort 1"]
    in0 ← [message "sort pitch 1 -1"]
    in0 ← [message "scramble pitch 1"]
    in0 ← [message "scramble"]
    in0 ← [message "up velocity"]
    in0 ← [message "down velocity"]
    in0 ← [message "down"]
```

### Editing sequences

```
Example — [live.step]  sequence index starts at 1
  fan-in:
    in0 ← [message "display_seq $1, target_seq $1"]    # Select the sequence to display and edit
    in0 ← [message "target_seq 0, next"]    # trigger the next step on all sequences
    in0 ← [message "set 0"]
    in0 ← [message "target_seq 2, active $1"]
    in0 ← [message "target_seq 0, direction $1"]
    in0 ← [message "set $1"]
    in0 ← [number]    # Select step and cause output
    in0 ← [message "target_seq 0"]    # Edit all sequences together
    in0 ← [message "mode $1"]
    in0 ← [message "target_seq 1, active $1"]
  fan-out:
    out0 → [gate 2]:in1
    out4 → [gate 2]:in0
```

### Counter-driven sequencing

> <-- "Always add a little bit of reverb, everyone knows that it sounds better", ddg April 09

```
Example — [live.step]  loop output information sets the min/max for the counter
  fan-in:
    in0 ← [message "fold $1"]
    in0 ← [p counter_min_max]    # p counter_min_max emits: "min $1" | "max $1"
    in0 ← [prepend mode] ← [live.menu]    # Display mode
  fan-out:
    out0 → [p monosynth]:in0
    out1 → [p counter_min_max]:in2
```

### Fold mode

> The fold mode lets you chose to display all possible pitches, or only a specific set of pitches (i.e. a scale or mode).

> Use the fold_pitch message to set which pitches you want to display in fold mode. Pitches not in the fold list will automatically be folded down to the nearest neighbor

> 1. Draw some nice notes 2. Activate Fold mode so only the specified pitches will appear

```
Example — [live.step]
  fan-in:
    in0 ← [message "fold_pitch 60 63 67 70 74"]
    in0 ← [message "fold $1"]
    in0 ← [message "fold_pitch 61 63 68 70 73"]
```

### basic

```
Example — [live.step]
  fan-in:
    in0 ← [message "target_seq $1"]    # Select a sequence to edit/modify
    in0 ← [p Transportation]    # p Transportation emits: "reset" | "0." | "tempo $1"
    in0 ← [message "active $1"]    # Activate/deactivate the currently selected sequence
    in0 ← [message "nstep $1"]    # Set the number of steps in the sequence (1-64)
    in0 ← [message "loop $1 $2"]    # Loop the currently selected sequence
    in0 ← [message "display_seq $1"]    # Select sequence to display
    in0 ← [message "nseq $1"]    # Set the number of sequences (1-16)
  fan-out:
    out0 → [p PlayMIDINote]:in0
    out0 → [message "10 79 80 120. 100 100"]:in1    # Current step values (list) : step pitch vel. dur. (ticks) prob. extra1 extra2
    out1 → [message "1 12"]:in1    # Outputs the current loop points in the form of a list (list) : min max
    out2 → [message "15 70 97 120. 100 100"]:in1
    out3 → [gate]:in1
    out4 → [number]:in0    # Sequence index out (int)
```

## See also

`live.grid`, `multislider`, `matrixctrl`
