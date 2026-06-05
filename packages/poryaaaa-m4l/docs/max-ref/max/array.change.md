# array.change

_max · Array_

> Detect array changes

Output an array if the order or value of the elements if different from the previous.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | array in |
| out0 | 1 if different, 0 if identical |
| out1 | 1 if different, 0 if equivalent |

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
- `array(array-value: list)` — Compare an array.
  Compare an array. In the right inlet, the array is stored and no output is generated. In the left inlet, the array is compared to the previous array received in the left inlet, or to any array received in the right inlet since any previous array was received in the left inlet. If the arrays are different, the incoming array will be output from the left inlet. A result (0/1) is sent to the right outlet, depending on whether the two arrays were the same or different.
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Attributes

- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `er` — seen as: `er 56`

## Help patcher examples

### unordered

```
Example — [array.change @unordered 1]  in unordered mode, arrays will be compared without regard for the element order
  fan-in:
    in0 ← [array]
    in1 ← [array]
  fan-out:
    out0 → [print change_unordered status_unordered @popup 1]:in0
    out1 → [print change_unordered status_unordered @popup 1]:in1
```

### basic

```
Example — [array.change]  1 if different, 0 if equivalent / array out if changed / Max lists are internally converted to arrays before comparison.
  fan-in:
    in0 ← [message "5 6 7"]
    in0 ← [array]
    in0 ← [message "er 56"]
    in1 ← [array]
    in1 ← [message "5 6 7"]
    in1 ← [message "er 56"]
  fan-out:
    out0 → [print change status @popup 1]:in0
    out1 → [print change status @popup 1]:in1
```

## See also

`array`, `array.compare`, `dict.compare`, `zl.change`
