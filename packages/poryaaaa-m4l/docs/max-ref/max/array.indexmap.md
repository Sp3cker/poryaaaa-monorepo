# array.indexmap

_max · Array_

> Reorder the elements of an array object based on an indexed map

Use an array of integers (an index map) to reorder the elements of another array.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | indices list |
| out0 | array out |

## Arguments

- **index-map** (`list`) _(optional)_ — initial index map

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
- `array(ARG_NAME_0: list)` — Apply the given index map to this array.
  Outputs the given array, but with the elements reordered according to the given index map. Indexes start at 0, and duplicates are allowed. For example, applying the index map [2, 1, 1, 0] to the array [A, B, C] would result in the array [C, B, B, A].
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `zero` — seen as: `zero one two three four five six seven`

## Help patcher examples

### basic

```
Example — [array.indexmap 1 3 5]  array out
  fan-in:
    in0 ← [message "zero one two three four five six seven"]
    in1 ← [message "5 4 3 2"]
    in1 ← [message "1 3 5 7"]
    in1 ← [message "0 2 4"]
    in1 ← [message "2 2 2 2"]
  fan-out:
    out0 → [print indexmap @popup 1]:in0
```

## See also

`array`, `zl.indexmap`
