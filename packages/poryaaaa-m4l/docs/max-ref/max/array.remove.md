# array.remove

_max · Array_

> Remove a range of elements from an array object

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | remove start index (0-based) |
| in2 | remove end index (0-based) |
| out0 | array out |

## Arguments

- **index-range** (`list`) _(optional)_ — Index-range
  The index range for removal includes the start index value and the end index value (0-based).

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
- `array(array-value: list)` — Remove a range of elements
  In the middle inlet, set the start index for removal. In the right inlet, set the end index for removal . The index range for removal includes the start index value and the end index value (0-based). In the left inlet, setting an array will trigger output of the array with the specified range of elements removed.
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `in1(ARG_NAME_0: int)` — TEXT_HERE
- `in2(ARG_NAME_0: int)` — TEXT_HERE
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `a` — seen as: `a b c d e`

## Help patcher examples

### basic

```
Example — [array.remove 2 3]  array with removed range of elements
  fan-in:
    in0 ← [message "a b c d e"]
    in1 ← [number]
    in2 ← [number]
  fan-out:
    out0 → [print remove @popup 1]:in0
```

## See also

`array`, `array.slice`, `array.subarray`, `zl.ecils`, `zl.slice`
