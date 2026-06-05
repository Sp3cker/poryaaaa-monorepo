# array.group

_max · Array_

> Output an array when it reaches a certain size

Outputs an array after the number of elements specified by the group size are received. Incoming arrays larger than the group size are split into multiple output arrays.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | number of elements to group (currently 0) |
| out0 | grouped array |

## Arguments

- **initial-size** (`int`) _(optional)_ — Size of grouped array
  Specifies a number of elements from incoming arrays to be grouped and output.

## Messages

- `bang` — Trigger output
  Immediately outputs current array (even if it hasn't reached the maximum size).
- `int(ARG_NAME_0: int)` — Convert an integer to an array.
  Convert an incoming integer to an array, then process as described for the array message.
- `float(ARG_NAME_0: float)` — Convert a floating-point number to an array.
  Convert an incoming floating-point number to an array, then process as described for the array message.
- `list(ARG_NAME_0: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `anything(ARG_NAME_0: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `array(ARG_NAME_0: list)` — Store and output elements
  Append elements to the output array. When the output array reaches the size specified as an argument (or as an int in the right inlet), it will be sent from the outlet. Upon output, the output array is reset to length 0.
- `clear` — Clear output array
  Resets the length of the output array to 0
- `dictionary(ARG_NAME_0: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `in1(array-size: int)` — Size of grouped array
  Specifies a number of elements from incoming arrays to be grouped and output.
- `string(ARG_NAME_0: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `bbb` — seen as: `bbb`
- `ccc` — seen as: `ccc`

## Help patcher examples

### basic

```
Example — [array.group 4]
  fan-in:
    in0 ← [message "111"]
    in0 ← [message "bbb"]
    in0 ← [message "222"]
    in0 ← [message "ccc"]
    in0 ← [button]
    in0 ← [message "1 b 2 d 3 f 4 h 5 j 6 l"]
    in1 ← [message "3"]
    in1 ← [message "6"]
    in1 ← [message "2"]
    in1 ← [message "5"]    # use 'bang' to output an a grouped array-in-progress
  fan-out:
    out0 → [print group @popup 1]:in0
```

## See also

`array`, `array.iter`, `array.stream`, `array.tuplewise`, `zl.group`, `zl.iter`
