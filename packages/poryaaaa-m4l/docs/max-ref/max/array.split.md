# array.split

_max · Array_

> Split an array object into two new array objects at a specified index

The position set as an argument or

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | position in array to split |
| out0 | array &lt; of split point |
| out1 | array &gt;= of split point |

## Arguments

- **position** (`int`) _(optional)_ — Element Position
  Element Index position of the array where the split will occur.

## Messages

- `bang` — Trigger output
  Reprocess previously received arrays and trigger output.
- `int(ARG_NAME_0: int)` — Convert an integer to an array.
  Convert an incoming integer to an array from the left inlet, then process as described for the array message.
  Set the index position where to split the array from the left inlet.
- `float(ARG_NAME_0: float)` — Convert a floating-point number to an array.
  Convert an incoming floating-point number to an array, then process as described for the array message.
- `list(ARG_NAME_0: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `anything(ARG_NAME_0: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `array(ARG_NAME_0: list)` — Split array into two separate arrays at a determined index position
  In the left inlet,an array will trigger output of two separate arrays separated by a determined index position. Elements to the left of the position will be sent out the left outlet. Elements to the right of the position will be sent out the right outlet.
- `dictionary(ARG_NAME_0: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `in1(ARG_NAME_0: int)` — TEXT_HERE
- `string(ARG_NAME_0: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Help patcher examples

### basic

```
Example — [array.split 4]
  fan-in:
    in0 ← [message "4 8 12 16 20 24 28"]
    in1 ← [number]
  fan-out:
    out0 → [print split_l split_r @popup 1]:in0
    out1 → [print split_l split_r @popup 1]:in1
```

## See also

`array`, `array.slice`, `array.subarray`, `zl.ecils`, `zl.slice`
