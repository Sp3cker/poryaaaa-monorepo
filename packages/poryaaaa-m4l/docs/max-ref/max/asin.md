# asin

_max · Math_

> Arc-sine function

Use the asin object to calculate and output the arc-sine of any given number.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Input Value X |
| out0 | Asin (x) |

## Arguments

- **initial-value** (`float or int`) _(optional)_ — Sets the initial value for the arc-sine function
  Sets the initial value for the arc-sine function.

## Messages

- `bang` — Calculate the arc-sine of the number currently stored
  Calculates the arc-sine of the number currently stored. If there is no argument, asin initially holds 0.
- `int(input: int)` — Input to the arc-sine function
- `float(input: float)` — Input to the arc-sine function

## Help patcher examples

### basic

```
Example — [asin]
  fan-in:
    in0 ← [flonum]    # click and drag to change input
  fan-out:
    out0 → [flonum]:in0
```

## See also

`asinh`, `sin`, `sinh`
