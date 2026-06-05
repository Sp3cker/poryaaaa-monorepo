# live.meter~

_m4l · Live UI Objects, Live MSP Objects_

> Live-style visual peak level indicator

live.meter~ is a simple mono Live-style signal level meter that can be attached to any signal whose level is between -1 and 1 (other signals should be scaled first).

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | float/signal | Input to Meter |
| out0 | float | Amplitude (in dB) |
| out1 | int | Channel for Amplitude Output |

### Port details

**`in0` (Input to Meter):** (signal) Input Between 0-1 to Meter

**`out0` (Amplitude (in dB)):** Peak Value For Each Metering Interval

## Messages

- `int(signal-value: int)` — Display an input value
  Converted to float.
- `float(signal-value: float)` — Display an input value
  When no signal is connected to the live.meter~ object's inlet, a float number will set the meter to react as though a signal with equal amplitude peak-value has been passed to its input. Corresponding LEDs will light up and the peak-level value will be passed out the outlet (0.0 will show silence, 0.0 through 1.0 will light up to any but the overload LED, and anything above 1.0 will light all LEDs including overload).
- `db(db-value: float)` — Display a deciBel input value
  The word db, followed by a floating point number that specficies a deciBel value, will set the live.meter~ object to react as though a signal with that dB peak-value has been passed to its input. Corresponding LEDs will light up and the value will be passed out the outlet.
- `signal` — Display peak amplitude of the signal
  The peak amplitude of the incoming signal is displayed by the on-screen level meter.

## Attributes

- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)
- `@paint` (int)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — Channel for Amplitude Output

### mc

```
Example #1 — [live.meter~]
  fan-in:
    in0 ← [mc.cycle~ @chans 8 @initialvalues 0.2 0.4 0.6 0.8 1. 1.2 1.4 1.6]
```

```
Example #2 — [live.meter~]  live.meter~ auto adapts to the number of channels received
  fan-in:
    in0 ← [mc.cycle~ @chans 4 @initialvalues 0.2 0.4 0.6 0.8]
```

### appearance

```
Example — [live.meter~]
  fan-in:
    in0 ← [attrui @bgcolor]
    in0 ← [attrui @coldcolor]
    in0 ← [attrui @inactivecoldcolor]
    in0 ← [attrui @inactivewarmcolor]
    in0 ← [attrui @overloadcolor]
    in0 ← [attrui @warmcolor]
    in0 ← [attrui @hotcolor]
    in0 ← [dbtoa~] ← [number~]    # Increase Level
```

Attributes demonstrated: `@bgcolor`, `@coldcolor`, `@hotcolor`, `@inactivecoldcolor`, `@inactivewarmcolor`, `@overloadcolor`, `@warmcolor`

### threshold

```
Example #1 — [live.meter~]  deciBel display
  fan-in:
    in0 ← [*~]
    in0 ← [attrui @threshold_db]
  fan-out:
    out0 → [flonum]:in0    # float output
```

```
Example #2 — [live.meter~]  linear display
  fan-in:
    in0 ← [attrui @threshold_linear]
    in0 ← [*~]
  fan-out:
    out0 → [flonum]:in0
```

Attributes demonstrated: `@threshold_db`, `@threshold_linear`

### float/list

```
Example #1 — [live.meter~]
  fan-in:
    in0 ← [message "db $1"]
```

```
Example #2 — [live.meter~]
  fan-in:
    in0 ← [snapshot~ 20] ← [slide~ 10. 100.] ← [average~ 512 rms]
```

### dB/linear

```
Example #1 — [live.meter~]  deciBel display
  fan-in:
    in0 ← [sig~]
  fan-out:
    out0 → [flonum]:in0    # float output
```

```
Example #2 — [live.meter~]  linear display
  fan-in:
    in0 ← [sig~]
  fan-out:
    out0 → [flonum]:in0
```

### range

```
Example #1 — [live.meter~]  extended clip size
  (no patch cords)
```

```
Example #2 — [live.meter~]
  (no patch cords)
```

```
Example #3 — [live.meter~]  -24 <-> +12 dB
  fan-in:
    in0 ← [dbtoa~] ← [number~]
```

```
Example #4 — [live.meter~]
  fan-in:
    in0 ← [dbtoa~] ← [number~]    # By default, live.meter~ range is -70dB <-> +6dB
```

### basic

```
Example #1 — [live.meter~]  horizontal example
  fan-in:
    in0 ← [*~ 0.05]
```

```
Example #2 — [live.meter~]  vertical example
  fan-in:
    in0 ← [ezadc~]
  fan-out:
    out0 → [flonum]:in0    # outputs peak value received in last interval (dB or linear)
```

```
Example #3 — [live.meter~]
  fan-in:
    in0 ← [ezadc~]
  fan-out:
    out0 → [flonum]:in0
```

## See also

`average~`, `gridmeter~`, `meter~`, `scope~`
