# array.foreach

_max · Array_

> Iterate the elements of an array

Iterate the elements of an array, with index and a done-iterating bang.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | array in |
| out0 | bang when finished |
| out1 | element value |
| out2 | element index |

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
- `array(ARG_NAME_0: list)` — Iterate the elements of an array.
  The array elements are sent out in order, along with the index of each element. A bang is output in the end, to indicate the end of the list.
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `one` — seen as: `one two three four five`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in1` — array in

### basic

```
Example — [array.foreach]  element index / element value
  fan-in:
    in0 ← [message "one two three four five"]    # converted to an array
    in0 ← [button]    # repeat previous input
    in0 ← [dict foreach] ← [message "set greeting hello, set age 12, set height 120, bang"]    # converted to an array
  fan-out:
    out0 → [print bang_when_done value index]:in0
    out0 → [button]:in0
    out1 → [print bang_when_done value index]:in1
    out2 → [print bang_when_done value index]:in2
```

## See also

`array`, `array.iter`, `array.stream`, `array.tuplewise`, `zl.iter`
