# array.tolist

_max · Array_

> Convert an array object to a list

Nested arrays (arrays within arrays) can be automatically flattened with the flatten attribute.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| out0 | list out |

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
- `array(ARG_NAME_0: list)` — Convert an array to a list.
  If the flatten attribute is enabled, all nested arrays will be collapsed into a single list.
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Attributes

- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### basic

```
Example #1 — [array.tolist @flatten 1]  flatten nested arrays
  fan-in:
    in0 ← [array.concat]
  fan-out:
    out0 → [print flat-list @popup 1]:in0
```

```
Example #2 — [array.tolist]  convert an array to a list
  fan-in:
    in0 ← [array.map]
  fan-out:
    out0 → [print tolist @popup 1]:in0
```

## See also

`array`, `array.foreach`, `array.iter`, `array.join`, `iter`
