# peak

_max · Control_

> Output larger numbers

Compares a number to a previous peak-value and, if larger, it is sent out the output while the new peak-value is set to that number.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Find the highest number (the peak) |
| in1 | Sets the peak |
| out0 | The highest number so far |
| out1 | Goes high for a new peak |
| out2 | Goes high for a number that isn't a new peak |

## Arguments

- **format** (`float`) _(optional)_ — Floating-point output
  The initial value stored in peak is 0. Providing a float argument will cause peak to operate with floating point numbers instead of integers.

## Messages

- `bang` — Send current peak value
  Sends the currently stored peak value out the left outlet.
- `int(input: int)` — Output if peak value
  In left inlet: If the input is greater than the value currently stored in peak, it is stored as the new peak value and is sent out.
- `float(input: float)` — Output if peak value
  In left inlet: If the input is greater than the value currently stored in peak, it is stored as the new peak value and is sent out.
- `ft1(input: float)` — Set new peak value
  In right inlet: The number is stored in peak as the new peak value, and is sent out.
- `in1(input: int)` — Set new peak value
  In right inlet: The number is stored in peak as the new peak value, and is sent out.
- `list(input: number, peak: number)` — Input both values
  In left inlet: The second number is stored as the new peak value and is sent out, then the first number is received in the left inlet.

## Help patcher examples

### basic

```
Example — [peak]
  fan-in:
    in0 ← [number]    # drag number box to test for peak
    in1 ← [number]    # set old peak
  fan-out:
    out0 → [number]:in0    # Current Peak
    out1 → [toggle]:in0    # New Peak
    out2 → [toggle]:in0    # Not Peak
```

## See also

`maximum`, `past`, `trough`, `>`
