# array

_max · Array_

> Create or duplicate an array object

Create or duplicate a named array object.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages in |
| in1 | set array value, messages in |
| out0 | arrayobj out |
| out1 | atoms out |
| out2 | dumpout |

## Arguments

- **value** (`atoms`) _(optional)_ — Array contents
  The initial contents of the array can be supplied as a Max list.

## Messages

- `bang` — Trigger output
  Output the current array.
- `int(value: int)` — Convert an integer to an array
  Convert an integer to an array. The integer will be placed inside of an array, which will be sent to the outlet.
- `float(value: float)` — Convert a floating-point number to an array
  Convert a floating-point number to an array. The floating-point number will be placed inside of an array, which will be sent to the outlet.
- `list(list-value: list)` — Convert a list to an array
  Convert a list to an array. The contents of the list will be placed inside the array, which will be sent to the outlet.
- `anything(list-value: list)` — Convert a list to an array
  Convert a list to an array. The contents of the list will be placed inside the array, which will be sent to the outlet.
- `append(value: list)` — Append a value to the end of the current array
  Append a value to the end of the current array. The array will not be output in response to this message. Use bang to force output.
- `array(ARG_NAME_0: list)` — Make a copy of an array
  Make a copy of an array. The incoming array will be cloned and passed to the outlet.
- `atoms` — Output the current array as a list
  Output the current array as a list out of the array objects middle outlet. The elements of the array will be output as a Max list.
- `clear` — Clear the current array
  Clear the current array. The (now empty) array will not be output in response to this message. Use bang to force output.
- `delete(index: int)` — Delete an entry in the array
  Delete an entry in the array. The indexed element will be removed from the array (indices are 0-based). The array will not be output in response to this message. Use bang to force output.
- `dictionary(dictionary-value: list)` — Wrap a dictionary in an array
  Wrap a dictionary in an array. The dictionary will be placed inside of an array, which will be sent to the outlet.
- `get(index: int)` — Get an array element
  Get an array element. The element will be passed to the rightmost outlet in the form get [index] [value].
- `insert(index: int, value: list)` — Insert a value into the current array
  Insert a value into the current array. A new array element will be created at the index provided, with the supplied value. Any existing array elements will be shifted to make room for the new element. The array will not be output in response to this message. Use bang to force output.
- `prepend(value: list)` — Place a new entry at the start of the current array
  Place a new entry at the start of the current array. The array will not be output in response to this message. Use bang to force output.
- `replace(index: int, value: list)` — Replace a value in the current array
  Replace a value in the current array at an existing index. The array will not be output in response to this message. Use bang to force output.
- `reserve(number-of-entries: int)` — Reserve memory for a provided number of entries (doesn't resize array)
  Reserve memory for a provided number of entries (doesn't resize array). This is rarely needed, as the object manages its own memory and grows as necessary. If the desired array size is known, and re-allocation of the array needs to be avoided, this message can be used to ensure that the array object is pre-allocated to the desired size.
- `shrink` — Reduce memory usage to the current array object length
  Reduce memory usage to the current array object length. This is rarely needed. The array object does not automatically shrink if its contents are removed or cleared, this message can be used to ensure that the object doesn't use more resources than necessary.
- `string(string-value: list)` — Wrap a string in an array
  Wrap a string in an array. The string will be placed inside of an array, which will be sent to the outlet.

## Attributes

- `@label` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `df` — seen as: `df df`
- `er` — seen as: `er 56`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — atoms out
> - `out2` — dumpout

### basic

```
Example — [array 1 2 3 bar bap]
  fan-in:
    in0 ← [button]
    in0 ← [number]
    in0 ← [message "df df"]    # store and immediately output
    in1 ← [message "5 6 7"]
    in1 ← [message "er 56"]    # set stored array
  fan-out:
    out0 → [print array @popup 1]:in0
```

## See also

`dict`, `string`, `array.change`, `array.compare`, `array.concat`, `array.every`, `array.filter`, `array.flatten`, `array.foreach`, `array.frombuffer`, `array.group`, `array.index`, `array.indexmap`, `array.indexof`, `array.iter`, `array.join`, `array.length`, `array.map`, `array.pop`, `array.push`, `array.reduce`, `array.remove`, `array.reverse`, `array.rotate`, `array.routepass`, `array.scramble`, `array.sect`, `array.shift`, `array.slice`, `array.some`, `array.sort`, `array.split`, `array.stream`, `array.subarray`, `array.thin`, `array.tobuffer`, `array.tolist`, `array.tostring`, `array.tosymbol`, `array.tuplewise`, `array.union`, `array.unique`, `array.unshift`, `array.wrap`
