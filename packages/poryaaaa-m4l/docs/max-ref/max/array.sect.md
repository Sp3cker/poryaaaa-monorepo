# array.sect

_max · Array_

> Return the elements of an array object which intersect with another array object

Intersection means that the elements are identical or equivalent (in the case of array, dictionary and string objects).

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
- `array(ARG_NAME_0: list)` — Compute the intersection of two arrays
  In the left inlet, an incoming array will be compared with the right-hand array. The intersecting elements will be appended to a new array, which will be output. In the right inlet, set the right-hand array. Elements do not need to appear in the same order.
- `dictionary(ARG_NAME_0: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(ARG_NAME_0: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `hating` — seen as: `hating the number 5 with sunshine`
- `love` — seen as: `love to eat 5 jellybeans and sunshine`
- `sunshine` — seen as: `sunshine with spinach`

## Help patcher examples

### basic

```
Example — [array.sect]
  fan-in:
    in0 ← [message "sunshine with spinach"]
    in0 ← [message "love to eat 5 jellybeans and sunshine"]    # set left-hand array and trigger output
    in1 ← [message "hating the number 5 with sunshine"]
  fan-out:
    out0 → [print sect @popup 1]:in0
```

## See also

`array`, `array.union`, `array.unique`, `zl.sect`, `zl.union`, `zl.unique`
