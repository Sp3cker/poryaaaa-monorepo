# array.rotate

_max · Array_

> Rotate the elements in any array object

Move elements forward or backwards in the array. Values will wrap around.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array to rotate |
| in1 | positions to rotate (negative to rotate left) |
| out0 | array out |

## Arguments

- **rotation amount** (`int`) — Initial rotation.
  Number of positions to rotate. Negative to rotate left.

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
- `array(ARG_NAME_0: list)` — Rotate the array a given number of positions.
  Use the second inlet or an initial argument to set the number of positions to rotate. Positive values rotate forwards, pushing values towards the end of the array. Values pushed past the last index will wrap around to the beginning of the array. Negative values rotate backwards, moving values towards the start of the array. A value of 0 will perform no rotation.
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `in1(ARG_NAME_0: int)` — Set the amount to rotate.
  Positive values rotate forwards, pushing values towards the end of the array. Values pushed past the last index will wrap around to the beginning of the array. Negative values rotate backwards, moving values towards the start of the array. A value of 0 will perform no rotation.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `rotate` — seen as: `rotate this sentence for a good time`

## Help patcher examples

### basic

```
Example — [array.rotate 1]
  fan-in:
    in0 ← [message "rotate this sentence for a good time"]
    in1 ← [message "-1"]
    in1 ← [message "1"]
    in1 ← [message "4"]
  fan-out:
    out0 → [print rotate @popup 1]:in0
    out0 → [array.tolist]:in0
```

## See also

`array`, `zl.rot`
