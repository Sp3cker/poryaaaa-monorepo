# array.join

_max · Array_

> Convert an array object to a string object with an optional separator string

Join the elements of an array together to form a string. The optional separator string will be placed between each element.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | separator string in |
| out0 | string out |

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
- `array(ARG_NAME_0: list)` — Combines the elements of the array into a single string.
  The optional separator string will be placed between each two elements of the array.
- `clear` — Clear the separator string.
  Resets the separator string to an empty string.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `"` — seen as: `" "`
- `::` — seen as: `::`
- `\` — seen as: `\,`

## Help patcher examples

### basic

```
Example — [array.join]  string output, joined array
  fan-in:
    in0 ← [array] ← [message "I was meant to be a string"]
    in1 ← [message "::"]
    in1 ← [message "clear"]
    in1 ← [message "" ""]
    in1 ← [message "\,"]
  fan-out:
    out0 → [print join @popup 1]:in0
```

## See also

`array`, `array.tolist`
