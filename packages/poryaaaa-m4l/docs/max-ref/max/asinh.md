# asinh

_max · Math_

> Hyperbolic arc-sine function

Use the asinh object to calculate and output the hyperbolic arc-sine of any given number.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Input Value X |
| out0 | Asinh (x) |

## Arguments

- **initial-value** (`float or int`) _(optional)_ — Sets the initial value for the hyperbolic arc-sine function
  Sets the initial value for the hyperbolic arc-sine function.

## Messages

- `bang` — Calculate the hyperbolic arc-sine of the number currently stored.
  Calculates the hyperbolic arc-sine of the number currently stored. If there is no argument, asin initially holds 0.
- `int(input: int)` — Input to the hyperbolic arc-cosine function
- `float(input: float)` — Input to the hyperbolic arc-cosine function

## Help patcher examples

### basic

```
Example — [asinh]
  fan-in:
    in0 ← [flonum]
  fan-out:
    out0 → [flonum]:in0
```

## See also

`asin`, `sin`, `sinh`
