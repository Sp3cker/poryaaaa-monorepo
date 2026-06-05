# array.tostring

_max · Array_

> Convert an array object to a string object

The string object contains a serialized representation of the array's contents.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| out0 | list out |

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
- `array(ARG_NAME_0: list)` — Convert an array to a string object.
  A string object is output containing a serialized version of the entire array.
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Help patcher examples

### basic

```
Example — [array.tostring]  convert an array to a string object
  fan-in:
    in0 ← [array.concat]
  fan-out:
    out0 → [print tostring @popup 1]:in0
```

## See also

`array`, `array.tosymbol`, `dict.view`, `tosymbol`
