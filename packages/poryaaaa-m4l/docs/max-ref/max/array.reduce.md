# array.reduce

_max · Array_

> Combine array elements based on a custom function

Perform an operation on every element of an array object, outputting the accumulated response.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | reduce response |
| out0 | reduced value out |
| out1 | accumulated value |
| out2 | element value |
| out3 | element index |

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
- `array(ARG_NAME_0: list)` — Reduce to a single value with a custom function.
  The object will iterate over the array, outputting each element, the index of each element, and the accumulated value. Send a value back to the rightmost inlet to accumulate the result. After iterating through each element, the object will output the final accumulated value.
- `cancel` — TEXT_HERE
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Attributes

- `@default` (atom)
- `@label` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out3` — element index

### basic

> set an initial value for the first iteration of the array elements. in this case, we use a symbol (nix) to clear the concatenation string.

> perform an operation on every entry in the array, passing a return value back to the object. The return value will be carried over to the next iteration, and so on. A single atom is output as the result of the iteration.

```
Example #1 — [array.reduce @initial nix]
  fan-in:
    in0 ← [message "1 2 3 4 5"]
    in1 ← [string.concat]
  fan-out:
    out0 → [print reduce @popup 1]:in0
    out1 → [sel nix]:in0
    out2 → [string.concat]:in1
```

```
Example #2 — [array.reduce]
  fan-in:
    in0 ← [message "1 2 3 4 5"]    # set array
    in1 ← [+]
  fan-out:
    out0 → [print reduce @popup 1]:in0
    out1 → [+]:in0
    out2 → [+]:in1
```

## See also

`array`, `array.filter`, `array.map`, `zl.median`, `zl.sum`
