# cosh

_max · Math_

> Hyperbolic cosine function

Use the cosh object to calculate and output the hyperbolic cosine of any given number.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Input Value X |
| out0 | Cosh (x) |

## Arguments

- **initial-value** (`float or int`) _(optional)_ — Sets the initial value for the hyperbolic cosine function
  Sets the initial value for the hyperbolic cosine function.

## Messages

- `bang` — Calculate the hyperbolic cosine of the number currently stored
  Calculates the hyperbolic cosine of the number currently stored. If there is no argument, cosh initially holds 0.
- `int(input: int)` — Input to the hyperbolic cosine function
- `float(input: float)` — Input to the hyperbolic cosine function

## Help patcher examples

### basic

```
Example #1 — [cosh 2.12]
  fan-in:
    in0 ← [button]
  fan-out:
    out0 → [flonum]:in0
```

```
Example #2 — [cosh]
  fan-in:
    in0 ← [flonum]
  fan-out:
    out0 → [flonum]:in0
```

## See also

`acos`, `acosh`, `cos`
