# minimum

_max · Control_

> Output the smallest value

Watches an input stream for any numbers which are less than its most recently set minimum. If the input value is greater than or equal to the minimum, the minimum value is output. If the input value is less, that value is output.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Compares Left and Right Inlets |
| in1 | Value to be Compared |
| out0 | Minimum of Left and Right Inlets |
| out1 | Index of the Minimum Value |

## Arguments

- **initial** (`int or float`) _(optional)_ — Initial value
  Sets an initial value to be compared with numbers received in the left inlet. If the argument contains a decimal point, all numbers are compared as floats, and the output is a float. If there is no argument, the initial value is 0.

## Messages

- `bang` — Output the most recent minimum
  In left inlet: Sends the most recent output out the outlet again.
- `int(input: int)` — Compare and output minimum
  In left inlet: If the number is less than the value currently stored in minimum, it is sent out the outlet. Otherwise, the stored value is sent out.
- `float(input: float)` — Compare and output minimum
  In left inlet: Converted to int, unless there is a float argument, in which case all numbers are compared as floats.
- `list(input: list)` — Compare all values and output minimum
  In left inlet: The numbers in the list are all compared to each other, and the smallest value is sent out the outlet. The value stored in minimum is replaced by the next smallest value in the list. The minimum object accepts lists of up to 256 elements.
- `ft1(comparison-value: float)` — Set a new comparison value
  In right inlet: Converted to int, unless there is a float argument, in which case all numbers are compared as floats.
- `in1(value: int)` — Set a new comparison value
  In right inlet: The number is stored for comparison with subsequent numbers received in the left inlet.

## Help patcher examples

### basic

```
Example #1 — [minimum]
  fan-in:
    in0 ← [message "30 89 77 21"]
  fan-out:
    out0 → [number]:in0
    out1 → [number]:in0    # also finds the minimum in a list of numbers / index (starts at 0) of the minimum value
```

```
Example #2 — [minimum 34.6]  floats
  fan-in:
    in0 ← [flonum] ← [dial]
  fan-out:
    out0 → [print floatmin @popup 1]:in0
```

```
Example #3 — [minimum]
  fan-in:
    in0 ← [number] ← [dial]
    in1 ← [number] ← [dial]
  fan-out:
    out0 → [print min @popup 1]:in0
```

## See also

`maximum`, `trough`, `<`
