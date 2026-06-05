# array.indexof

_max · Array_

> Search for the index of an array element

Output the position of an array element within a longer array as an integer index (0-based). Output -1 if the element cannot be found.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | array in |
| out0 | index out |

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
- `array(ARG_NAME_0: list)` — Set the array to search in.
  This search is case-sensitive. Will output the position of the provided element (left inlet) in the larger array (right inlet) if found, otherwise will output -1.
  A multiple-element input in the left inlet will match an array (e.g. a b c will find index 2 of [ 1, 2, [ a, b, c ], 4]), but does not match a sequence of individual elements (that is, a b c will not find anything in [ 1, 2, a, b, c, 4 ]).
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

- `antelope` — seen as: `antelope`
- `badger` — seen as: `badger antelope crispy 6`
- `offset` — seen as: `offset $1, bang`, `offset 0, 4`

## Help patcher examples

### basic

```
Example #1 — [array.indexof]
  fan-in:
    in0 ← [message "offset 0, 4"]
    in0 ← [message "offset $1, bang"]
    in1 ← [message "1 2 3 4 3 2 1 4 3 2 1 4 5 6 4 2 0"]
  fan-out:
    out0 → [print indexof_offset @popup 1]:in0    # output first match starting at an offset into the input array
    out0 → [if $i1 >= 0 then $i1]:in0
```

```
Example #2 — [array.indexof @all 1]
  fan-in:
    in0 ← [message "4"]
    in1 ← [message "1 2 3 4 3 2 1 4 3 2 1 4 5 6 4 2 0"]
  fan-out:
    out0 → [print indexof_all @popup 1]:in0    # output all matches
```

```
Example #3 — [array.indexof]
  fan-in:
    in0 ← [message "antelope"]
    in0 ← [message "6"]
    in1 ← [message "badger antelope crispy 6"]
  fan-out:
    out0 → [print indexof @popup 1]:in0
```

## See also

`array`, `array.foreach`, `zl.sub`
