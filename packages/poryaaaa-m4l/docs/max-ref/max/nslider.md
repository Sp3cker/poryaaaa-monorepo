# nslider

_max · U/I_

> Output numbers from a notation display

nslider or "Note Slider" is a musical-notation-based integer value slider.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Note Value In |
| in1 | Velocity Value In |
| out0 | Note Value Out |
| out1 | Velocity Value Out |

## Messages

- `bang` — Send out the stored values
  In left inlet: Sends out the pitch and velocity values currently stored in nslider.
- `int(input: int)` — Display and output a note value
  The number received in the inlet is displayed graphically by nslider if it falls within its displayed range. The current velocity value (from 1 to 127) that nslider holds is sent out its right outlet, followed by the received number out the left outlet.
- `float(input: float)` — Display and output a note value
  In left inlet: Converted to int.
- `list(value-pair: list)` — Set pitch and velocity, cause output
  A list of two numbers sent to the left inlet of nslider can be used to set and output the note and velocity values.
- `anything(notenames: list)` — Specify notes by note name
  Notes can also be added using the musical note name and octave, i.e., F#3. This is especially useful for forcing display of accidentals (# or b). For example, a value of 59 and the message Cb4 both produce the same note, but the number will display a B note while the message displays a C-flat.
- `chord(value-pairs: list)` — Set and output multiple notes
  In left inlet: The word chord, followed by a list of MIDI note name and velocity pairs, can be used to play chords on the nslider in polyphonic mode (set by the mode 1 message). The chord message sends note-offs for currently held notes, followed by note-on commands for the specified note and velocity pairs. When the nslider object's state is saved by a preset object in polyphonic mode, the preset object will store chord messages.
- `clear` — Clear all stored notes
  In left inlet: The clear message will clear any notes on the staves, but will not trigger any output.
- `flush` — Send note-off messages for all held notes
  In left inlet: When the nslider object is in polyphonic mode (set by the mode 1 message), the flush message will send note-offs to currently held notes and clear the nslider object's display.
- `ft1(velocity: float)` — Set the output velocity
  In right inlet: Converted to int.
- `in1(velocity: int)` — Set the output velocity
  In right inlet: The number received in the right inlet sets the output key velocity without triggering output.
- `set(value-pair: list)` — Set pitch and velocity with no output
  In left inlet: The word set, followed by a number, changes the value displayed by nslider, without triggering output. If the set message is followed by two numbers, both the note and velocity values are set, without causing output.

## GUI behaviors

- `(mouse)` — Output note values
  nslider also sends out numbers when you click or drag on it with the mouse. The velocity value is determined by the previous value received in the right inlet.
  If the nslider object is in polyphonic mode, you need to click on a note twice: once to send a note-on and draws the note, and once again to send a note-off and erase the note.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)
- `@save` (int)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `Cb4` — seen as: `Cb4`
- `E#4` — seen as: `E#4`
- `F4` — seen as: `F4`

## Help patcher examples

### notenames

```
Example #1 — [nslider]
  fan-in:
    in0 ← [message "E#4"]
    in0 ← [message "F4"]
    in0 ← [message "Cb4"]
  fan-out:
    out0 → [number]:in0
```

```
Example #2 — [nslider]
  fan-in:
    in0 ← [message "flush"]
    in0 ← [message "chord Cb4 64 Fb4 64 Ab4 64"]
    in0 ← [message "chord C#3 64 G#3 64 E#4 64 C#5 64"]
  fan-out:
    out0 → [pack]:in0
    out1 → [pack]:in1
```

### appearance

```
Example #1 — [nslider]
  fan-in:
    in0 ← [p stream-o-notes] ← [active]    # p stream-o-notes emits: "-1"
```

```
Example #2 — [nslider]
  fan-in:
    in0 ← [p stream-o-notes] ← [active]    # p stream-o-notes emits: "-1"
```

```
Example #3 — [nslider]
  fan-in:
    in0 ← [p stream-o-notes] ← [active]    # p stream-o-notes emits: "-1"
```

```
Example #4 — [nslider]
  fan-in:
    in0 ← [p stream-o-notes] ← [active]    # p stream-o-notes emits: "-1"
  fan-out:
    out0 → [makenote 96 150]:in0
```

```
Example #5 — [nslider]
  fan-in:
    in0 ← [p stream-o-notes] ← [active]    # p stream-o-notes emits: "-1"
```

```
Example #6 — [nslider]
  fan-in:
    in0 ← [p stream-o-notes] ← [active]    # p stream-o-notes emits: "-1"
```

```
Example #7 — [nslider]
  fan-in:
    in0 ← [attrui @bgcolor]
    in0 ← [attrui @style]    # Create and apply styles using the format palette or type a style name here
    in0 ← [attrui @clefs]
```

Attributes demonstrated: `@bgcolor`, `@clefs`, `@style`

### polyphonic chords

> The 'chord' message followed by pairs of (note, velocity) information can be used to play chords with the nslider in polyphonic mode.

> chord <note1> <vel1> <note2> <vel2> ... <noteN> <velN>

> first it sends note-offs for currently held notes, then it sends note-on commands for the using the specified note, velocity pairs

> the 'chord' message can ONLY be used in polyphonic mode

```
Example — [nslider]
  fan-in:
    in0 ← [preset]    # when the nslider's state is saved by a preset in polyphonic mode, the preset stores 'chord' messages
    in0 ← [message "flush"]    # send note offs to currently held notes and clear display
    in0 ← [message "chord 60 64 64 64 67 64 71 64"]
    in0 ← [message "chord 60 64 62 64 67 64 72 64"]
  fan-out:
    out0 → [pack]:in0
    out1 → [pack]:in1
```

### basic

```
Example #1 — [nslider]
  fan-in:
    in0 ← [* -1] ← [kslider]
  fan-out:
    out0 → [number]:in0
```

```
Example #2 — [nslider]
  fan-in:
    in0 ← [message "clear"]    # also in polyphonic mode:
    in0 ← [makenote]
    in1 ← [makenote]
```

```
Example #3 — [nslider]
  fan-in:
    in0 ← [number] ← [kslider]
  fan-out:
    out0 → [number]:in0
```

## See also

`kslider`, `makenote`, `notein`, `noteout`, `pictslider`, `rslider`, `slider`
