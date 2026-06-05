# array.subarray

_max · Array_

> Output a range of elements of an array object as a new array object

array.subarray is similar to array.slice, but doesn't conform to the JavaScript Array.slice() behavior.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | subarray indices (0-based) |
| out0 | subarray out |

## Arguments

- **subarray-indexes** (`list`) _(optional)_ — Subarray indexes
  Set the range to extract when an array is received in the leftmost inlet. Negative values index from the end of the array, and a single value on its own implicitly stretches to the end of the array. If the end of the range is before the start, then the elements of the range will be reversed.

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
- `array(ARG_NAME_0: list)` — Output a subarray of this array.
  Set the range of values to output using the second inlet or the object arguments.
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `a` — seen as: `a b c d e`

## Help patcher examples

### basic

```
Example — [array.subarray 1 2]
  fan-in:
    in0 ← [message "a b c d e"]
    in1 ← [message "2 4"]    # start and end indices to extract
    in1 ← [message "2 -1"]    # start index to extract (to end)
    in1 ← [message "4 2"]    # also works backward
    in1 ← [message "-3 -2"]    # negative indices supported (in this case, 3rd element from the end to 2nd element from the end)
    in1 ← [message "2"]    # start index to extract (to end)
  fan-out:
    out0 → [print subarray @popup 1]:in0
```

## See also

`array`, `array.slice`, `zl.ecils`, `zl.slice`
