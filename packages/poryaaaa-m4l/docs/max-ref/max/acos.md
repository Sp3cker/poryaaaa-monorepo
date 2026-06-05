# acos

_max · Math_

> Arc-cosine function

Use the acos object to calculate and output the arc-cosine of any given number.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Input Value X |
| out0 | Acos (x) |

## Arguments

- **initial-value** (`float or int`) _(optional)_ —
  Sets the initial value for the arc-cosine function.

## Messages

- `bang` — Calculate the arc-cosine of the number currently stored
  Calculates the arc-cosine of the number currently stored. If there is no argument, acos initially holds 0.
- `int(input: int)` — Input to the arc-cosine function
- `float(input: float)` — Input to the arc-cosine function

## Help patcher examples

### basic

```
Example — [acos]
  fan-in:
    in0 ← [flonum]
  fan-out:
    out0 → [flonum]:in0
```

## See also

`acosh`, `cos`, `cosh`
