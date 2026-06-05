# ftom

_max · Math_

> Convert frequency to a MIDI note number

ftom converts frequency to MIDI note numbers

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Frequency In |
| out0 | MIDI Note Number Out |

## Arguments

- **format** (`float`) _(optional)_ — Float output
  If a float value is present, the ftom object outputs floating-point values with fractional parts. By default, it outputs whole number values.

## Messages

- `int(frequency: int)` — Convert frequency to MIDI note number
  Outputs the MIDI note number (from 0 to 127) corresponding to the frequency
- `float(frequency: float)` — Convert frequency to MIDI note number
  Outputs the MIDI note number (from 0 to 127) corresponding to the frequency
- `list(list of frequencies: list)` — Convert frequencies to MIDI note numbers
  Outputs a list of MIDI note numbers (from 0 to 127) corresponding to the frequencies in the input list

## Attributes

- `@label` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `scale` — seen as: `scale 12 76.049 0 193.15686 0 310.26471 0 5 4 503.42157 0 579.47057 0 696.57843 0 25 16 889.73529 0 1006.84314 0 1082.89214 0 2 1`
- `scalename` — seen as: `scalename ji_30`, `scalename none`

## Help patcher examples

### scala

> from scala archive: ji_30 11-limit rational interpretation of 30-tET

```
Example #1 — [ftom @round 0]
  fan-in:
    in0 ← [message "scale 12 76.049 0 193.15686 0 310.26471 0 5 4 503.42157 0 579.47057 0 696.57843 0 25 16 889.73529 0 1006.84314 0 1082.89214 0 2 1"]    # define a scale as a list:
    in0 ← [message "scalename none"]    # use equal temperament
    in0 ← [message "scalename ji_30"]    # load a scale from the scala archive
    in0 ← [flonum] ← [* 20] ← [slider]    # frequency in
  fan-out:
    out0 → [flonum]:in0    # MIDI note with fractional part
```

```
Example #2 — [ftom @round 1]
  fan-in:
    in0 ← [message "scale 12 76.049 0 193.15686 0 310.26471 0 5 4 503.42157 0 579.47057 0 696.57843 0 25 16 889.73529 0 1006.84314 0 1082.89214 0 2 1"]    # define a scale as a list:
    in0 ← [message "scalename none"]    # use equal temperament
    in0 ← [message "scalename ji_30"]    # load a scale from the scala archive
    in0 ← [flonum] ← [* 20] ← [slider]    # frequency in
  fan-out:
    out0 → [flonum]:in0    # MIDI note rounded to nearest integer
```

### lists

```
Example — [ftom]
  fan-in:
    in0 ← [message "311.126984 830.609375 349.228241 164.813782"]
    in0 ← [message "220 330 440 880"]
  fan-out:
    out0 → [message "63 80 65 52"]:in1
```

### base

```
Example #1 — [ftom @base 438]
  fan-in:
    in0 ← [mtof @base 438] ← [flonum] ← [kslider]
  fan-out:
    out0 → [flonum]:in0
```

```
Example #2 — [ftom @base 442]
  fan-in:
    in0 ← [mtof @base 442] ← [flonum] ← [kslider]
  fan-out:
    out0 → [flonum]:in0
```

### basic

```
Example — [ftom]
  fan-in:
    in0 ← [message "440"]
    in0 ← [message "123.45"]    # set frequency values
    in0 ← [flonum]
  fan-out:
    out0 → [kslider]:in0
    out0 → [flonum]:in0
```

## See also

`expr`, `ftom~`, `mtof`
