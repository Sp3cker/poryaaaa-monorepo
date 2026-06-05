# array.shift

_max · Array_

> Remove an element from the beginning of an array

Set the array to shift, then send bangs repeatedly to output the elements of the array one at a time. The remainder of the array is sent out the right outlet, and a bang is sent out the leftmost outlet when the array is empty.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | array in |
| out0 | array is empty bang |
| out1 | shifted element |
| out2 | shifted array out |

## Messages

- `bang` — Trigger output
  Remove the first element from the stored array and output it, sending the remainder out the rightmost outlet.
- `int(value: int)` — Convert an integer to an array.
  Convert an incoming integer to an array, then process as described for the array message.
- `float(value: float)` — Convert a floating-point number to an array.
  Convert an incoming floating-point number to an array, then process as described for the array message.
- `list(list-value: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `anything(list-value: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `array(ARG_NAME_0: list)` — TEXT_HERE
- `clear` — TEXT_HERE
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Attributes

- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `er` — seen as: `er 56`

## Help patcher examples

### basic

```
Example — [array.shift]
  fan-in:
    in0 ← [button]
    in1 ← [message "5 6 7"]    # set base array
    in1 ← [message "er 56"]
  fan-out:
    out0 → [print shift_empty shift_element shift_remaining @popup 1]:in0
    out1 → [print shift_empty shift_element shift_remaining @popup 1]:in1
    out2 → [print shift_empty shift_element shift_remaining @popup 1]:in2
```

## See also

`array`, `array.concat`, `array.pop`, `array.push`, `array.unshift`, `zl.queue`, `zl.stack`
