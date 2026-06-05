# mean

_max · Math_

> Calculate a running average

Calculates the mean (average) of all the numbers it has received and outputs it.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | int/float/list | (int/float/list) Numbers to be averaged |
| out0 | float | (float) Mean value |
| out1 | int | (int) Sample size |

## Messages

- `bang` — Output previous calculation
  Sends out the previous output (the stored average value).
- `int(input: int)` — Calculate and cause output
  The number is added to the sum of all numbers received up to that point, and the mean is sent out.
- `float(input: float)` — Calculate and cause output
  The number is added to the sum of all numbers received up to that point, and the mean is sent out.
- `list(input: list)` — Calculate mean of list elements
  The numbers in the list are added together, the sum is divided by the number of items in the list, and the mean is sent out. All previously received numbers are cleared from memory.
- `clear` — Clear previous values
  Resets the stored and calculated contents of the object to zero.

## Help patcher examples

### basic

```
Example — [mean]
  fan-in:
    in0 ← [message "clear"]
    in0 ← [number] ← [slider]    # drag slider
    in0 ← [button]    # repeat most recent output / zero the memory
  fan-out:
    out0 → [slider]:in0
    out1 → [number]:in0    # how many numbers have been received
```

## See also

`accum`, `anal`, `bag`, `histo`, `prob`
