# array.some

_max · Array_

> Test the elements of an array object for a matching condition

Each element of an incoming array will be output sequentially. Each element can be tested and a 0 or 1 passed back into the right inlet of the array.some object, indicating whether the element passed the test. When iteration is complete, either a 0 or 1 will be output, signalling whether some elements of the array matched the testing condition.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | test response (0/1) |
| out0 | some elements of input array pass test (0/1) |
| out1 | test output |
| out2 | element index |

## Messages

- `bang` — Trigger output
  Reprocess previously received array and trigger output.
- `int(ARG_NAME_0: int)` — Convert an integer to an array.
  Convert an incoming integer to an array, then process as described for the array message.
- `float(ARG_NAME_0: float)` — Convert a floating-point number to an array.
  Convert an incoming floating-point number to an array, then process as described for the array message.
- `list(ARG_NAME_0: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `anything(ARG_NAME_0: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `array(ARG_NAME_0: list)` — Iterate elements and test for a condition
  In the left inlet, an incoming array will trigger the output of each element sequentially, as a series of individual messages. The element index will be output from the rightmost outlet, and the element value from the middle outlet.
  Each of these messages can be tested (synchronously or asynchronously) and the result of that test (a 0 (failed) or a 1 (passed)) returned to the array.some object's right inlet. When iteration is complete, and a result for each element has been sent to the object's right inlet, a 0 (no elements passed the test) or a 1 (some elements passed the test) will be sent from the object's left outlet.
- `cancel(ARG_NAME_0: symbol)` — Cancel iteration
  Cancels the currently active iteration and testing. The array.some object will now be ready to start testing a new array.
- `dictionary(ARG_NAME_0: list)` — Wrap a dictionary object in an array.
  In the left inlet, wrap an incoming dictionary object in an array, then process as described for the array message.
- `in1(ARG_NAME_0: int)` — The test result for an element
  0 (test failed) or a 1 (test passed) should be sent to this inlet for each iterated element.
- `string(ARG_NAME_0: list)` — Wrap a string object in an array.
  In the left inlet, wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `er` — seen as: `er 56`
- `kling` — seen as: `kling klang`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out2` — element index

### basic

> test the incoming array for some property. In this case, whether any entries are ints or floats. Entries are output sequentially; send a 1 into the right inlet to indicate that the entry passes the test, pass 0 to indicate failure.

```
Example — [array.some]
  fan-in:
    in0 ← [message "kling klang"]
    in0 ← [message "er 56"]
    in0 ← [message "3. 4 5.6"]
    in0 ← [message "5 6 7"]
    in1 ← [t 1]
    in1 ← [t 0] ← [typeroute~] ← [array.some]
  fan-out:
    out0 → [print some @popup 1]:in0
    out1 → [typeroute~]:in0
```

## See also

`array`, `array.every`, `array.filter`
