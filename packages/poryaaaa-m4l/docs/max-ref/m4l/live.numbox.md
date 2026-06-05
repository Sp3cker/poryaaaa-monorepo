# live.numbox

_m4l · Live UI Objects_

> Display and output a number

The live.numbox object is a number box used to display, input, and output numbers.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | int/float | Parameter Value (0.00-127.00) |
| out0 | int/float | Parameter Value (0.00-127.00) |
| out1 | int/float | Parameter Raw Value (0.-1.) |

## Messages

- `bang` — Send the current value out the outlet
- `int(input: int)` — Store, display, and send a number
  The number received in the inlet is stored and displayed by the live.numbox object and sent out the outlet.
- `float(input: float)` — Store, display, and send a number
  The number received in the inlet is stored and displayed by the live.numbox object and sent out the outlet.
- `assign(assign-input: float)` — Display and output a value
  The word assign, followed by a floating point value, causes that value to be displayed and sent out the live.numbox object's outlet. The value, however, will not be stored. If the Parameter Visibility attribute is set to Stored Only, the assign message will not add the new value to the Live application’s undo chain.
- `init` — Restores and outputs the initial value
- `outputvalue` — Send the current value out the outlet
- `rawfloat(input: float)` — Store a raw normalized value, convert to real, display, and output
  A raw normalized value (between 0. and 1.) received in the inlet is converted to a real value, stored, displayed by live.numbox, and the current value is sent out the outlet.
- `set(set-input: float)` — Sets the current value without causing any output

## GUI behaviors

- `(mouse)` — Click and drag to change number values
  Click and drag the live.numbox display to change the value. Hold down the Shift key for more precise mouse control.
  If the live.numbox object is set to store an initial value (set by checking the Initial Enable option in the object's Inspector), double-clicking in the triangle region will restore that value.

## Attributes

- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)
- `@paint` (int)
- `@style` (symbol)

## Help patcher examples

### appearance

```
Example #1 — [live.numbox]
  fan-in:
    in0 ← [attrui @activetricolor2]
    in0 ← [attrui @bordercolor]
    in0 ← [attrui @focusbordercolor]
    in0 ← [attrui @textcolor]
    in0 ← [attrui @tricolor]
    in0 ← [attrui @activebgcolor]
    in0 ← [attrui @active]    # set active state
    in0 ← [attrui @activeslidercolor]
```

```
Example #2 — [live.numbox]
  fan-in:
    in0 ← [attrui @activetricolor2]
    in0 ← [attrui @tricolor2]
    in0 ← [attrui @bordercolor]
    in0 ← [attrui @focusbordercolor]
    in0 ← [attrui @textcolor]
    in0 ← [attrui @tricolor]
    in0 ← [attrui @activebgcolor]
    in0 ← [attrui @active]    # set active state
    in0 ← [attrui @appearance]    # set appearance mode for right live.numbox
    in0 ← [attrui @activeslidercolor]
    in0 ← [attrui @activetricolor]
```

Attributes demonstrated: `@active`, `@activebgcolor`, `@activeslidercolor`, `@activetricolor`, `@activetricolor2`, `@appearance`, `@bordercolor`, `@focusbordercolor`, `@textcolor`, `@tricolor`, `@tricolor2`

### basic

```
Example #1 — [live.numbox]  LCD
  (no patch cords)
```

```
Example #2 — [live.numbox]  bipolar
  (no patch cords)
```

```
Example #3 — [live.numbox]  enum
  (no patch cords)
```

```
Example #4 — [live.numbox]  slider
  (no patch cords)
```

```
Example #5 — [live.numbox]  appearance modes: / triangle
  (no patch cords)
```

```
Example #6 — [live.numbox]
  fan-in:
    in0 ← [button]    # bang to output
    in0 ← [flonum]    # set value
  fan-out:
    out0 → [flonum]:in0
    out0 → [button]:in0
    out1 → [flonum]:in0    # param value / raw value (0.-1.)
```

## See also

`live.dial`, `live.slider`, `number`, `flonum`
