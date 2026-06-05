# array.random

_max · Array_

> Generate a random array of a specified length

Creates a new array object of a specified length, pre-filled with random numbers.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | range in (array) |
| in1 | new array length |
| out0 | filled array |

## Arguments

- **length** (`int`) — Array length
  The length of the generated array.
- **initial-range** (`list`) _(optional)_ — Initial range
  The range of random values to generate. If both values are integers, random integers will be generated, otherwise random floating-point numbers will be generated.

## Messages

- `bang` — Trigger output
  Reprocess previously received arrays and trigger output.
- `int(value: int)` — Convert an integer to an array.
  Convert an incoming integer to an array, then process as described for the array message.
- `float(value: float)` — Convert a floating-point number to an array.
  Convert an incoming floating-point number to an array, then process as described for the array message.
- `list(list-value: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `anything(list-value: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `array(ARG_NAME_0: list)` — Generate a random array
  A new array object will be created at the length specified by the object's first argument, or by a number received in the rightmost inlet. The object will then be filled with random values, as specified by the range and seed.
  An incoming array will be used as a new range. As such, only the first two integer and floating-point numbers will be used -- all other data will be interpreted as 0.
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `in1(length: int)` — Array length
  The length of the generated array.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Attributes

- `@mode` (int) — Fill Mode
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### basic

> generate a five element integer array with elements randomly chosen from 0-10 (but not including 10)

> generate a five element float array with elements randomly chosen from 5-10. (inclusive)

```
Example #1 — [array.random 10]
  fan-in:
    in0 ← [message "0. 1."]
    in0 ← [message "1 10"]    # new arrays update the range and regenerate
    in1 ← [number] ← [loadmess 5]    # change output array length
  fan-out:
    out0 → [print random @popup 1]:in0
```

```
Example #2 — [array.random 5 0 10]
  fan-in:
    in0 ← [button]
    in1 ← [number] ← [loadmess 5]    # change output array length
  fan-out:
    out0 → [print random_int @popup 1]:in0
```

```
Example #3 — [array.random 5 5 10.]
  fan-in:
    in0 ← [button]
    in1 ← [number] ← [loadmess 5]    # change output array length
  fan-out:
    out0 → [print random_flt @popup 1]:in0
```

## See also

`array`, `array.fill`, `random`, `urn`
