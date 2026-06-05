# array.index

_max · Array_

> Output the indexed element of an array object

Extract an element from an incoming array object, based on the index specified (0-based).

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | position in array to extract |
| out0 | indexed element of the array |
| out1 | array with indexed element removed |

## Arguments

- **index** (`int`) _(optional)_ — Index
  The index to look up for incoming arrays. By default, this is 0 (the first element).

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
- `array(ARG_NAME_0: list)` — Extract indexed element
  Output the element of the array at the provided index.
- `dictionary(ARG_NAME_0: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `in1(ARG_NAME_0: int)` — Set index
  Set the index used to extract an element from an incoming array object. Indices begin at 0 for the first element.
- `string(ARG_NAME_0: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Attributes

- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `df` — seen as: `df df`

## Help patcher examples

### basic

```
Example — [array.index 1]
  fan-in:
    in0 ← [message "5 6 7"]
    in0 ← [message "df df"]
    in0 ← [message "anything b c def jam"]
    in1 ← [number]
  fan-out:
    out0 → [print index @popup 1]:in0
    out1 → [print remainder @popup 1]:in0
```

## See also

`array`, `zl.mth`, `zl.nth`
