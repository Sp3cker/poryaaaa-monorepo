# array.pop

_max · Array_

> Remove an element from the end of an array

A bang sent to the left inlet will shift elements from the end of the output array (the remaining array from the right outlet and the popped element is sent via the middle outlet) until the array is empty, at which point a bang is sent from the left outlet. Unlike the JavaScript implementation, the input array is not changed in place.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | bang to pop off last element |
| in1 | array in |
| out0 | array is empty bang |
| out1 | popped element |
| out2 | popped array out |

## Messages

- `bang` — Pop off the last element in an array
  Pop off the last element in an array from the right inlet. The updated array will appear out the right outlet. The popped element will print out the middle outlet.
  If no elements are remaining in the array, a bang will be sent out the leftmost outlet.
- `int(value: int)` — Convert an integer to an array from the right inlet.
  Convert an integer to an array from the right inlet, then process as described for the array message.
- `float(value: float)` — Convert a floating-point to an array from the right inlet.
  Convert an incoming floating-point number to an array from the right inlet, then process as described for the array message.
- `list(list-value: list)` — Convert a list to an array from the right inlet.
  Convert an incoming list to an array from the right inlet, then process as described for the array message.
- `anything(list-value: list)` — Convert a list to an array from the right inlet.
  Convert an incoming list to an array from the right inlet, then process as described for the array message.
- `array(array-value: list)` — Set the internal array from the right inlet.
  Set the internal array to pop from the right inlet.
- `clear` — Reset the internal state of the object.
  All internal arrays will be cleared when the clear message is sent to the left inlet.
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array from the right inlet.
  Wrap an incoming dictionary object in an array from the right inlet, then process as described for the array message.
- `string(string-value: list)` — Wrap a string object in an array from the right inlet.
  Wrap a string object in an array from the right inlet, then process as described for the array message.

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
Example — [array.pop]
  fan-in:
    in0 ← [button]
    in1 ← [message "5 6 7"]    # set base array
    in1 ← [message "er 56"]
  fan-out:
    out0 → [print pop_empty pop_element pop_remaining @popup 1]:in0
    out1 → [print pop_empty pop_element pop_remaining @popup 1]:in1
    out2 → [print pop_empty pop_element pop_remaining @popup 1]:in2
```

## See also

`array`, `array.concat`, `array.push`, `array.shift`, `array.unshift`, `zl.queue`, `zl.stack`
