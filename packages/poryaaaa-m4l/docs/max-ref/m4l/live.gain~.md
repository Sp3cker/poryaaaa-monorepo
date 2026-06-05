# live.gain~

_m4l · Live UI Objects, Live MSP Objects_

> Decibel volume slider and monitor

live.gain~ is a slider that scales input audio signals and provides a visual indication of the current sound level on a deciBel scale.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | signal | Audio Signal to be Scaled (ch 1), (Int/Float) Parameter Value (-70.00-6.00) |
| in1 | signal | Audio Signal to be Scaled (ch 2) |
| out0 | signal | Scaled Signal (ch 1) |
| out1 | signal | Scaled Signal (ch 2) |
| out2 | int/float | Parameter Value (-70.00-6.00) |
| out3 | int/float | Parameter Raw Value (0.-1.) |
| out4 | float/list | Amplitude of Both Channels (in dB) |

## Messages

- `bang` — Send the current value out the outlet
- `int(db-input: int)` — Set the level (in dB)
  The number received in the inlet sets the level (in dB).
- `float(db-input: float)` — Set the level (in dB)
  The number received in the inlet sets the level (in dB).
- `assign(assign-input: float)` — Display and output a value
  The word assign, followed by a floating point value, causes that value to be displayed and sent out the live.gain~ object's outlet. The value, however, will not be stored. If the Parameter Visibility attribute is set to Stored Only, the assign message will not add the new value to the Live application’s undo chain.
- `init` — Restore and output the initial value
- `outputvalue` — Send the current value out the outlet
- `rawfloat(input: float)` — Store a raw normalized value, convert to real, display, and output
  A raw normalized value (between 0. and 1.) received in the inlet is converted to a real value, stored, displayed by live.gain~ object, and the current value is sent out the outlet.
- `set(set-slider-value: float)` — Set the current value without causing output
  Sets the current value without causing output.
- `signal` — Function depends on inlet
  In left inlet: The input signal (left channel) to be scaled.
  In right inlet: The input signal (right channel) to be scaled.

## GUI behaviors

- `(mouse)` — Click and drag to change the amplification.
  Click and drag the slider to change the amplification. Hold down the Shift key for more precise mouse control.

## Attributes

- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)
- `@paint` (int)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `interp` — seen as: `interp $1`
- `metering` — seen as: `metering $1`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out2` — Parameter Value (-70.00-6.00)
> - `out3` — Parameter Raw Value (0.-1.)

### mc

> For MC signals, mc.live.gain~ will automatically populate the correct number of channels (if there's enough space). The maximum number of channels allowed is 64.

### MultiChannel

```
Example #1 — [live.gain~]
  (no patch cords)
```

```
Example #2 — [live.gain~]
  (no patch cords)
```

```
Example #3 — [live.gain~]
  (no patch cords)
```

```
Example #4 — [live.gain~]
  fan-in:
    in0 ← [p 24-channels-of-cycle~]
    in1 ← [p 24-channels-of-cycle~]
    in2 ← [p 24-channels-of-cycle~]
    in3 ← [p 24-channels-of-cycle~]
    in4 ← [p 24-channels-of-cycle~]
    in5 ← [p 24-channels-of-cycle~]
    in6 ← [p 24-channels-of-cycle~]
    in7 ← [p 24-channels-of-cycle~]
    in8 ← [p 24-channels-of-cycle~]
    in9 ← [p 24-channels-of-cycle~]
    in10 ← [p 24-channels-of-cycle~]
    in11 ← [p 24-channels-of-cycle~]
    in12 ← [p 24-channels-of-cycle~]
    in13 ← [p 24-channels-of-cycle~]
    in14 ← [p 24-channels-of-cycle~]
    in15 ← [p 24-channels-of-cycle~]
    in16 ← [p 24-channels-of-cycle~]
    in17 ← [p 24-channels-of-cycle~]
    in18 ← [p 24-channels-of-cycle~]
    in19 ← [p 24-channels-of-cycle~]
    in20 ← [p 24-channels-of-cycle~]
    in21 ← [p 24-channels-of-cycle~]
    in22 ← [p 24-channels-of-cycle~]
    in23 ← [p 24-channels-of-cycle~]
```

### threshold

```
Example #1 — [live.gain~]
  fan-in:
    in0 ← [*~]
    in0 ← [attrui @threshold_db]
  fan-out:
    out4 → [flonum]:in0
```

```
Example #2 — [live.gain~]
  fan-in:
    in0 ← [*~]
    in0 ← [attrui @threshold_linear]
  fan-out:
    out4 → [flonum]:in0
```

Attributes demonstrated: `@threshold_db`, `@threshold_linear`

### dB/linear

```
Example #1 — [live.gain~]  deciBel display
  fan-in:
    in0 ← [sig~]
  fan-out:
    out4 → [flonum]:in0    # and output
```

```
Example #2 — [live.gain~]  linear display
  fan-in:
    in0 ← [sig~]
  fan-out:
    out4 → [flonum]:in0    # and output
```

### appearance

```
Example — [live.gain~] (gain-o-matic)
  fan-in:
    in0 ← [attrui @warmcolor]
    in0 ← [attrui @modulationcolor]
    in0 ← [attrui @slidercolor]
    in0 ← [attrui @hotcolor]
    in0 ← [attrui @overloadcolor]
    in0 ← [attrui @trioncolor]
    in0 ← [attrui @tribordercolor]
    in0 ← [attrui @textcolor]
    in0 ← [attrui @tricolor]
    in0 ← [attrui @inactivecoldcolor]
    in0 ← [attrui @active]
    in0 ← [attrui @focusbordercolor]
    in0 ← [attrui @inactivewarmcolor]
    in0 ← [attrui @coldcolor]
```

Attributes demonstrated: `@active`, `@coldcolor`, `@focusbordercolor`, `@hotcolor`, `@inactivecoldcolor`, `@inactivewarmcolor`, `@modulationcolor`, `@overloadcolor`, `@slidercolor`, `@textcolor`, `@tribordercolor`, `@tricolor`, `@trioncolor`, `@warmcolor`

### range

> range -24 <-> 0 dB display_range -24 <-> +12 dB

```
Example #1 — [live.gain~]
  fan-in:
    in0 ← [dbtoa~] ← [number~]
```

```
Example #2 — [live.gain~]  -24 <-> +12 dB
  fan-in:
    in0 ← [dbtoa~] ← [number~]
```

```
Example #3 — [live.gain~]
  fan-in:
    in0 ← [sig~ 1]    # By default, live.gain~ range is the same as the display_range (-70dB <-> +6dB)
```

### size and orientation

```
Example #1 — [live.gain~]  orientation horizontal, without parameter name nor parameter value
  (no patch cords)
```

```
Example — [live.gain~] (WithName)  orientation horizontal
  (no patch cords)
```

```
Example #3 — [live.gain~]  orientation vertical, without parameter name nor parameter value
  (no patch cords)
```

### basic

```
Example — [live.gain~] (Stereo)
  fan-in:
    in0 ← [message "interp $1"]    # default interpolation (slew) time is 10 ms
    in0 ← [message "metering $1"]
    in0 ← [*~]
    in1 ← [*~]
  fan-out:
    out0 → [ezdac~]:in0
    out1 → [ezdac~]:in1
```

## See also

`gain~`, `meter~`
