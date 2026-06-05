# past

_max · Control_

> Notify when a threshold is passed

Sends a bang when a number rises above a certain specified value.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Input to be Synchronized |
| out0 | bang When Numbers Rise Above Specified Value |

## Arguments

- **list** (`list`) — Threshold list
  Sets the numbers which must be exceeded by the numbers received in the inlet. Output will be triggered when any of the threshold values are met or exceeded.
- **number** (`int`) — Threshold number
  Triggers output when the number is met or exceeded.

## Messages

- `int(input: int)` — Test for threshold
  If there is only one argument, and the input is greater than the argument, and the previous input was not greater than or equal to it, past sends a bang out the outlet.
- `float(input: float)` — Test for threshold
  If there is only one argument, and the input is greater than the argument, and the previous input was not greater than or equal to it, past sends a bang out the outlet.
- `list(input: list)` — Test values for threshold
  The numbers in the list are compared to the arguments. If all of the numbers in the list are greater than or equal to the corresponding arguments, a bang is sent out the outlet. Before a bang is sent again, however, past must receive a clear message, or must receive another list in which the number that equaled or exceeded its argument goes back below (is less than) its argument.
- `clear` — Reset threshold status
  Causes past to forget previously received input, readying it to send a bang message again.
- `set(watch-list: list)` — Set the threshold value(s)
  The word set, followed by one or more numbers, sets the numbers which must be equaled or exceeded by the numbers received in the past object's inlet.

## Help patcher examples

### basic

```
Example #1 — [past 50]
  fan-in:
    in0 ← [message "50 50 50"]    # no
    in0 ← [message "50 51 50"]    # yes
    in0 ← [message "49 51 50"]    # no
  fan-out:
    out0 → [button]:in0
    out0 → [print @popup 1]:in0
```

```
Example #2 — [past 80]
  fan-in:
    in0 ← [message "set 25"]    # define value(s) to compare
    in0 ← [slider]    # 80 / Move the slider above "80" and then back down again
    in0 ← [message "clear"]    # forget previously received input
  fan-out:
    out0 → [button]:in0
```

```
Example #3 — [past 0 0 2]
  fan-in:
    in0 ← [message "0 0 0"]    # accepts lists, too!
    in0 ← [message "0 0 2"]
    in0 ← [message "set 0 0 1"]
    in0 ← [message "0 0 3"]    # to trigger a bang, at least one element of the list must be greater than the value(s) to compare and all values must be at least equal
    in0 ← [message "set 0 0 2"]
  fan-out:
    out0 → [button]:in0
    out0 → [print @popup 1]:in0
```

## See also

`maximum`, `peak`, `>`
