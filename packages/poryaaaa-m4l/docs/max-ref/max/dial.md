# dial

_max · U/I_

> Output numbers using an onscreen dial

Outputs numbers according to its degree of rotation. dial can be set with a certain range, offset, multiplier, as well as numerous visual settings.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Dial Displays Value Received |
| out0 | Outputs Value Received or Entered |

## Messages

- `bang` — Output the current value
  Sends out the number currently stored in dial.
- `int(input: int)` — Display and cause output
  The number received in the inlet is displayed graphically by dial, and is passed out its outlet. Optionally, dial can multiply the number by some amount and add an offset to it before sending it out the outlet.
  The dial will also send out numbers in response to clicking or dragging on it directly with the mouse.
- `float(input: float)` — Display and cause output
  Converted to int.
- `resize(input: int)` — Change the display size
  The word resize, followed by a number, changes the size of the dial dial object in pixels.
- `set(input: int)` — Display and store value, but do not output
  The word set, followed by a number, changes the displayed value of the dial, without triggering output.
- `setminmax(min-max-values: list)` — Set the minimum and maximum values
  The word setminmax, followed by two numbers, sets the low and high range values for the dial object. If the number list consists of floating point values, the floatoutput attribute will automatically be set.

## GUI behaviors

- `(mouse)` — Adjust the current value
  The dial object will send out numbers in response to clicking or dragging on it directly with the mouse.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)
- `@save` (int)

## Help patcher examples

### basic

```
Example #1 — [dial]
  fan-in:
    in0 ← [attrui @min]    # set offset value / float output
  fan-out:
    out0 → [flonum]:in0
```

```
Example #2 — [dial]  endless rotary
  fan-out:
    out0 → [flonum]:in0
```

```
Example #3 — [dial]
  fan-in:
    in0 ← [attrui @mult]    # set output multiplier
  fan-out:
    out0 → [number]:in0
```

```
Example #4 — [dial]
  fan-in:
    in0 ← [attrui @vtracking]    # tracking mode
  fan-out:
    out0 → [number]:in0
```

```
Example #5 — [dial]
  fan-in:
    in0 ← [attrui @size]    # set range
  fan-out:
    out0 → [number]:in0
```

```
Example #6 — [dial]  click-drag on dial to change value
  fan-in:
    in0 ← [message "0"]
    in0 ← [message "10"]
    in0 ← [message "20"]    # click on message boxes to set and output value
  fan-out:
    out0 → [number]:in0
```

Attributes demonstrated: `@min`, `@mult`, `@size`, `@vtracking`

### appearance

> You can change the appearance of dial by adjusting the color, style, mode, and the size of the outline.

```
Example — [dial]
  fan-in:
    in0 ← [attrui @thickness]
    in0 ← [attrui @degrees]    # Adjust the appearance of the outline and needle
    in0 ← [attrui @style]    # Create and apply styles using the format palette or type a style name here
    in0 ← [attrui @outlinecolor]
    in0 ← [attrui @needlecolor]
    in0 ← [attrui @bgcolor]    # Adjust the background color
    in0 ← [attrui @mode]    # Change the drawing style
```

Attributes demonstrated: `@bgcolor`, `@degrees`, `@mode`, `@needlecolor`, `@outlinecolor`, `@style`, `@thickness`

## See also

`pictctrl`, `pictslider`, `rslider`, `slider`
