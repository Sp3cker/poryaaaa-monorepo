# join

_max · Lists_

> Combine items into a list

Takes separate untyped items and combines them into an output list.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Input (0) |
| in1 | Input (1) |
| out0 | Output |

## Arguments

- **inlets** (`int`) — Number of inlets
  Specifies the number of inlets. If there is no argument, there will be two inlets, and the two list items will be set to (int) 0 initially.

## Messages

- `bang` — Send out current list
  In either inlet: Causes join to send out a list composed of the currently stored items.
- `int(input: int)` — Store data as a list element
  The number is stored in the join object as an item in a list with its position in the list corresponding to the inlet in which it was received. If the inlet is a 'trigger' input, the entire list is sent out the outlet.
- `float(input: float)` — Store data as a list element
  The number is stored in the join object as an item in a list with its position in the list corresponding to the inlet in which it was received. If the inlet is a 'trigger' input, the entire list is sent out the outlet.
- `list(values: list)` — Store data as list elements
  The list is stored in the join object as an array of items in a list with its position in the list corresponding to the inlet in which it was received. If the inlet is a 'trigger' input, the entire list is sent out the outlet.
- `anything(values: list)` — Store data as list elements
  The message with its arguments is stored in the join object as an array of items in a list with its position in the list corresponding to the inlet in which it was received. If the inlet is a 'trigger' input, the entire list is sent out the outlet.
- `set(values: list)` — Store data with no output
  The list is stored in the join object as an array of items in a list with its position in the list corresponding to the inlet in which it was received.

## Attributes

- `@basic` (int)
- `@label` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `"toto` — seen as: `"toto titi"`
- `yoyo` — seen as: `yoyo`

## Help patcher examples

### basic

```
Example #1 — [join 3 @triggers -1]
  fan-in:
    in0 ← [multislider]
    in0 ← [flonum]
    in1 ← [multislider]
    in1 ← [flonum]
    in2 ← [multislider]    # takes lists as well!
    in2 ← [flonum]
  fan-out:
    out0 → [multislider]:in0
    out0 → [zl len]:in0
```

```
Example #2 — [join 5 @triggers 1 3]
  fan-in:
    in0 ← [button]
    in0 ← [number]
    in0 ← [message "1 2 3"]
    in1 ← [flonum]
    in1 ← [number]
    in2 ← [number]
    in2 ← [message "yoyo"]
    in3 ← [flonum]    # mixed changing types
    in4 ← [message ""toto titi""]
    in4 ← [number]
  fan-out:
    out0 → [message ""]:in1
```

## See also

`pack`, `pak`, `unjoin`, `unpack`
