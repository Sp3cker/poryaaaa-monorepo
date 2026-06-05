# array.compare

_max · Array_

> Compare two arrays for equality

Arrays are compared for their value and order.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | array in |
| out0 | 1 if identical, 0 if different |

## Messages

- `bang` — Compare the left and right inputs again and output the result (0/1).
  Compare the left and right inputs again and output the result (0 - Unequal/ 1 - Equal)
- `int(value: int)` — Convert an integer to an array for comparison.
  Convert an incoming integer to an array, then process as described for the array message.
- `float(value: float)` — Convert a floating-point number to an array for comparison.
  Convert an incoming floating-point number to an array, then process as described for the array message.
- `list(list-value: list)` — Convert a list to an array for comparison.
  Convert an incoming list to an array, then process as described for the array message.
- `anything(list-value: list)` — Convert a list to an array for comparison.
  Convert an incoming list to an array, then process as described for the array message.
- `array(array-value: list)` — Compare an array.
  Compare an array . Set the array to be compared in the right inlet without triggering output. In the left inlet, set the array to compare to and immediately trigger the array out. In the right inlet, the array is stored and no output is generated. In the left inlet, the array is compared to any array received in the right inlet and a result (0/1) is sent to the outlet.
- `dictionary(dictionary-value: dictionary)` — Wrap a dictionary object in an array for comparison.
- `string(string-value: string)` — Wrap a string object in an array for comparison.

## Attributes

- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `er` — seen as: `er 56`

## Help patcher examples

### unordered

> in unordered mode, arrays will be compared without regard for the element order

```
Example — [array.compare @unordered 1]
  fan-in:
    in0 ← [message "5 6 7"]
    in0 ← [array]
    in0 ← [message "er 56"]
    in1 ← [array]
    in1 ← [message "7 6 5"]
    in1 ← [message "56 er"]
  fan-out:
    out0 → [print compare_unordered @popup 1]:in0
```

### basic

> array objects can accept arrays or Max lists, which are internally converted to arrays pre-comparison.

```
Example — [array.compare]
  fan-in:
    in0 ← [message "5 6 7"]
    in0 ← [array]
    in0 ← [message "er 56"]
    in1 ← [array]
    in1 ← [message "5 6 7"]
    in1 ← [message "er 56"]
  fan-out:
    out0 → [print compare @popup 1]:in0    # 0 - unequal comparison 1 - equal comparison
```

## See also

`array`, `array.change`, `dict.compare`, `zl.change`
