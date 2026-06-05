# array.deserialize

_max · Array_

> Parse a string, symbol or list to an array.

A string/symbol/list representing an array in JSON format (e.g. "[ 25, 50, 75, 100 ]") will be converted into an array object.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| out0 | deserializeped array out |

## Messages

- `bang` — Trigger output
  Reprocess previously received data and trigger output.
- `int(ARG_NAME_0: int)` — Convert an integer to an array.
  Convert an incoming integer to an array, then process as described for the array message.
- `float(ARG_NAME_0: float)` — Convert a floating-point number to an array.
  Convert an incoming floating-point number to an array, then process as described for the array message.
- `list(ARG_NAME_0: list)` — Convert or parse a list to an array.
  Convert or parse an incoming list to an array, then process as described for the array message.
- `anything(ARG_NAME_0: list)` — Convert or parse a list to an array.
  Convert or parse an incoming list to an array, then process as described for the array message.
- `array(ARG_NAME_0: list)` — Set the output array.
  An incoming array will be copied to the object's output array, which will be sent from the object's outlet.
- `dictionary(ARG_NAME_0: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(ARG_NAME_0: list)` — Parse a string object to an array.
  A string object will be parsed to an array of possible, then processed as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `[{\"id\":\"7ij\"\` — seen as: `[{\"id\":\"7ij\"\,\"url\":\"https://cdn2.thecatapi.com/images/7ij.jpg\"\,\"width\":500\,\"height\":500}]`

## Help patcher examples

### basic

```
Example — [array.deserialize]
  fan-in:
    in0 ← [message "[{\"id\":\"7ij\"\,\"url\":\"https://cdn2.thecatapi.com/images/7ij.jpg\"\,\"width\":500\,\"height\":500}]"]    # send a string or symbol (or list) to be parsed into an array.
  fan-out:
    out0 → [array.at 0]:in0
    out0 → [print parse @popup 1]:in0
```

## See also

`array`, `array.tolist`, `array.tostring`, `array.tosymbol`, `dict.deserialize`
