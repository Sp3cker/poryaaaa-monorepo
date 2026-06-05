# cycle

_max · Data_

> Round-robin messages to outlets

Each incoming number is sent to the next outlet, wrapping around to the first outlet after the last has been reached.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Number to Send to Successive Outlets |
| out0 | Output of Item 1 |

## Arguments

- **outlets** (`int`) _(optional)_ — Number of outlets
  Determines the number of outlets. If there is no argument, there will be one outlet.
- **mode** (`int`) _(optional)_ — Output mode
  Sets the output mode. If it is non-zero, cycle detects separate "events" and restarts at the leftmost outlet when a new event occurs. Examples of separate events include messages with delays between them, and messages triggered by successive mouse clicks or MIDI events. A stream of items separated by commas in a message box is considered a single event. If this argument is not present or is 0, the values cycle through all the outlets, regardless of whether they are attached to separate events or not.

## Messages

- `bang` — Send a bang to the next output
  Sends a bang to the next outlet.
- `int(input: int)` — Send value to the next outlet
  The input to be directed to successive outlets.
- `float(input: float)` — Send value to the next outlet
  The input to be directed to successive outlets.
- `list(input: list)` — Send value to the successive outlets
  The stream of ints, floats, or symbols to be directed to successive outlets.
- `anything(input: list)` — Send value to the successive outlets
  The stream of ints, floats, or symbols to be directed to successive outlets.
- `set(outlet number: int)` — Set next outlet
  The word set, followed by a number, specifies an outlet to which the next input should be directed, if in cycle mode. Outlets are numbered beginning with 0; if an outlet number is specified that does not actually exist, the message is ignored. (This message has no effect when cycle is in event-sensitive mode, in which case each message is always sent out beginning at the leftmost outlet.)
- `symbol(input: symbol)` — The stream of ints, floats, or symbols to be directed to successive outlets.
- `thresh(mode: int)` — Set output mode
  The word thresh, followed by a number, sets the output mode, in the same way as the second typed-in argument. If the number is non-zero, cycle will detect separate "events" and restart at the leftmost outlet whenever a new event occurs. If the number is 0, each number received will be directed to the next outlet in the cycle.

## Help patcher examples

### basic

```
Example #1 — [cycle 5]
  fan-in:
    in0 ← [message "1, 2, 3, 4, 5"]    # click these and watch the number boxes
    in0 ← [message "1 2 3"]    # "cycle" mode
    in0 ← [message "1, 2, 3, 4"]
  fan-out:
    out0 → [number]:in0
    out1 → [number]:in0
    out2 → [number]:in0
    out3 → [number]:in0
    out4 → [number]:in0
```

```
Example #2 — [cycle 5 1]
  fan-in:
    in0 ← [message "1, 2, 3, 4"]    # "event sensitive" mode
    in0 ← [message "1 2 3"]    # click these and watch the number boxes
    in0 ← [message "1, 2, 3, 4, 5"]
  fan-out:
    out0 → [number]:in0
    out1 → [number]:in0
    out2 → [number]:in0
    out3 → [number]:in0
    out4 → [number]:in0
```

## See also

`bucket`, `counter`, `spell`, `spray`
