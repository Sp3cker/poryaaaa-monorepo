# array.filter

_max · Array_

> Output elements of an array matching a condition

If the condition set for the object is '0' the element is excluded in the filtered array output. In the case the condition is '1' it is included in the array.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | filter inclusion response (0/1) |
| out0 | filtered array out |
| out1 | filter inclusion output |
| out2 | element index |

## Messages

- `bang` — Trigger output
  Reprocess previously received arrays and trigger output.
- `int(ARG_NAME_0: int)` — Convert an integer to an array.
  Convert an incoming integer to an array, then process as described for the array message.
- `float(ARG_NAME_0: float)` — Convert a floating-point number to an array.
  Convert an incoming floating-point number to an array, then process as described for the array message.
- `list(ARG_NAME_0: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `anything(ARG_NAME_0: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `array(ARG_NAME_0: list)` — Filter and sequentially output elements
  In the left inlet, sending an array will trigger output of the filtered array, element value and the index element. Entries are output sequentially.
- `cancel` — TEXT_HERE
- `dictionary(ARG_NAME_0: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `in1(ARG_NAME_0: int)` — TEXT_HERE
- `string(ARG_NAME_0: )` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `er` — seen as: `er 56`
- `kling` — seen as: `kling klang`

## Help patcher examples

### basic

> send a 1 into the right inlet to indicate that the entry passes the test, pass 0 to indicate failure.

```
Example — [array.filter]  filtered array out / element value
  fan-in:
    in0 ← [message "er 56"]
    in0 ← [message "kling klang"]
    in0 ← [message "5 6 7"]
    in1 ← [t 1]
    in1 ← [t 0] ← [typeroute~] ← [array.filter]    # filtered array out / element value
  fan-out:
    out0 → [print filter @popup 1]:in0
    out1 → [typeroute~]:in0
    out2 → [number]:in0    # element index
```

## See also

`array`, `array.map`, `array.reduce`, `zl.filter`
