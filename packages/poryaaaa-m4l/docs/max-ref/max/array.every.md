# array.every

_max · Array_

> Tests all elements in the array

Tests whether all elements in the array pass the provided test. While testing, return the value of the test to the right inlet.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | test response (0/1) |
| out0 | every element of input array passes test (0/1) |
| out1 | test output |
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
- `array(ARG_NAME_0: list)` — Tests whether all elements in the array pass the provided test.
  When an array is received in the left inlet, array.every will send each element of the array through the middle outlet, one at a time. If the element passes the test, return a 1 to the right inlet, otherwise return a 0. After receiving a test result for each element, array.every will output an overall result.
- `cancel` — TEXT_HERE
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `in1(ARG_NAME_0: int)` — TEXT_HERE
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `er` — seen as: `er 56`

## Help patcher examples

### examples

> Is every element equal to its index?

```
Example #1 — [array.every]  Is every element greater than 5?
  fan-in:
    in0 ← [message "0"]
    in0 ← [message "0 1 2"]
    in0 ← [message "8 9 10"]
    in1 ← [==]
  fan-out:
    out0 → [print every_example_2 @popup 1]:in0
    out1 → [==]:in0
    out2 → [==]:in1
```

```
Example #2 — [array.every]
  fan-in:
    in0 ← [message "1 2 3"]
    in0 ← [message "8 9 10"]
    in0 ← [message "5 6 7"]
    in1 ← [> 5] ← [array.every]
  fan-out:
    out0 → [print every_example_1 @popup 1]:in0
    out1 → [> 5]:in0
```

### basic

> Test the incoming array for some property. In this case, whether all entries are ints or floats. Entries are output sequentially; send a 1 into the right inlet to indicate that the entry passes the test, pass 0 to indicate failure.

```
Example — [array.every]
  fan-in:
    in0 ← [message "er 56"]
    in0 ← [message "3. 4 5.6"]
    in0 ← [message "5 6 7"]
    in1 ← [t 1]
    in1 ← [t 0] ← [typeroute~] ← [array.every]
  fan-out:
    out0 → [print every @popup 1]:in0
    out1 → [typeroute~]:in0
```

## See also

`array`, `array.filter`, `array.some`
