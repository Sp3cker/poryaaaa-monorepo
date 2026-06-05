# atan

_max · Math_

> Arc-tangent function

Use the atan object to calculate and output the arc-tangent of any given number.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Input Value X |
| out0 | Atan (x) |

## Arguments

- **initial-value** (`float or int`) _(optional)_ — Sets the initial value for the arc-tangent function
  Sets the initial value for the arc-tangent function.

## Messages

- `bang` — Calculate the arc-tangent of the number currently stored
  Calculates the arc-tangent of the number currently stored. If there is no argument, atan initially holds 0.
- `int(input: int)` — Input to the arc-tangent function
- `float(input: float)` — Input to the arc-tangent function

## Help patcher examples

### basic

```
Example — [atan]
  fan-in:
    in0 ← [flonum]
  fan-out:
    out0 → [flonum]:in0
```

## See also

`atan2`, `atanh`, `tan`, `tanh`
