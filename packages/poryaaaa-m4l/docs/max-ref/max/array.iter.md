# array.iter

_max · Array_

> Iterate every element of an array object

Outputs each element in an array object sequentially, as a series of messages.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | array in |
| out0 | iterated array members |

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
- `array(ARG_NAME_0: list)` — Iterate elements
  Output each element of the array sequentially, as a series of individual messages.
- `dictionary(ARG_NAME_0: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(ARG_NAME_0: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in1` — array in

### basic

```
Example — [array.iter]
  fan-in:
    in0 ← [array] ← [message "1 2 3 4"]
    in0 ← [message "anything b cow dung"]
  fan-out:
    out0 → [print iter @popup 1]:in0
```

## See also

`array`, `array.foreach`, `array.stream`, `array.tuplewise`, `zl.iter`
