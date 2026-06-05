# change

_max · Control_

> Filter out repetitions of a number

Output a number only if it is different from the stored number and will reset the stored number to that differing input number. Alternate modes of operation also identify greater-than or less-than the conditions.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Repeated to Output if Number Changes |
| out0 | Number if it Changes |
| out1 | 1 for Logical Transition from 0 to Non-Zero |
| out2 | 1 for Logical Transition from Non-Zero to 0 |

## Arguments

- **initial-value** (`int or float`) _(optional)_ — Set the initial stored value
  Sets the initial value for comparison to incoming numbers. If there is no argument, the initial value is 0.
- **mode** (`symbol`) _(optional)_ — Set detection mode
  A second argument may be + or -, a change to mode + or mode -. Subsequent mode messages can change this behavior.

## Messages

- `int(input: int)` — Send value if changed
  The number is sent out the outlet only if it is different from the currently stored value. Replaces the stored value.
- `float(input: float)` — Send value if changed
  Outputs the number only if it is different from the currently stored value. Replaces the stored value.
- `mode(flag (+ or -): symbol)` — Set detection mode
  Sets the detection mode of change. The word mode, followed by a +, causes change to send a 1 out its left outlet if the received number is greater than the previously received number.
  The word mode, followed by a -, causes change to send out a -1 if the received number is less than the previously received number.
  The word mode by itself returns change to its default mode of sending out received values that differ from the previously received input.
- `set(stored value: number)` — Replace stored value
  Replaces the stored value without triggering output.

## Help patcher examples

### modes

```
Example #1 — [change +]  + mode outputs a 1 whenever input is increasing
  fan-in:
    in0 ← [number]    # values changes are tested using modes
  fan-out:
    out0 → [sel 1]:in0
```

```
Example #2 — [change -]
  fan-in:
    in0 ← [number]    # values changes are tested using modes
  fan-out:
    out0 → [sel -1]:in0    # - mode outputs a -1 whenever input is decreasing
```

### basic

```
Example #1 — [change 0.]  a float argument checks for float changes
  fan-in:
    in0 ← [message "0.5"]
    in0 ← [message "1."]
    in0 ← [message "1.5"]
  fan-out:
    out0 → [flonum]:in0
```

```
Example #2 — [change 0]
  fan-in:
    in0 ← [number] ← [incdec] ← [number]    # input is compared for difference
    in0 ← [message "set 100"]    # set stores number with no output
  fan-out:
    out0 → [number]:in0
    out1 → [print @popup 1]:in0    # number if changed
    out2 → [print @popup 1]:in0    # nonzero-to-zero transition / zero-to-nonzero transition
```

## See also

`peak`, `togedge`, `trough`
