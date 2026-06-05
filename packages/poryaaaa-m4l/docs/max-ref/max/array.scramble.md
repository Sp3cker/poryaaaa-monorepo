# array.scramble

_max · Array_

> Randomize the order of elements in an array object

When triggered, output the scrambled array and the reordered index list.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in (trigger output) |
| in1 | set array |
| out0 | scrambled array out |
| out1 | reordered indexes list |

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
- `array(array-value: list)` — Scramble an array
  In the right inlet, set an internal array without triggering output. In the left inlet, the contents of the array are scrambled and triggers output. The scrambled array is sent out the left outlet, and the scrambled index is sent out the right outlet.
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `monkey` — seen as: `monkey see monkey do makes a monkey out of you`
- `you` — seen as: `you ain't nothing but a hound dog`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — reordered indexes list

### basic

```
Example — [array.scramble]
  fan-in:
    in0 ← [message "monkey see monkey do makes a monkey out of you"]    # set with ouput
    in0 ← [button]
    in1 ← [message "you ain't nothing but a hound dog"]    # set without ouput
  fan-out:
    out0 → [print scramble @popup 1]:in0
```

## See also

`array`, `zl.scramble`
