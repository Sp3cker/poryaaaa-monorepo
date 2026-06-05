# maximum

_max · Control_

> Output the highest value

Watches an input stream for any numbers which are greater than its most recently set maximum. If the input value is less than or equal to the maximum, the maximum value is output. If the input value is greater, that value is output.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Compares Left and Right Inlets |
| in1 | Value to be Compared |
| out0 | Maximum of Left and Right Inlets |
| out1 | Index of the Maximum Value |

## Arguments

- **initial** (`int or float`) _(optional)_ — Initial maximum value
  Sets an initial value to be compared with numbers received in the left inlet. If the argument contains a decimal point, all numbers are compared as floats, and the output is a float. If there is no argument, the initial value is 0.

## Messages

- `bang` — Output most recent maximum
  In left inlet: Sends the most recent output out the outlet again.
- `int(input: int)` — Compare and output maximum
  In left inlet: If the number is greater than the value currently stored in maximum, it is sent out the outlet. Otherwise, the stored value is sent out.
- `float(input: float)` — Compare and output maximum
  Converted to int, unless there is a float argument, in which case all numbers are compared as floats.
- `list(input: list)` — Compare all values and output maximum
  In left inlet: The numbers in the list are all compared to each other, and the greatest value is sent out the outlet. The value stored in maximum is replaced by the next greatest value in the list.The maximum object accepts lists of up to 256 elements.
- `ft1(input: float)` — Set a new comparison value
  Converted to int, unless there is a float argument, in which case all numbers are compared as floats.
- `in1(input: int)` — Set a new comparison value
  In right inlet: The number is stored for comparison with subsequent numbers received in the left inlet.

## Help patcher examples

### basic

```
Example #1 — [maximum]
  fan-in:
    in0 ← [message "30 89 77 21"]
  fan-out:
    out0 → [number]:in0
    out1 → [number]:in0    # also finds the maximum in a list of numbers / index (starts at 0) of the maximum value
```

```
Example #2 — [maximum 34.6]  floats
  fan-in:
    in0 ← [flonum] ← [dial]
  fan-out:
    out0 → [print floatmax @popup 1]:in0
```

```
Example #3 — [maximum]
  fan-in:
    in0 ← [number] ← [dial]
    in1 ← [number] ← [dial]
  fan-out:
    out0 → [print max @popup 1]:in0
```

## See also

`minimum`, `past`, `peak`, `>`
