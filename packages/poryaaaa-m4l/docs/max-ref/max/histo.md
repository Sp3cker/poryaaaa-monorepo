# histo

_max · Data_

> Create a histogram of numbers received

Records and outputs histogram data of the numbers it receives.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Numbers to Keep Track Of (and 'clear' Message) |
| in1 | How Many of a Number |
| out0 | Number Out |
| out1 | Quantity of a Given Number |

## Arguments

- **size** (`int`) _(optional)_ — Histogram size
  histo takes an optional argument to set the size of the histogram. The default size is 128 (0-127).

## Messages

- `bang` — Output count of last value received
  Using the number most recently received in the left inlet, histo reports out the right outlet how many times that number has been received, and sends the number itself out the left outlet. If no number has been previously received in the left inlet, 0 is sent out both outlets.
- `int(count: int)` — Input data and cause output
  In left inlet: histo keeps count of how many times it has received a number between 0 and 127 in the left inlet. When a number is received, histo includes it in the count, sends the number of times that number has been received out the right outlet, and passes the number itself out the left outlet. Numbers outside the range 0-127 are ignored.
- `clear` — Clear histogram counts
  Erases the memory of histo, to begin a new histogram.
- `in1(count: int)` — Query a value count
  In right inlet: Has the same effect as a number in the left inlet, except that the number is not counted by histo.

## Help patcher examples

### basic

```
Example — [histo]
  fan-in:
    in0 ← [message "1, 1, 1, 2, 4, 4, 5, 7, 7, 7, 7"]
    in0 ← [message "clear"]
    in1 ← [message "1"]
    in1 ← [message "2"]
    in1 ← [message "3"]
    in1 ← [message "4"]
    in1 ← [message "5"]
    in1 ← [message "6"]
    in1 ← [message "7"]
  fan-out:
    out0 → [number]:in0
    out1 → [number]:in0
```

## See also

`anal`, `itable`, `prob`, `table`
