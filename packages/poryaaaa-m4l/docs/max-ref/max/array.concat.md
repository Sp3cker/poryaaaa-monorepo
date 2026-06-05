# array.concat

_max · Array_

> Concatenate two array objects

The data in the array received in the right inlet will be appended to the data in the array received in the left inlet and a new array will be output. The original array objects are not modified.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | left array in will be concatenated with the right array |
| in1 | set right array |
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
- `array(array-value: list)` — Concatenate an array.
  In the right inlet, the array is stored and no output is generated. In the left inlet, the contents of the array are concatenated with the contents of any array received in the right inlet, and a new array is sent to the outlet.
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `df` — seen as: `df df`
- `er` — seen as: `er 56`
- `har` — seen as: `har har`

## Help patcher examples

### basic

```
Example — [array.concat]
  fan-in:
    in0 ← [message "har har"]
    in0 ← [button]
    in0 ← [message "df df"]    # set left array and immediately output
    in1 ← [message "5 6 7"]    # set right array
    in1 ← [message "er 56"]
  fan-out:
    out0 → [print concat @popup 1]:in0    # this is a named array object
```

## See also

`array`, `array.push`, `array.unshift`, `append`, `prepend`
