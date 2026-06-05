# cos

_max · Math_

> Cosine function

Use the cos object to calculate and output the cosine of any given number.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Input Value X |
| out0 | Cos (x) |

## Arguments

- **initial-value** (`float or int`) _(optional)_ — Sets the initial value for the cosine function
  Sets the initial value for the cosine function.

## Messages

- `bang` — Calculate the cosine of the number currently stored
  Calculates the cosine of the number currently stored. If there is no argument, cos initially holds 0.
- `int(input: int)` — Input to the cosine function
- `float(input: float)` — Input to the cosine function

## Help patcher examples

### basic

```
Example #1 — [cos 2.12]
  fan-in:
    in0 ← [button]
  fan-out:
    out0 → [flonum]:in0
```

```
Example #2 — [cos]
  fan-in:
    in0 ← [flonum]
  fan-out:
    out0 → [flonum]:in0
```

## See also

`acos`, `acosh`, `cosh`
