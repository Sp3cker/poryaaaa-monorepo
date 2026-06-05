# array.replace

__

> Replace elements in an array

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | replacement array |
| in1 | base array |
| in2 | replacement start index (0-based, -1 = bypass) |
| out0 | array out |

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
- `array(array-value: list)` — Replace elements
  In the right inlet, set the start index at which to replace elements (0-based). In the middle inlet, set an array to operate upon (the base array). In the left inlet, an array will trigger output of the base array with elements replaced by the elements of the incoming array (the replacement array), beginning at the start index. An index of -2 will ignore the replacement array and output a copy of the base array. An index of -1 will ignore the base array and output a copy of the replacement array.
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `in2(ARG_NAME_0: int)` — TEXT_HERE
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `a` — seen as: `a b c d e f`

## Help patcher examples

### basic

```
Example — [array.replace 2]  array with replaced range of elements
  fan-in:
    in0 ← [message "10 20 30"]    # multiple entries can be replaced at once
    in0 ← [message "a b c d e f"]    # the base array can be overwritten or extended
    in0 ← [number]
    in1 ← [message "1 2 3 4 5"]
    in2 ← [number]
  fan-out:
    out0 → [print replace @popup 1]:in0
```

## See also

`array`, `array.foreach`, `array.map`, `array.remove`
