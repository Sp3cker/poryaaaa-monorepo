# incdec

_max · U/I_

> Increment and decrement a value

Increment or Decrement a value. When connected to a number box, Click the upper half of the object to increment, click the lower half to decrement.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Number to Inc/Dec |
| out0 | Changed Number |

## Messages

- `bang` — Output the current value
  bang message causes the incdec object to output the currently stored value.
- `int(input: int)` — Store as the current value
  A number sent to the incdec object's inlet sets the value that will be incremented or decremented by clicking on the top or bottom of half of the object. The number is not sent out the outlet. incdec is designed to be used with user interface objects such as the number box, dial, and the various sliders.
- `float(input: float)` — Store as the current value
  A floating-point number sent to the incdec object's inlet sets the value that will be incremented or decremented by clicking on the top or bottom of half of the object. The number is not sent out the outlet. incdec is designed to be used with user interface objects such as the number box, dial, and the various sliders.
- `dec` — Decrement the value and cause output
  The dec message can be used to decrement and output the stored value.
- `inc` — Increment the value and cause output
  The inc message can be used to increment and output the stored value.
- `set(input: int)` — Store as the current value
  The word set followed by an integer value functions identically to the int message, and is provided for convenience.

## GUI behaviors

- `(mouse)` — Increment or decrement the value
  A mouse click increments or decrements the stored value (depending on which arrow is clicked) and sends it out the outlet.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)
- `@save` (int)

## Help patcher examples

### appearance

```
Example — [incdec]
  fan-in:
    in0 ← [attrui @elementcolor]
    in0 ← [attrui @bgcolor]
    in0 ← [attrui @fgcolor]
    in0 ← [attrui @style]    # Create and apply styles using the format palette or type a style name here
```

Attributes demonstrated: `@bgcolor`, `@elementcolor`, `@fgcolor`, `@style`

### basic

```
Example #1 — [incdec]
  fan-in:
    in0 ← [button]    # trigger output again
    in0 ← [message "dec"]    # manually increment/decrement
    in0 ← [message "set 74"]    # (same as int)
    in0 ← [message "inc"]
    in0 ← [number] ← [incdec]
  fan-out:
    out0 → [number]:in0
```

```
Example #2 — [incdec]
  fan-in:
    in0 ← [number] ← [incdec]
  fan-out:
    out0 → [number]:in0
```

```
Example #3 — [incdec]  Floating increment
  fan-in:
    in0 ← [attrui @increment]
    in0 ← [flonum] ← [incdec]    # Connect like this / Floating increment
  fan-out:
    out0 → [flonum]:in0    # Connect like this
```

Attributes demonstrated: `@increment`

## See also

`number`, `umenu`, `slider`
