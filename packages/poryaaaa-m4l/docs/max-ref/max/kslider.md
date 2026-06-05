# kslider

_max · U/I_

> Output numbers from an onscreen keyboard

Outputs and displays note and velocity information using an on-screen keyboard.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Displays Value Received |
| in1 | Velocity Value Received |
| out0 | Outputs Key Value Changed or Received |
| out1 | 'Velocity' from Mouse Height on Key |

## Messages

- `bang` — Output the current note and velocity
  In left inlet: Sends out the pitch and velocity values currently stored in kslider.
- `int(pitch: int)` — Display and output note
  The number received in the inlet is displayed graphically by kslider if it falls within its displayed range. The current velocity value (from 1 to 127) that kslider holds is sent out its right outlet, followed by the received number out the left outlet.
- `float(pitch: float)` — Display and output note
  Converted to int.
- `chord(value-pairs: list)` — Display and output chords
  In left inlet: The word chord, followed by a list of MIDI note name and velocity pairs, can be used to play chords on the kslider in polyphonic mode (set by the mode 1 message). The chord message sends note-offs for currently held notes, followed by note-on commands for the specified note and velocity pairs. When the kslider object's state is saved by a preset object in polyphonic mode, the preset object will store chord messages.
- `clear` — Clear highlighted notes
  In left inlet: The clear message will clear any currently highlighted notes on the keyboard, but will not trigger any output.
- `flush` — Cause note-offs for held notes
  In left inlet: When the kslider object is in polyphonic mode (set by the mode 1 message), the flush message will send note-offs to currently held notes and clear the kslider object's display.
- `ft1(velocity: float)` — Display and store velocity
  Converted to int.
- `in1(velocity: int)` — Display and store velocity
  In right inlet: The number received in the right inlet sets the output key velocity without triggering output.
- `set(pitch: int, velocity: int)` — Display and output note and velocity
  In left inlet: The word set, followed by a number, changes the value displayed by kslider, without triggering output.
- `size(size-flag: int)` — Set keyboard size
  This is a legacy message - the size of the kslider object can be set by clicking on the object's resize handle and dragging.
  In left inlet: The word size, followed by a zero or one, sets the size of the keyboard display. size 0 (default) sets the large keyboard, and size 1 selects the small keyboard.

## GUI behaviors

- `(mouse)` — Generate note and velocity values
  The kslider object sends out numbers when you click or drag on it with the mouse. The velocity value is determined by the vertical position of the mouse within each key. Higher vertical positions produce higher velocities, to a maximum of 127.
  If the kslider object is in polyphonic mode, you need to click on a key twice: once to send a note-on, and once again for a note-off.
  Clicking on the very rightmost edge of the kslider sends out the note of the key C that would be just to the right of the keys that are visible.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)
- `@save` (int)

## Help patcher examples

### multitouch

```
Example — [kslider]  click and hold a key, then release to see the note-off output
  fan-in:
    in0 ← [attrui @mode]
  fan-out:
    out0 → [number]:in0
    out1 → [number]:in0
```

Attributes demonstrated: `@mode`

### appearance

```
Example — [kslider]  Click on a key to see the selectioncolor
  fan-in:
    in0 ← [attrui @blackkeycolor]
    in0 ← [attrui @style]    # Create and apply styles using the format palette or type a style name here
    in0 ← [attrui @hkeycolor]
    in0 ← [attrui @selectioncolor]
    in0 ← [attrui @whitekeycolor]
```

Attributes demonstrated: `@blackkeycolor`, `@hkeycolor`, `@selectioncolor`, `@style`, `@whitekeycolor`

### chords

> The 'chord' message first sends note-offs for currently held notes, then sends note-on commands using the specified note / velocity pairs.

> when the kslider's state is saved by a preset in polyphonic mode, the preset stores 'chord' messages

```
Example — [kslider]  the 'chord' message can ONLY be used in polyphonic mode
  fan-in:
    in0 ← [message "flush"]    # send note offs to currently held notes and clear display
    in0 ← [message "chord 60 64 64 64 67 64 71 64"]    # chord <note1> <vel1> <note2> <vel2> ... <noteN> <velN>
    in0 ← [message "chord 60 64 62 64 67 64 72 64"]
  fan-out:
    out0 → [pack]:in0
    out1 → [pack]:in1
```

### polyphony

```
Example — [kslider]
  fan-in:
    in0 ← [makenote 86 1000] ← [+ 40] ← [random 40]
    in0 ← [message "flush"]    # clear highlighted notes AND send the necessary note-offs out the outlets
    in0 ← [message "clear"]    # clear highlighted notes (does not cause output)
    in1 ← [makenote 86 1000] ← [+ 40] ← [random 40]
  fan-out:
    out0 → [number]:in0
    out1 → [number]:in0    # if you're mousing while in polyphonic mode you need to click on a key twice: once to send a note-on, and once again for a note-off
```

### basic

```
Example — [kslider]  Click the mouse at different heights on each key and watch velocity output.
  fan-in:
    in0 ← [slider]    # Move the slider and watch the keyboard display part of the range
    in1 ← [number]    # set the output velocity
  fan-out:
    out0 → [number]:in0
    out1 → [number]:in0
```

## See also

`makenote`, `notein`, `noteout`, `nslider`, `pictslider`, `rslider`, `slider`
