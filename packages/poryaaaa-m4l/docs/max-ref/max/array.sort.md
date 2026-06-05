# array.sort

_max · Array_

> Sort the elements of an array object according to a test

Each pair of entries is output, the user compares them and then passes a 1 (left is > right) or a 0 (left is <= right) to determine a final sorted order.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | sort comparison response (0/1) |
| out0 | sorted array out |
| out1 | sort comparison output 1 |
| out2 | sort comparison output 2 |

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
- `array(ARG_NAME_0: list)` — Sort the elements of the array.
  Pairs of array elements will be output from the second and third outlets. Compare them and send the result to the rightmost inlet, sending a 1 if the left is greater than the right, or a 0 if the right is greater than or equal to the left. Finally, the object will output the sorted result. The input array is not modified, but a sorted copy is output.
- `cancel` — TEXT_HERE
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `in1(ARG_NAME_0: int)` — Record the comparison result.
  As you compare each two elements of the array, send the comparison result to the rightmost inlet.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Attributes

- `@simple` (int) — Simple sorting mode
- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `words` — seen as: `words to be sorted`

## Help patcher examples

### basic

> use with string.compare in cmpmode to sort strings

> each pair of entries is output, the user compares them and then passes a 1 (left is > right) or a 0 (left is <= right) to determine a final sorted order.

```
Example #1 — [array.sort]
  fan-in:
    in0 ← [message "words to be sorted"]
    in1 ← [> 0] ← [string.compare @cmpmode 1]
  fan-out:
    out0 → [print alpha @popup 1]:in0
    out1 → [string.compare @cmpmode 1]:in0
    out2 → [string.compare @cmpmode 1]:in1
```

```
Example #2 — [array.sort]
  fan-in:
    in0 ← [message "4 2 8 1 9 3"]    # set array
    in1 ← [< 1.]
  fan-out:
    out0 → [print dec @popup 1]:in0
    out1 → [< 1.]:in0
    out2 → [< 1.]:in1
```

```
Example #3 — [array.sort]
  fan-in:
    in0 ← [message "4 2 8 1 9 3"]
    in1 ← [> 1.]
  fan-out:
    out0 → [print asc @popup 1]:in0
    out1 → [> 1.]:in0
    out2 → [> 1.]:in1
```

## See also

`array`, `array.filter`, `array.map`, `array.reverse`, `coll`, `zl.sort`
