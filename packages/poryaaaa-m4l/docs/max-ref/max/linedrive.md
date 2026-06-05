# linedrive

_max · Math_

> Scale numbers exponentially

Scales number from one range to another with an exponential curve. Both the input and output ranges are expressed as single arguments representing the maximum value. The minimum values are the negative values of the ranges (argument * -1).

## Arguments

- **input** (`number`) — Input range
  The first argument is the maximum input value. The range used is from (argument * -1) to (argument).
- **output** (`number`) — Output range
  The second argument is the maximum output value. The range used is from (argument * -1) to (argument).
- **curve** (`number`) — Output curve
  The third argument specifies the nature of the scaling curve. The third argument must be greater than 1. The larger the value, the more steeply exponential the curve is. An appropriate value for this argument is 1.06.
- **delay** (`int`) — Delay
  The fourth argument specifies a ramp time (slew rate). The linedrive object outputs a list consisting of the scaled output value followed by a ramp time in milliseconds which can be sent to a line or line~ object. The initial argument value can be modifed using the linedrive object's right inlet.

## Messages

- `int(input: int)` — Convert value
  In left inlet: The number is converted according to the following expression
  y = b e^{-a log c} e^{x log c}
  where x is the input, y is the output, a, b, and c are the three typed-in arguments, and e is the base of the natural logarithm (approximately 2.718282). The output is a two-item list containing y followed by the delay time most recently received in the right inlet.
- `float(input-to-conversion: float)` — Convert value
  In left inlet: The number is converted according to the following expression
  y = b e^{-a log c} e^{x log c}
  where x is the input, y is the output, a, b, and c are the three typed-in arguments, and e is the base of the natural logarithm (approximately 2.718282). The output is a two-item list containing y followed by the delay time most recently received in the right inlet.
- `in1(delay: int)` — Set delay time
  In right inlet: Sets the current delay time appended to the scaled output. A connected line~ object will ramp to the new target value over this time interval.

## Help patcher examples

### line(~)

```
Example — [linedrive 127 1. 1.06 200]
  fan-in:
    in0 ← [slider] ← [ctlin 7]
    in1 ← [number]    # ramp time
  fan-out:
    out0 → [line~]:in0
    out0 → [message ""]:in1
```

### basic

```
Example — [linedrive 127 1. 1.06 30]
  fan-in:
    in0 ← [number] ← [slider]
    in1 ← [number]    # slew time
  fan-out:
    out0 → [f]:in0
    out0 → [message "0.943396 30"]:in1
```

## See also

`expr`, `scale`
