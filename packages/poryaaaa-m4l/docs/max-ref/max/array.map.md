# array.map

_max · Array_

> Perform an operation on every element of an array object, replacing elements in-place

Each element of an incoming array will be output sequentially. After processing the element, it should be passed back into the right inlet of the array.map object, replacing the element in the array. When iteration is complete, the substituted array will be output.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | map response |
| out0 | mapped array out |
| out1 | mapping entry output |
| out2 | element index |

## Messages

- `bang` — Trigger output
  Reprocess previously received array and trigger output.
- `int(ARG_NAME_0: int)` — Convert an integer to an array.
  In the left inlet, convert an incoming integer to an array, then process as described for the array message. In the right inlet, the incoming value will replace the last-output array element and either trigger the next phase of the iteration, or, if the iteration is complete, cause output of the mapped array.
- `float(ARG_NAME_0: float)` — Convert a floating-point number to an array.
  In the left inlet, convert an incoming floating-point number to an array, then process as described for the array message. In the right inlet, the incoming value will replace the last-output array element and either trigger the next phase of the iteration, or, if the iteration is complete, cause output of the mapped array.
- `list(ARG_NAME_0: list)` — Convert a list to an array.
  In the left inlet, convert an incoming list to an array, then process as described for the array message. In the right inlet, the incoming value will replace the last-output array element and either trigger the next phase of the iteration, or, if the iteration is complete, cause output of the mapped array.
- `anything(ARG_NAME_0: list)` — Convert a list to an array.
  In the left inlet, convert an incoming list to an array, then process as described for the array message. In the right inlet, the incoming value will replace the last-output array element and either trigger the next phase of the iteration, or, if the iteration is complete, cause output of the mapped array.
- `array(ARG_NAME_0: list)` — Iterate elements and perform an operation on each one
  In the left inlet, an incoming array will trigger the output of each element sequentially, as a series of individual messages. The element index will be output from the rightmost outlet, and the element value from the middle outlet.
  Each of these messages can be processed (synchronously or asynchronously) and then returned to the array.map object's right inlet. In the right inlet, the incoming value will replace the last-output array element and either trigger the next phase of the iteration, or, if the iteration is complete, cause output of the mapped array.
- `cancel(ARG_NAME_0: symbol)` — Cancel array mapping.
  Cancels the currently active mapping operation. The array.map object will now be ready to start mapping a new array.
- `dictionary(ARG_NAME_0: list)` — Wrap a dictionary object in an array.
  In the left inlet, wrap an incoming dictionary object in an array, then process as described for the array message. In the right inlet, the incoming value will replace the last-output array element and either trigger the next phase of the iteration, or, if the iteration is complete, cause output of the mapped array.
- `string(ARG_NAME_0: list)` — Wrap a string object in an array.
  In the left inlet, wrap a string object in an array, then process as described for the array message. In the right inlet, the incoming value will replace the last-output array element and either trigger the next phase of the iteration, or, if the iteration is complete, cause output of the mapped array.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `beef` — seen as: `beef rock mess`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out2` — element index

### basic

> perform an operation on every entry in the array. each entry can be processed individually and placed back into the array.

```
Example #1 — [array.map]
  fan-in:
    in0 ← [message "2 3 4"]
    in1 ← [*]
  fan-out:
    out0 → [print map @popup 1]:in0
    out1 → [*]:in0
    out1 → [*]:in1
```

```
Example #2 — [array.map]
  fan-in:
    in0 ← [message "beef rock mess"]
    in1 ← [string.concat y] ← [array.map]
  fan-out:
    out0 → [print map @popup 1]:in0
    out1 → [string.concat y]:in0
```

## See also

`array`, `array.filter`, `array.reduce`
