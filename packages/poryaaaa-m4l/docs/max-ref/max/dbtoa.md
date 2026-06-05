# dbtoa

_max · Math_

> Convert decibels to a linear value

Converts a decibel value to its corresponding linear value.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | float | (float) Gain/Attenuation dB |
| out0 | float | (float) Amplitude Scalar |

## Messages

- `bang` — Repeat the most recent calculation
  The most recently stored linear amplitude value is sent out the outlet.
- `int(dB-gain: int)` — Convert to linear amplitude
  Converts a gain/attenuation in deciBels into its corresponding linear amplitude.
- `float(dB-gain: float)` — Convert to linear amplitude
  Converts a gain/attenuation in deciBels into its corresponding linear amplitude.
- `list(gain-values: list)` — Convert a list of value
  Converts a list of gain/attenuation values in deciBels into their corresponding linear amplitude values.
- `set(dB-gain: number)` — Convert without output
  Converts a gain/attenuation in deciBels into its corresponding linear amplitude, but no output is sent.

## Help patcher examples

### basic

```
Example — [dbtoa]
  fan-in:
    in0 ← [flonum]    # dB
  fan-out:
    out0 → [flonum]:in0    # linear
```

## See also

`expr`, `atodb`, `atodb~`, `dbtoa~`
