# array.push

_max · Array_

> Add one or more elements to the end of an array

The base array is sent to the right inlet, which clears any elements sent to the left inlet. Additional elements are sent to the left inlet, and can be repeatedly placed at the end of the output array, without the user needing to update the array in the right inlet. Unlike the JavaScript implementation, the input array is not changed in place.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | array in |
| out0 | pushed array out |

## Messages

- `bang` — Trigger output
  Reprocess previously received array and trigger output.
- `int(ARG_NAME_0: int)` — Push an integer value
  In the left inlet, the integer value will be added to the end of the base array and the result will be output. In the right inlet, convert an incoming integer to an array and replace the base array.
- `float(ARG_NAME_0: float)` — Push a floating-point value
  In the left inlet, the floating-point value will be added to the end of the base array and the result will be output. In the right inlet, convert an incoming floating-point value to an array and replace the base array.
- `list(ARG_NAME_0: list)` — Push a list
  In the left inlet, the list will be added to the end of the base array and the result will be output. In the right inlet, convert an incoming list to an array and replace the base array.
- `anything(ARG_NAME_0: list)` — Push a list
  In the left inlet, the list will be added to the end of the base array and the result will be output. In the right inlet, convert an incoming list to an array and replace the base array.
- `array(ARG_NAME_0: list)` — Push an array
  In the left inlet, the array will be added to the end of the base array and the result will be output. In the right inlet, replace the base array with the incoming array.
- `clear` — Reset the internal state of the object.
  All internal arrays will be cleared.
- `dictionary(ARG_NAME_0: list)` — Push a dictionary
  In the left inlet, the dictionary will be added to the end of the base array and the result will be output. In the right inlet, wrap the dictionary in an array, and replace the base array.
- `string(ARG_NAME_0: list)` — Push a string
  In the left inlet, the string will be added to the end of the base array and the result will be output. In the right inlet, wrap the string in an array, and replace the base array.

## Attributes

- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `df` — seen as: `df df`
- `er` — seen as: `er 56`
- `har` — seen as: `har har`

## Help patcher examples

### basic

```
Example #1 — [array.push @wrapmode 1]  with @wrapmode 1, elements are wrapped in an array before being pushed
  fan-in:
    in0 ← [message "har har"]
    in1 ← [message "er 56"]
  fan-out:
    out0 → [print push-wrap @popup 1]:in0
```

```
Example #2 — [array.push]
  fan-in:
    in0 ← [message "har har"]
    in0 ← [button]    # immediately output array
    in0 ← [message "df df"]    # append these elements to the output array and trigger output
    in1 ← [message "5 6 7"]    # set base array
    in1 ← [message "er 56"]
  fan-out:
    out0 → [print push @popup 1]:in0
```

## See also

`array`, `array.concat`, `array.pop`, `array.shift`, `array.unshift`, `zl.queue`, `zl.stack`
