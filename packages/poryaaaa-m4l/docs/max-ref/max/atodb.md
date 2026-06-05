# atodb

_max · Math_

> Convert a linear value to decibels

Converts any given linear value to its corresponding decibel value.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | float | (float) Amplitude Scalar |
| out0 | float | (float) Gain/Attenuation dB |

## Messages

- `bang` — Repeat the most recent calculation
  The most recently calculated decibel value is sent out the outlet.
- `int(linear-amplitude: int)` — Convert linear to decibel amplitude
  A linear amplitude value. The corresponding gain/attenuation in decibels is sent out the outlet.
- `float(linear-amplitude: float)` — Convert linear to decibel amplitude
  A linear amplitude value. The corresponding gain/attenuation in decibels is sent out the outlet.
- `list(amplitude-list: list)` — Convert a list of linear amplitude values
  A list of linear amplitude values. Corresponding gain/attenuation values in decibels for each list item are sent out the outlet.
- `set(linear-amplitude: float)` — Set amplitude value with no output
  The message set followed by a linear amplitude value will set the next value to be calculated into decibels without sending anything out the outlet.

## Help patcher examples

### basic

```
Example #1 — [atodb]
  fan-in:
    in0 ← [multislider]    # click-drag in multislider to generate a list
  fan-out:
    out0 → [message "-2.896671 -3.652443 -7.894221 -6.151217"]:in1
```

```
Example #2 — [atodb]
  fan-in:
    in0 ← [flonum]    # linear amplitude
  fan-out:
    out0 → [live.gain~]:in0
```

## See also

`expr`, `atodb~`, `dbtoa`, `dbtoa~`
