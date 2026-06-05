# array.insert

_max · Array_

> Insert elements into an array object

Elements can be inserted at the beginning, the end, or in the middle, based on the index specified (0-based).

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | position in array to extract |
| in2 | position in array to insert |
| out0 | inserted element of the array |

## Arguments

- **index** (`int`) _(optional)_ — insert
  The index at which to insert elements into incoming arrays. By default, this is 0 (insert before the first element).

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
- `array(ARG_NAME_0: list)` — Insert specified element(s)
  In the middle inlet, an element or multiple elements can be specified for insertion. In the left inlet, the provided elements will be inserted into the incoming array at the specified index, and the resulting array sent to the output.
- `dictionary(ARG_NAME_0: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `in2(index: int)` — Set index
  Set the index at which to insert an element or elements into an incoming array object. Indices begin at 0 for the first element, and elements are inserted previous to the specified index. However, negative indices can also be to insert after elements, counting from the back of the array -- an index of -1 will insert elements at the end of the array, a la array.concat.
- `string(ARG_NAME_0: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `a` — seen as: `a thing or two`
- `df` — seen as: `df df`

## Help patcher examples

### basic

> Positive indices will be inserted before the indexed element (0 = before the first element). Negative indices will be inserted after the indexed element (counting from the back of the array (-1 = after the last element).

```
Example — [array.insert 1]
  fan-in:
    in0 ← [message "5 6 7"]
    in0 ← [message "df df"]
    in1 ← [message "a thing or two"]    # what to insert
    in2 ← [number]    # set insertion index
  fan-out:
    out0 → [print insert @popup 1]:in0
```

## See also

`array`, `array.concat`, `array.index`, `zl.mth`, `zl.nth`
