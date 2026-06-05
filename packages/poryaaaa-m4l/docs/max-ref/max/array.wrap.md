# array.wrap

_max · Array_

> Wrap an array inside of an array

Among other things, you can use wrapped arrays in conjunction with array.concat to create multidimensional arrays.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | array in |
| out0 | wrapped array out |

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
- `array(ARG_NAME_0: list)` — Wrap an array
  Wrap an array in an array, then send it to the outlet.
- `dictionary(ARG_NAME_0: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(ARG_NAME_0: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in1` — array in

### basic

```
Example #1 — [array.wrap]  make a multidimensional array
  fan-in:
    in0 ← [message "5 6"]
  fan-out:
    out0 → [array.concat]:in1
```

```
Example #2 — [array.wrap]
  fan-in:
    in0 ← [message "1 2"]
  fan-out:
    out0 → [array.concat]:in0
```

```
Example #3 — [array.wrap]
  fan-in:
    in0 ← [array] ← [message "5 6 7"]    # send an array to be wrapped inside of an array
    in0 ← [message "3 4 nudge 5"]    # converted to an array, then wrap
  fan-out:
    out0 → [print wrap @popup 1]:in0
```

## See also

`array`, `array.push`, `array.unshift`
