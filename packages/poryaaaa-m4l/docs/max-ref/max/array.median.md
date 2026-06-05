# array.median

_max · Array_

> Calculate the median of the numerical elements of an array

The median is the value halfway between the minimum and maximum of the set of numerical elements. Non-numerical elements (symbols, arrays, strings, dictionaries) are ignored.

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
- `array(ARG_NAME_0: list)` — Calculate the median
  A single floating-point number will be sent from the object's outlet. Non-numerical elements are ignored in the calculation.
- `dictionary(ARG_NAME_0: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(ARG_NAME_0: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Attributes

- `@evenmode` (int) — Even Length Behavior
  Determines how the array.median object behaves When the incoming array has an even number of entries.
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in1` — array in

### basic

```
Example — [array.median]
  fan-in:
    in0 ← [message "100 max gen rnbo 0"]    # symbols are ignored
    in0 ← [message "0 2 10"]
  fan-out:
    out0 → [print map @popup 1]:in0
```

## See also

`array`, `array.min`, `array.max`, `array.mean`, `array.mode`, `array.stddev`, `zl.median`
