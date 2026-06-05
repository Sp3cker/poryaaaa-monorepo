# array.tuplewise

_max · Array_

> Make an array of a certain size (counting iterations)

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | length in |
| out0 | streamed array out |
| out1 | 0-based output index since the last clear message was received |

## Arguments

- **array-length** (`int`) _(optional)_ — Desired array length

## Messages

- `bang` — Trigger output
  Reprocess previously received arrays and trigger output
- `int(value: int)` — Convert an integer to an array.
  Convert an incoming floating-point number to an array, then process as described for the array message.
- `float(value: float)` — Convert a floating-point number to an array.
  Convert an incoming floating-point number to an array, then process as described for the array message.
- `list(list-value: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `anything(array-value: list)` — Add a list of elements sequentially (one element at a time) to the output array.
- `array(array-value: list)` — Output array legnth
  In the right inlet, a number specifies the length of the output array. Following the receipt of this number, the object will collect this number of elements input through the left inlet. After the array-length is complete, and with each subsequent input, the array will be output from the left outlet. An index (0-based) will be output from the right outlet along with each array sent from the left outlet, counting upward since the creation of the object, or since the last clear was received.
- `clear` — Clear the output array.
  Use the clear message to reset the index to 0.
- `dictionary(dictionary-value: list)` — Add a dictionary object to the output array.
- `string(string-value: list)` — Add a string object to the output array.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `df` — seen as: `df df`

## Help patcher examples

### basic

> a negative length adds elements to the front of the array. a positive length adds them to the back.

```
Example — [array.tuplewise 2]  streamed array out
  fan-in:
    in0 ← [message "5 6 7"]
    in0 ← [message "clear"]
    in0 ← [message "df df"]    # set array
    in0 ← [message "anything b c def jam"]
    in1 ← [message "2"]    # set length of the output array
    in1 ← [message "-2"]
  fan-out:
    out0 → [print tuplewise index @popup 1]:in0
    out1 → [number]:in0    # index of current array output since the last clear (0-based)
    out1 → [print tuplewise index @popup 1]:in1
```

## See also

`array`, `array.stream`, `zl.iter`, `zl.queue`
