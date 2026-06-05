# array.union

_max · Array_

> Combine two arrays into a new array object containing non-duplicate entries of both arrays

Combine two arrays into a new array object containing non-duplicate entries of both arrays. For instance, the arrays [ 0, 1, 1, 2, 8 ] and [ 2, 2, 3, 3, 5 ] would become a new array [ 0, 1, 2, 8, 3, 5 ].

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | array in |
| out0 | array out |

## Messages

- `bang` — Trigger output
  Reprocess previously received array and trigger output.
- `int(ARG_NAME_0: int)` — Convert an integer to an array.
  Convert an incoming integer to an array, then process as described for the array message.
- `float(ARG_NAME_0: float)` — Convert a floating-point number to an array.
  Convert an incoming floating-point number to an array, then process as described for the array message.
- `list(ARG_NAME_0: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `anything(ARG_NAME_0: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `array(ARG_NAME_0: list)` — Compute the union of two arrays
  In the left inlet, an incoming array will be compared with the right-hand array. A new array will be generated containing non-duplicate entries of both arrays, which will be output. In the right inlet, set the right-hand array.
- `dictionary(ARG_NAME_0: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(ARG_NAME_0: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `hating` — seen as: `hating the number 5 with sunshine`
- `love` — seen as: `love to eat 5 jellybeans and sunshine`

## Help patcher examples

### basic

```
Example — [array.union]
  fan-in:
    in0 ← [message "0 1 1 2 8"]
    in0 ← [message "love to eat 5 jellybeans and sunshine"]
    in1 ← [message "2 2 3 3 5"]
    in1 ← [message "hating the number 5 with sunshine"]
  fan-out:
    out0 → [print union @popup 1]:in0
```

## See also

`array`, `array.sect`, `array.unique`, `zl.sect`, `zl.union`, `zl.unique`
