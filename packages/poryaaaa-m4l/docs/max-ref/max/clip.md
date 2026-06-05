# clip

_max · Math_

> Limit numbers to a range

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | value to be constrained |
| in1 | minimum value |
| in2 | maximum value |
| out0 | output value |

## Arguments

- **minimum** (`number`) _(optional)_ — Minimum value
  The minimum value is the lower limit to the range of output values allowed. If only one argument is present, it is used as both the minimum and maximum limit. If no argument is present, the minimum and maximum limit is 0.
- **maximum** (`number`) _(optional)_ — Maximum value
  The maximum value is the upper limit to the range of output values allowed. If only one argument is present, it is used as both the minimum and maximum limit. If no argument is present, the minimum and maximum limit is 0.

## Messages

- `int(input: int)` — Constrain value to limits
  The number is sent out the outlet, constrained within the minimum and maximum limits specified by the minimum and maximum limits. The clip object adapts the clipped output values to its input, so an integer input value will output an integer value regardless of the numeric type specified as an argument.
- `float(input: float)` — Constrain value to limits
  The number is sent out the outlet, constrained within the minimum and maximum limits. The clip object adapts the clipped output values to its input, so a floating point input value will output a floating point value regardless of the numeric type specified as an argument.
- `list(input: list)` — Constrain list values to limits
  Each number in the list is constrained within the minimum and maximum limits, and the constrained numbers are sent out as a list.
- `set(minimum/maximum: list)` — Set minimum and maximum limits
  The word set, followed by two numbers, resets the minimum and maximum limits.

## Attributes

- `@mode` (int) — Boundary Mode
  When the mode attribute is non-zero, clip outputs 0 for values above its maximum or below its minimum to 0. When mode is disabled, clip constrains its input between its minimum and maximum.
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### mode

```
Example — [clip 40 100]
  fan-in:
    in0 ← [attrui @mode]    # change the mode to 1: Zero and observe the result when you change the input value
    in0 ← [slider]    # change input value
  fan-out:
    out0 → [multislider]:in0
```

Attributes demonstrated: `@mode`

### basic

```
Example — [clip 10 50]
  fan-in:
    in0 ← [number]    # clip adapts output values to the numeric type of its input
    in0 ← [flonum]
    in0 ← [message "set 0 29"]    # change the clipping range with the set message
    in0 ← [message "1. 2 51.23 9 10 50.04"]    # clip will process lists of numbers
    in0 ← [slider]    # input is clipped to range
    in1 ← [flonum]    # output minimum
    in2 ← [flonum]    # output maximum
  fan-out:
    out0 → [print @popup 1]:in0
```

## See also

`maximum`, `minimum`, `split`, `<`, `<=`, `>`, `>=`
