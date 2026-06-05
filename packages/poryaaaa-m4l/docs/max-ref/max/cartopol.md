# cartopol

_max · Math_

> Convert cartesian to polar coordinates

Converts a cartesian-coordinate pair consisting of real and imaginary values into a polar-coordinate pair consisting of distance and angle values.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | real/x input |
| in1 | imaginary/y input |
| out0 | amplitude/alpha output |
| out1 | phase/theta output |

## Messages

- `bang` — Output most recent values
  bang will output the most recently stored coordinate pair conversion.
- `int(coordinate: int)` — Convert cartesian value
  Converted to float.
- `float(coordinate: float)` — Convert cartesian value
  In left inlet: The x coordinate of a Cartesian pair to be converted into a polar coordinate pair consisting of distance and angle values. When used in an audio context, the value represents the real part of a frequency domain value to be converted into a polar coordinate pair consisting of amplitude and phase values.
  In right inlet: The y coordinate of a Cartesian pair to be converted into a polar coordinate pair consisting of distance and angle values. When used in an audio context, the value represents the imaginary part of a frequency domain value to be converted into a polar coordinate pair consisting of amplitude and phase values.

## Help patcher examples

### basic

```
Example — [cartopol]
  fan-in:
    in0 ← [message "0 0"]    # click and drag on the LCD
    in0 ← [p offset] ← [lcd]
    in0 ← [flonum]
    in1 ← [flonum]    # real/imaginary
  fan-out:
    out0 → [flonum]:in0
    out1 → [/ 3.141593]:in0
    out1 → [flonum]:in0    # amplitude/angle
```

## See also

`atan2`, `lcd`, `poltocar`, `pow`
