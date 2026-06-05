# acosh

_max · Math_

> Hyperbolic arc-cosine function

Use the acosh object to calculate and output the hyperbolic arc-cosine of any given number.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Input Value X |
| out0 | Acosh (x) |

## Arguments

- **initial-value** (`float or int`) _(optional)_ — Sets the initial value for the hyperbolic arc-cosine function
  Sets the initial value for the hyperbolic arc-cosine function.

## Messages

- `bang` — Calculate a hyperbolic arc-cosine of the number currently stored.
  Calculates a hyperbolic arc-cosine of the number currently stored. If there is no argument, acosh initially holds 0.
- `int(input: int)` — Input to the hyperbolic arc-cosine function
- `float(input: float)` — Input to the hyperbolic arc-cosine function

## Help patcher examples

### basic

```
Example — [acosh]
  fan-in:
    in0 ← [flonum]
  fan-out:
    out0 → [flonum]:in0
```

## See also

`acos`, `cos`, `cosh`
