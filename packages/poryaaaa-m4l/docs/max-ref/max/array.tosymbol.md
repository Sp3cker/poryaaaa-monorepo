# array.tosymbol

_max · Array_

> Convert an array object to a symbol

This object is particularly useful for visualization of an array object's contents.

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
- `array(ARG_NAME_0: list)` — Convert an array to a symbol.
  A single symbol is output containing a serialized version of the entire array.
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Help patcher examples

### basic

```
Example — [array.tosymbol]  convert an array to a symbol
  fan-in:
    in0 ← [array.concat]
  fan-out:
    out0 → [message "set $1"]:in0    # very useful for in-patcher visualization!
    out0 → [print tosymbol @popup 1]:in0
```

## See also

`array`, `array.tostring`, `dict.view`, `tosymbol`
