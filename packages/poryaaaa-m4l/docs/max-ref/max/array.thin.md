# array.thin

_max · Array_

> Remove duplicated entries from an array object

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| out0 | thinned array out |

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
- `array(array-value: list)` — Remove duplicates
  In the left inlet, the contents of the array are filtered for duplicates and a new array with no duplicates is sent to the outlet.
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(string-value: list)` — Wrap a string object in an array.

## Help patcher examples

### basic

```
Example — [array.thin]  thinned array out
  fan-in:
    in0 ← [message "1 2 3.4 3.4 4.5 6 7 3.4 4.5 6"]    # set array
  fan-out:
    out0 → [print thin @popup 1]:in0
```

## See also

`array`, `zl.thin`
