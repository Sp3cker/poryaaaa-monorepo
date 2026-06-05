# atan2

_max · Math_

> Two-variable arc-tangent function

Use the atan2 object to calculate and output the arc-tangent of any two given numbers where the left input is the y value and the right input is the x value.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Y value |
| in1 | X value |
| out0 | atan2(y/x) |

## Arguments

- **x-value** (`number`) _(optional)_ — Sets the initial X value for the arc-tangent function
  Sets the initial X value for the arc-tangent function.

## Messages

- `bang` — Calculate the arc-tangent of the numbers currently stored
  Calculates the arc-tangent of the numbers currently stored. If there is no argument, atan2 initially holds 0 for both input values.
- `int(y-value: int)` — In left input: y value input to an arc-tangent function
- `float(y-value: float)` — In left input: y value input to an arc-tangent function
- `ft1(x-value: float)` — In right input: x value input to an arc-tangent function
- `in1(x-value: int)` — In right input: x value input to an arc-tangent function

## Help patcher examples

### basic

```
Example — [atan2]
  fan-in:
    in0 ← [flonum]    # y
    in1 ← [flonum]    # x
  fan-out:
    out0 → [flonum]:in0
```

## See also

`atan`, `atanh`, `tan`
