# array.unique

_max · Array_

> Filtering duplicates and subtract arrays

Combine two arrays into a new array object containing non-duplicate entries of the first array which do not appear in the second array.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | array in |
| out0 | array out |

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
- `array(ARG_NAME_0: list)` — Process the array, removing duplicates and filtering entries.
  The output will be a new array containing non-duplicate entries of the first array which do not appear in the second array.
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `hating` — seen as: `hating the number 5 with sunshine`
- `love` — seen as: `love to eat 5 jellybeans and sunshine`

## Help patcher examples

### basic

```
Example — [array.unique]
  fan-in:
    in0 ← [message "0 1 1 2 8"]
    in0 ← [message "love to eat 5 jellybeans and sunshine"]
    in1 ← [message "2 2 3 3 5"]
    in1 ← [message "hating the number 5 with sunshine"]
  fan-out:
    out0 → [print unique @popup 1]:in0
```

## See also

`array`, `array.sect`, `array.union`, `zl.sect`, `zl.union`, `zl.unique`
