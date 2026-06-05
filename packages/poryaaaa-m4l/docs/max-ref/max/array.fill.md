# array.fill

_max · Array_

> Generate an array of a specified length

Creates a new array object of a specified length, pre-filled with elements.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | new array length |
| out0 | filled array |

## Arguments

- **length** (`int`) — Array length
  The length of the generated array.
- **initial-contents** (`list`) _(optional)_ — Initial contents
  Any data to be used to populate the array (without any initial data, the array will be filled with 0 s).

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
- `array(ARG_NAME_0: list)` — Create and fill an array
  A new array object will be created at the length specified by the object's first argument, or by a number received in the rightmost inlet. The object will then be filled with (by default) 0, or by the contents specifed by additional arguments to the array.fill object, or an incoming array in the left inlet.
  If the initial contents are not the same length as the output array, the mode will be used to determine how to handle the length discrepancy.
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `in1(length: int)` — Array length
  The length of the generated array.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Attributes

- `@mode` (int) — Fill Mode
  The mode determines how to resize data which doesn't match the length of the output array. Several options are available.
- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `e` — seen as: `e f g`

## Help patcher examples

### basic

> @mode 0 (rrepeat) fills the output array with a repetition of the provided elements.

> @mode 3 (linear interpolation) fills the output array by interpolating between supplied elements.

```
Example #1 — [array.fill 5 a b c @mode 3]
  fan-in:
    in0 ← [message "0. 1."]
    in0 ← [message "1 10"]    # all ints are interpolated using ints
    in1 ← [number] ← [loadmess 5]    # change output array length
  fan-out:
    out0 → [print mode3 @popup 1]:in0
```

```
Example #2 — [array.fill 5 a b c]
  fan-in:
    in0 ← [button]
    in1 ← [number] ← [loadmess 5]    # change output array length
  fan-out:
    out0 → [print mode0 @popup 1]:in0
```

```
Example #3 — [array.fill 5 @mode 1]
  fan-in:
    in0 ← [message "e f g"]    # @mode 1 (repeat last) fills the output array with the provided elements, repeating the final element.
    in1 ← [number] ← [loadmess 5]    # change output array length
  fan-out:
    out0 → [print mode1 @popup 1]:in0
```

## See also

`array`, `array.expr`
