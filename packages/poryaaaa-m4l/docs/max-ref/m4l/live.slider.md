# live.slider

_m4l · Live UI Objects_

> Output numbers by moving a slider onscreen

live.slider is a user interface object that resembles a sliding potentiometer.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | int/float | Parameter Value (0.00-127.00) |
| out0 | int/float | Parameter Value (0.00-127.00) |
| out1 | int/float | Parameter Raw Value (0.-1.) |

## Messages

- `bang` — Send the current value out the outlet
- `int(input: int)` — Store, display, and output a number
  The number received in the inlet is stored and displayed by the live.slider object and sent out the outlet.
- `float(input: float)` — Store, display, and output a number
  The number received in the inlet is stored and displayed by the live.slider object and sent out the outlet.
- `assign(assign-input: float)` — Display and output a value
  The word assign, followed by a floating point value, causes that value to be displayed and sent out the live.slider object's outlet. The value, however, will not be stored. If the Parameter Visibility attribute is set to Stored Only, the assign message will not add the new value to the Live application’s undo chain.
- `init` — Restore and output the initial value
- `outputvalue` — Send the current value out the outlet
- `rawfloat(input: float)` — Store a raw normalized value, convert to real, display, and output
  A raw normalized value (between 0. and 1.) received in the inlet is converted to a real value, stored, displayed by live.slider, and the current value is sent out the outlet.
- `set(set-input: float)` — Set and display the current value without triggering output
  Sets and displays the current value without triggering any output.

## GUI behaviors

- `(mouse)` — Click and drag in the slider to change the value.
  Click and drag in the slider to change the value. Hold down the Shift key for more precise mouse control.

## Attributes

- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)
- `@paint` (int)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — Parameter Raw Value (0.-1.)

### appearance

```
Example — [live.slider] (sliderriffic)
  fan-in:
    in0 ← [attrui @slidercolor]
    in0 ← [attrui @textcolor]
    in0 ← [attrui @tribordercolor]
    in0 ← [attrui @trioncolor]
    in0 ← [attrui @tricolor]
    in0 ← [attrui @active]
    in0 ← [attrui @focusbordercolor]    # Click here and click on the button to see the active color
    in0 ← [attrui @modulationcolor]
```

Attributes demonstrated: `@active`, `@focusbordercolor`, `@modulationcolor`, `@slidercolor`, `@textcolor`, `@tribordercolor`, `@tricolor`, `@trioncolor`

### basic

```
Example — [live.slider] (center frequency)
  fan-in:
    in0 ← [attrui @showname]
    in0 ← [attrui @shownumber]
```

```
Example — [live.slider] (enum)
  (no patch cords)
```

```
Example — [live.slider] (enum)
  (no patch cords)
```

```
Example #4 — [live.slider]
  (no patch cords)
```

```
Example — [live.slider] (center frequency)
  fan-in:
    in0 ← [flonum]
  fan-out:
    out0 → [flonum]:in0
```

Attributes demonstrated: `@showname`, `@shownumber`

## See also

`live.numbox`, `live.dial`, `live.slider`
