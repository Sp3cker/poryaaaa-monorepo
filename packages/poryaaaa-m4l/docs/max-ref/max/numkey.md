# numkey

_max · Interaction_

> Interpret numbers typed on the keyboard

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | ASCII Key Codes From key |
| out0 | Value of Number Typed When Entered |
| out1 | Value of Number While It's Being Typed |

## Arguments

- **format** (`float`) _(optional)_ — Output format
  A float argument causes numkey to understand the decimal point and the fractional part of a number, and send out floats instead of ints. (The argument does not, however, set an initial value for numkey. The initial value is always 0.)

## Messages

- `bang` — Output the currently stored value
  Sends the number currently stored in numkey out the left outlet, and resets the stored number to 0.
- `int(ASCII: int)` — Input an ASCII value
  The number is an ASCII value received from a key or keyup object. When digits are typed on the computer keyboard, numkey recognizes the ASCII values and interprets them as the numbers being typed.
  The keys recognized by numkey are the digits 0-9, the Delete (Backspace) key, decimal point (period), Return, and Enter. Digits are combined as a single number and stored in numkey.
- `clear` — Reset the stored number to 0

## Help patcher examples

### basic

```
Example #1 — [numkey 0.]
  fan-in:
    in0 ← [key]    # (press number keys)
  fan-out:
    out0 → [flonum]:in0    # result (after entered)
    out0 → [print @popup 1]:in0    # floating-point version
    out1 → [flonum]:in0    # current result
```

```
Example #2 — [numkey]
  fan-in:
    in0 ← [key]    # (press number keys)
  fan-out:
    out0 → [print @popup 1]:in0    # integer version
    out0 → [number]:in0    # result (after entered)
    out1 → [number]:in0    # current result
```

## See also

`key`, `keyup`, `number`
