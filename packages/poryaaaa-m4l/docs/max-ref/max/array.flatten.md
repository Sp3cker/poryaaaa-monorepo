# array.flatten

_max · Array_

> Flatten a multi-dimensional array object to a single dimension

Flattening can be simple sequential, or recursive sequential, depending on mode.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | array in |
| out0 | flattened array out |

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
- `array(ARG_NAME_0: list)` — Flatten array
  Performs array flattening as described for the mode attribute, then outputs the flattened array.
- `dictionary(ARG_NAME_0: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(ARG_NAME_0: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Attributes

- `@mode` (int) — Flatten Mode
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in1` — array in

### basic

> @mode 1 (simple) flattens non-recursively, moving the first level of nesting to the top level

```
Example #1 — [array.flatten @mode 0]
  fan-in:
    in0 ← [p make_sequential_array] ← [button]    # @mode 0 (recursive) flattens all nested elements to the top level, sequentially. / [ 1, 2, 3, [ 4, 5, 6, [ 7, 8, 9 ] ] ] / p make_sequential_array emits: "1 2 3" | "4 5 6" | "7 8 9" / generate a nested array for flattening
  fan-out:
    out0 → [print recursive @popup 1]:in0
```

```
Example #2 — [array.flatten @mode 1]
  fan-in:
    in0 ← [p make_sequential_array] ← [button]    # @mode 0 (recursive) flattens all nested elements to the top level, sequentially. / [ 1, 2, 3, [ 4, 5, 6, [ 7, 8, 9 ] ] ] / p make_sequential_array emits: "1 2 3" | "4 5 6" | "7 8 9" / generate a nested array for flattening
  fan-out:
    out0 → [print simple @popup 1]:in0
```

## See also

`array`
