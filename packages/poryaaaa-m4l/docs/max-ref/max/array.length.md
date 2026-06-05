# array.length

_max · Array_

> Determine the length of an array object

Length is determined by sending a message to the left inlet.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| out0 | length out |

## Messages

- `bang` — Trigger output
  Reprocess previously received arrays and trigger output
- `int(value: int)` — Convert an integer to an array.
  Convert an incoming floating-point number to an array, then process as described for the array message.
- `float(value: float)` — Convert a floating-point number to an array.
  Convert an incoming floating-point number to an array, then process as described for the array message.
- `list(list-value: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `anything(array-value: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `array(dictionary-value: list)` — Output array legnth
  Output the length of an array when and array is sent to the left inlet.
- `clear(clear: list)` — Clear array
  Clear the recently stored array from the array.legnth object.
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `df` — seen as: `df df`

## Help patcher examples

### basic

```
Example — [array.length]  length out
  fan-in:
    in0 ← [message "df df"]    # set array
    in0 ← [message "5 6 7"]
  fan-out:
    out0 → [print array @popup 1]:in0
```

## See also

`array`, `zl.len`
