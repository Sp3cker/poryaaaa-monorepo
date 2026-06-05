# array.min

_max · Array_

> Calculate the minimum of the numerical elements of an array

Non-numerical elements (symbols, arrays, strings, dictionaries) are ignored.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | array in |
| out0 | result out |

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
- `array(ARG_NAME_0: list)` — Calculate the minimum
  A single atom (int or float, depending on the input) will be sent from the object's outlet. Non-numerical elements are ignored in the calculation.
- `dictionary(ARG_NAME_0: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(ARG_NAME_0: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `foo` — seen as: `foo bar 10 99999999 obj:dictionary`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in1` — array in

### basic

```
Example — [array.min]
  fan-in:
    in0 ← [message "foo bar 10 99999999 obj:dictionary"]    # foo bar and obj:dictionary are ignored
    in0 ← [message "10 1 -1 1000"]
  fan-out:
    out0 → [print min @popup 1]:in0
```

## See also

`array`, `array.max`, `array.mean`, `array.median`, `array.mode`, `array.stddev`, `minimum`
