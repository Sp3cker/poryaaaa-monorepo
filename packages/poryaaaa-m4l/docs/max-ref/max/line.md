# line

_max · Data, Timing_

> Generate timed ramp

Generate ramps and line segments from one value to another within a specified amount of time.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Destination Value of Ramp |
| in1 | Total Ramp Time in Milliseconds |
| in2 | Time Grain in Milliseconds |
| out0 | Ramp Output |
| out1 | Signals End of Ramp |

## Arguments

- **initial** (`number`) _(optional)_ — Initial value and type
  Sets the initial value to be stored in line and the output type for the object (floating-point or integer). If there is no argument, the initial value is 0 and the output type is int.
- **grain** (`number`) _(optional)_ — Output granularity
  Sets an initial value for the grain: the time interval at which numbers are sent out. If the grain is not specified, line outputs a number every 20 milliseconds. The minimum grain allowed is 1 millisecond; any number less than 1 will be set to 20.

## Messages

- `int(input: int)` — Function depends on inlet
  In left inlet: The number is the target value, to be arrived at in the time specified by the number in the middle inlet. If no time has been specified since the last target value, the time is considered 0 and line immediately outputs the target value.
  Note: the output type for the line object is set by using the first argument to the object (see Arguments).
  In middle inlet: The number is the time, in milliseconds, in which to arrive at the target value.
  In right inlet: The number is the interval (in milliseconds) at which intermediary numbers are regularly sent out.
- `float(input: float)` — Function depends on inlet
  Performs the same function as int but with floats only if the object-argument is a float.
- `list(input: list)` — Set target value and time
  Use various list combinations to reach a target value.
  In one list combination, the first number specifies a starting value, followed by a comma and a number pair. The first number in the pair specifies the target value. The second number of the pair specifies the total amount of time (in milliseconds) in which line should reach the target value. In that amount of time, numbers are output regularly in a line from the currently stored value to the target value.
  An example of this type of list is 0, 1 1000 0 1000. In this example, line would go from the starting value of 0 to 1 in one second, then back down to 0 in one second. Once the first ramp has reached its target value, the next one starts. A subsequent list, float, or int in the left inlet clears all ramps yet to be generated.
  In another combination, the first number specifies a target value, not followed a comma, and the second number specifies a total amount of time (in milliseconds) in which line should reach the target value. The third number, which is optional, sets the grain. Grain will affect the time interval at which numbers are sent out. Once grains are set in a list, they will override the default until manually reset.
  An example of this type of list is 1 1000 100. In this example, line would go from the current value to 1 in a second, outputting a value every 100 milliseconds.
  If the list has an even number of elements greater than three, each pair of elements is considered a destination-ramptime pair in a breakpoint function. If the list has an odd number of elements greater than three, the last element will be ignored.
- `clock(setclock object name: symbol)` — Select a clock for timing
  The word clock, followed by the name of an existing setclock object, sets the line object to be controlled by that setclock object rather than by Max’s internal millisecond clock. The word clock by itself sets the line object back to using Max’s regular millisecond clock.
- `pause` — Pause ramp
  In left inlet: Pauses the internal ramp but does not change the target value nor clear pending target-time pairs. line will continue outputting whatever value was its current value when the pause message was received, until either it receives a resume message or until a new ramp is input.
- `resume` — Resume ramp
  In left inlet: Resumes the internal ramp and subsequent pending target-time pairs if the line object was paused as a result of the pause message.
- `set(input: float)` — Set a new starting value
  In left inlet: The word set, followed by a number, makes that number the new starting value from which to proceed to the next received target value. The set message also stops line if it is in the process of sending out numbers.
- `stop` — Stop generating output
  In left inlet: Stops line from sending out numbers, until a new target value is received.

## Attributes

- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `compatmode` — seen as: `compatmode $1`

## Help patcher examples

### compatibility

> new behavior: if distance is <= 1 and line does not have a float argument and step size is < 0.4, line outputs floating point values.

> new behavior: line calculates its increment differently when the grain does not divide equally into the ramp time, so that the total time of the ramp is always accurate

> new behavior: line does not output the first incremented value immediately (more obvious when grain size is large) which permits the ramp to take exactly the right time

```
Example #1 — [line 0 500]
  fan-in:
    in0 ← [message "0, 4000 4000"]    # ramp from 0 to 4000 in 4000 ms
    in0 ← [message "compatmode $1"]
  fan-out:
    out0 → [flonum]:in0
    out0 → [print @deltatime 1]:in0    # check the Max console!
    out1 → [timer]:in1
```

```
Example #2 — [line 0 500]
  fan-in:
    in0 ← [message "compatmode $1"]
    in0 ← [message "0, 4000 4100"]    # ramp from 0 to 4000 in 4100 ms
  fan-out:
    out0 → [flonum]:in0
    out0 → [print @deltatime 1]:in0    # check the Max console!
    out1 → [timer]:in1
```

```
Example #3 — [line]
  fan-in:
    in0 ← [message "compatmode $1"]
    in0 ← [message "0, 1 1000"]    # ramp from 0 to 1 in 1000 ms
  fan-out:
    out0 → [flonum]:in0
```

### multi segment

> line defaults to a maximum of 129 user defined points. To allow a greater number of points, use the @maxpoints attribute.

```
Example #1 — [line 0.]
  fan-in:
    in0 ← [function] ← [button]
  fan-out:
    out0 → [flonum]:in0
```

```
Example #2 — [line 0.]
  fan-in:
    in0 ← [message "0.25 50 0.15 150 0.5 250 0.25 350 0.75 450 0.35 350 1. 250 0. 1000"]
  fan-out:
    out0 → [multislider]:in0
```

```
Example #3 — [line @maxpoints 256]  This line is set to a maximum of 256 user defined points.
  fan-in:
    in0 ← [zl.group 512 @zlmaxsize 512] ← [pack 0. 0]
  fan-out:
    out0 → [multislider]:in0
```

### lists and ramps

```
Example — [line 1 20]
  fan-in:
    in0 ← [message "100, 0 750"]    # set a starting point of 100, then ramp to 0 in 750 ms
    in0 ← [message "100 1000"]    # ramp to 0 in 1000 ms / ramp to 100 in 1000 ms
    in0 ← [message "0 1000"]
    in0 ← [message "1000"]
    in0 ← [message "42"]    # send a single number to the left inlet without a number to the middle inlet to output the number immediately
    in1 ← [flonum] ← [message "500"]    # Ramp time (ms)
    in2 ← [flonum]    # For floating point output, use a floating point value as the first argument to the line object / Output interval (ms)
  fan-out:
    out0 → [number]:in0
    out1 → [button]:in0
```

### basic

```
Example #1 — [line 0.]  For float output, give the line object a float value for its arg
  fan-in:
    in0 ← [message "3.5, 19.25 400"]    # ramp from 3.5 to 19.25 in 400 ms
  fan-out:
    out0 → [flonum]:in0
    out1 → [button]:in0
```

```
Example #2 — [line 1 20]
  fan-in:
    in0 ← [message "50, 150 1000"]
    in1 ← [flonum]    # Set the ramp time in milliseconds (int or float)
    in2 ← [flonum]    # Set the output interval (grain) in milliseconds. The default is 20 milliseconds
  fan-out:
    out0 → [number]:in0    # The output is int or float depending on the first argument to the line object (the default is int)
    out1 → [button]:in0    # a bang is output when the ramp has completed
```

## See also

`bline`, `funbuff`, `line~`, `setclock`, `uzi`
