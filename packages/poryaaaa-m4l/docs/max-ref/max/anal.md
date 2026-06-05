# anal

_max · Data_

> Make a histogram of number pairs

Reports how many times a number pair has been received.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Incoming Number Stream |
| out0 | Transitions Seen Between Current and Previous |

## Arguments

- **input-limit** (`int`) _(optional)_ — Limit the input values
  Sets a maximum limit for the values that can be input into anal. Input values are clipped between zero and this value. The default value, when no argument is present, is 128. By supplying an argument, you can change the maximum input value up to a maximum of 16384.

## Messages

- `int(input: int)` — Report numeric frequency
  Reports how many times this number and the previously received number have occurred in immediate succession. (The first time a number is received, there has been no previous number, so nothing happens.)
- `clear` — Clear pairing counts
  Erases the memory of the anal object entirely, but retains the most recently received number to use as the next "previous" value.
- `reset` — Remove a prior value
  Erases the most recently received number from the memory of the anal object. The next number to be received gets stored in its place, to serve as the next "previous" value (but nothing else happens).

## Help patcher examples

### basic

```
Example — [anal]
  fan-in:
    in0 ← [message "reset"]    # completely erase memory
    in0 ← [message "clear"]    # clear analysis, but remember the last int in the inlet...
    in0 ← [number]
  fan-out:
    out0 → [message "35 37 1"]:in1
```

## See also

`histo`, `prob`
