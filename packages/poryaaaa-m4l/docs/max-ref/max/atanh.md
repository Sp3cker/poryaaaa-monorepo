# atanh

_max · Math_

> Hyperbolic arc-tangent function

Use the atanh object to calculate and output the hyperbolic arc-tangent of any given number.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Input Value X |
| out0 | Atanh (x) |

## Arguments

- **initial-value** (`float or int`) _(optional)_ — Sets the initial value for the hyperbolic arc-tangent function
  Sets the initial value for the hyperbolic arc-tangent function.

## Messages

- `bang` — Calculate the hyperbolic arc-tangent of the number currently stored.
  Calculates the hyperbolic arc-tangent of the number currently stored. If there is no argument, atanh initially holds 0.
- `int(input: int)` — Input to the hyperbolic arc-tangent function
- `float(input: float)` — Input to the hyperbolic arc-tangent function

## Help patcher examples

### basic

```
Example — [atanh]
  fan-in:
    in0 ← [flonum]
  fan-out:
    out0 → [flonum]:in0
```

## See also

`atan`, `atan2`, `tan`, `tanh`
