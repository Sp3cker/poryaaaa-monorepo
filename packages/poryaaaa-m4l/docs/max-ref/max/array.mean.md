# array.mean

_max · Array_

> Calculate the mean of the numerical elements of an array

The mean is the total of all numerical values divided by the number of values (and is also known as the average). Non-numerical elements (symbols, arrays, strings, dictionaries) are ignored.

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
- `array(ARG_NAME_0: list)` — Calculate the mean
  A single floating-point number will be sent from the object's outlet. Non-numerical elements are ignored in the calculation.
- `dictionary(ARG_NAME_0: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(ARG_NAME_0: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in1` — array in

### basic

```
Example — [array.mean]
  fan-in:
    in0 ← [message "100 max gen rnbo 0"]    # symbols are ignored
    in0 ← [message "0 10"]
  fan-out:
    out0 → [print mean @popup 1]:in0
```

## See also

`array`, `array.min`, `array.max`, `array.median`, `array.mode`, `array.stddev`, `mean`
