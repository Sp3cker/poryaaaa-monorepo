# bline

_max · Data, Timing_

> Generate ramps using bang

Generates a linear ramp driven by incoming bang messages. It takes a list of breakpoint segments (and the number of events to span) and outputs a smooth ramp between values.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | bang, list, stop |
| out0 | ramp output |
| out1 | bang when bline reaches destination |

## Arguments

- **initial-value** (`number`) _(optional)_ — The initial value
  Optional. An argument may be used to set the initial value to be stored and the output type for the object--if the first argument is an int, the bline object outputs integer values, and a float will set the bline object to output floating point values. If there is no argument, the initial value is 0 and the output type is int.

## Messages

- `bang` — Output a ramp step
  Sends a new step in the breakpoint list out the left outlet. If the current list of ramp segments is finished, a bang message will be sent out the right outlet
- `int(input: int)` — Set the current value
  Sets the bline object to the specified integer value. Any and all pending breakpoint segments are forgotten (i.e. the time is considered 0 and bline outputs the target value when it receives a bang).
- `float(input: float)` — Set the current value
  Sets the bline object to the specified float value. Any and all pending breakpoint segments are forgotten (i.e. the time is considered 0 and bline outputs the target value when it receives a bang).
- `list(segment-pairs: list)` — Set segment values and event counts
  The bline object sets breakpoint segment values using lists of data composed of pairs of numbers. The first number in each pair can be either an int or a float specifying a target value, followed by an integer that specifies the number of bang messages that will have to be received before reaching the target value--note that this differs from other Max breakpoint objects like line, which specify a time-to-target value in milliseconds.
- `set(input: number)` — Set the current value
  Sets the bline object to the specified value. Any and all pending breakpoint segments are forgotten (i.e. the time is considered 0 and bline outputs the target value when it receives a bang).
- `stop` — Stop output generation
  Stops bline from sending out numbers, until a new list of ramp segments is received.

## Help patcher examples

### basic

> The bline object is useful for generating ramps over a period which has no specified timebase (such as when using MSP's Non-RealTime mode or working in Jitter when the length of time needed to process a single matrix varies).

```
Example — [bline 0.]
  fan-in:
    in0 ← [message "stop"]    # stop the line
    in0 ← [message "0. 20 0.5 5 0.5 3 20. 4"]    # bline works with a multisegment ramp, too / trigger the next value in ramp
    in0 ← [button]
    in0 ← [message "1. 20"]    # go to 1. over 20 events
    in0 ← [flonum]    # • go to a number immediately without causing output
  fan-out:
    out0 → [flonum]:in0    # current output
    out1 → [button]:in0    # bang when ramp is completed
```

## See also

`funbuff`, `line`, `uzi`
