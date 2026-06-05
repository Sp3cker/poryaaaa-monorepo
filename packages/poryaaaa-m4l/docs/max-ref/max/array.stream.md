# array.stream

_max · Array_

> Make an array of a certain size

array.stream accepts a number in the right inlet which specifies the length of the output array. Following the receipt of this number, the object will collect this number of elements input through the left inlet. After the array-length is complete, and with each subsequent input, the array will be output from the left outlet. A 1 or a 0 will be output from the right outlet depending on whether the array-length has been reached or not. A 1 signifies that the array-length has been reached and that the object is now collecting the stream. Use the clear message to reset the array.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | length in |
| out0 | streamed array out |
| out1 | 1 if 0 elements are defined |

## Arguments

- **array-length** (`int`) _(optional)_ — Desired array length

## Messages

- `bang` — Trigger output
  Reprocess previously received array and trigger output.
- `int(array-length: int)` — In the right inlet, set the desired output array length. In the left inlet, add an int element to the current output array.
  If the output array length is negative, elements are added to the front of the output array, otherwise they are added to the end.
- `float(array-length: int/float)` — In the right inlet, set the desired output array length. In the left inlet, add a float element to the current output array.
  If the output array length is negative, elements are added to the front of the output array, otherwise they are added to the end.
- `list(ARG_NAME_0: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `anything(ARG_NAME_0: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `array(ARG_NAME_0: list)` — Stream the contents of an array
  Add a list of elements sequentially (one element at a time) to the output array.
- `clear` — Clear the output array.
  Resets the streaming process.
- `dictionary(ARG_NAME_0: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(ARG_NAME_0: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `df` — seen as: `df df`

## Help patcher examples

### basic

> a negative length adds elements to the front of the array. a positive length adds them to the back.

```
Example — [array.stream 5]
  fan-in:
    in0 ← [message "5 6 7"]
    in0 ← [message "clear"]
    in0 ← [message "df df"]
    in0 ← [message "anything b c def jam"]
    in1 ← [message "5"]
    in1 ← [message "-5"]
  fan-out:
    out0 → [print stream @popup 1]:in0
    out1 → [toggle]:in0    # array has reached requested length flag (0/1)
```

## See also

`array`, `array.group`, `array.tuplewise`, `zl.iter`, `zl.queue`
