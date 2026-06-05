# abs

_max · Math_

> Calculate an absolute value

Outputs the absolute (non-negative) value of any given number. Floats will be output only if the argument to abs is a float.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Input for Absolute Value |
| out0 | Absolute Value of Input |

## Arguments

- **format** (`number`) _(optional)_ — Output format
  Float argument forces a float output.

## Messages

- `int(input: int)` — Output the absolute value
  The absolute (non-negative) value of the input is sent out the output.
- `float(input: float)` — Output the absolute value
  Converted to int, unless abs has a float argument.

## Help patcher examples

### basic

```
Example #1 — [abs]
  fan-in:
    in0 ← [number] ← [- 20] ← [slider]
  fan-out:
    out0 → [number]:in0
```

```
Example #2 — [abs 0.]  floating-point version
  fan-in:
    in0 ← [flonum] ← [- 19.6] ← [slider]
  fan-out:
    out0 → [flonum]:in0
```

## See also

`expr`
