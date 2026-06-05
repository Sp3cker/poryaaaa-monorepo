# mtof

_max · Math_

> Convert a MIDI note number to frequency

Performs MIDI-note-number to frequency conversion. Frequency is reported in as a float in Hertz (Hz).

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | MIDI note number input |
| out0 | frequency output |

## Messages

- `int(MIDI-note-number: int)` — MIDI note number
  Outputs the frequency for the incoming MIDI note value (0 - 127).
- `float(MIDI-note-number: float)` — MIDI note number
  Outputs the frequency for the incoming MIDI note value (0 - 127).
- `list(MIDI note list: list)` — List of MIDI note numbers
  Generates a list of frequency values corresponding to the list of MIDI note numbers.

## Attributes

- `@label` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `map` — seen as: `map 0`, `map 0 0 0 60 69 345`, `map 12 0 127 60 62 310 7 0 x 1 x 2 3 x 4 x 5 x 6`
- `scale` — seen as: `scale 7 128.298 0 276.357 0 545.806 0 669.366 0 784.692 0 967.096 0 2 1`
- `scalename` — seen as: `scalename none`, `scalename pelog_me3`

## Help patcher examples

### keyboard mapping

```
Example #1 — [mtof]
  fan-in:
    in0 ← [flonum] ← [counter 60 71] ← [metro 700 @active 1]
  fan-out:
    out0 → [flonum]:in0
    out0 → [button]:in0    # when note is unmapped, mtof does not output a frequency
```

```
Example #2 — [mtof]
  fan-in:
    in0 ← [message "map 0"]    # reset to default map (every MIDI note is mapped to a scale degree)
    in0 ← [flonum] ← [counter 60 71] ← [metro 700 @active 1]
    in0 ← [message "map 12 0 127 60 62 310 7 0 x 1 x 2 3 x 4 x 5 x 6"]    # define a keyboard map as a list:
    in0 ← [message "map 0 0 0 60 69 345"]    # use map to set mid, ref, and base with one list (see mtof reference for information on mid and ref)
  fan-out:
    out0 → [flonum]:in0
    out0 → [button]:in0
```

### scala

> from the scala archive: pelog_me3 Gamelan Kyahi Pangasih (kraton Solo). 1/1=286 Hz

```
Example #1 — [mtof]
  fan-in:
    in0 ← [flonum] ← [counter 60 67] ← [metro 700 @active 1]
  fan-out:
    out0 → [flonum]:in0
```

```
Example #2 — [mtof]
  fan-in:
    in0 ← [message "scalename none"]    # use equal temperament scale:
    in0 ← [flonum] ← [counter 60 67] ← [metro 700 @active 1]
    in0 ← [message "scale 7 128.298 0 276.357 0 545.806 0 669.366 0 784.692 0 967.096 0 2 1"]    # define a scale as a list:
    in0 ← [message "scalename pelog_me3"]    # load a scale from scala archive:
  fan-out:
    out0 → [flonum]:in0
```

### lists

```
Example — [mtof]
  fan-in:
    in0 ← [message "72 80 64 38"]
    in0 ← [message "36 48 60 72"]
  fan-out:
    out0 → [message "65.406391 130.812783 261.625565 523.251131"]:in1
```

### base

```
Example #1 — [mtof @base 438]
  fan-in:
    in0 ← [flonum] ← [kslider] ← [loadmess 69]
  fan-out:
    out0 → [flonum]:in0
```

```
Example #2 — [mtof @base 442]
  fan-in:
    in0 ← [flonum] ← [kslider] ← [loadmess 69]
  fan-out:
    out0 → [flonum]:in0
```

### basic

```
Example — [mtof]
  fan-in:
    in0 ← [flonum] ← [kslider]    # note numbers are converted into frequency values
    in0 ← [message "60.25"]    # MIDI note 60 plus 25 cents
  fan-out:
    out0 → [flonum]:in0    # frequency in Hertz (Hz)
```

## See also

`expr`, `ftom`, `mtof~`
