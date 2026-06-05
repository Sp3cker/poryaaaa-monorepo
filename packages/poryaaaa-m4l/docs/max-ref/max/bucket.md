# bucket

_max · Data_

> Pass numbers from outlet to outlet

Outputs incoming values to outlets in bucket-brigade fashion. bucket acts as an n-stage shift register which can shift its contents from outlet to outlet in either direction.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Number to Bucket-brigade |
| out0 | Delay Stage 1 |

## Arguments

- **outlets** (`int`) _(optional)_ — The number of outlets
  Sets the number of outlets. If there is no argument, there will be one outlet.
- **output-flag** (`int`) _(optional)_ — Echo to output mode flag
  A second non-zero argument sets the bucket object to "echo to output" mode, whereby the number received in the inlet is stored and sent out the left outlet when it is received. This makes it somewhat easier to visualize the data coming from the outlets.

## Messages

- `bang` — Output current values
  All stored values are sent out, but their position is not shifted.
- `int(input: int)` — Store and shift data
  The numbers currently stored in bucket are sent out, then each number is moved one outlet to the right and the new number is stored to be sent out the left outlet the next time a number is received.
- `float(input: float)` — Store and shift data
  The numbers currently stored in bucket are sent out, then each number is moved one outlet to the right and the new number is stored to be sent out the left outlet the next time a number is received.
- `L2R` — Set shift function left-to-right
  Sets bucket to shift its stored values from left to right (the default) whenever it receives a number in its inlet.
- `R2L` — Set shift function right-to-left
  Sets bucket to shift its stored values from right to left whenever it receives a number in its inlet, placing the incoming number in the rightmost outlet.
- `clear` — Reset internal values without causing output
  The clear message resets the internal values of bucket without causing any output.
- `freeze` — Suspend output
  Suspends the bucket output, but new incoming numbers continue to shift the stored values internally.
- `l2r` — Set shift function left-to-right
  Sets bucket to shift its stored values from left to right (the default) whenever it receives a number in its inlet.
- `r2l` — Set shift function right-to-left
  Sets bucket to shift its stored values from right to left whenever it receives a number in its inlet, placing the incoming number in the rightmost outlet.
- `roll` — Use the end value as input
  The word roll, followed by any number, causes bucket to use the value stored in its rightmost outlet as input; thus, it sends its output, shifts all stored values to the right, then stores the value which had been in the rightmost outlet in the leftmost outlet (as if it had been received in the inlet).
- `set(input: number)` — Store and output one value in all locations
  The word set, followed by a number, sends that number out each outlet, and stores the number as the next value to be sent out each of its outlets.
- `thaw` — Release a previous freeze action
  Resumes bucket output.

## Help patcher examples

### basic

```
Example #1 — [bucket 4 1]  non-zero output flag sends input to left outlet when it is received
  fan-in:
    in0 ← [flonum]    # works with floats
  fan-out:
    out0 → [flonum]:in0
    out1 → [flonum]:in0
    out2 → [flonum]:in0
    out3 → [flonum]:in0
```

```
Example #2 — [bucket 8]
  fan-in:
    in0 ← [message "roll"]    # shift in loop
    in0 ← [message "l2r"]    # set the shift direction left-to-right
    in0 ← [message "r2l"]    # set the shift direction right-to-left
    in0 ← [message "freeze"]    # suspend bucket output
    in0 ← [message "thaw"]    # resume bucket output
    in0 ← [number]    # input number to shift
    in0 ← [message "set $1"]
    in0 ← [message "clear"]    # reset internal values without causing output
  fan-out:
    out0 → [number]:in0
    out1 → [number]:in0
    out2 → [number]:in0
    out3 → [number]:in0
    out4 → [number]:in0
    out5 → [number]:in0
    out6 → [number]:in0
    out7 → [number]:in0
```

## See also

`cycle`, `decode`, `gate`, `spray`
