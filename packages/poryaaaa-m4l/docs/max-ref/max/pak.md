# pak

_max · Lists_

> Output a list when any element changes

The pak object (pronounced "pock") offers much of the functionality of pack, but outputs the entire list whenever input is received in any inlet.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | int to be Element 1 in the list |
| in1 | int to be Element 2 in the list |
| out0 | Combined list from Inputs |

## Arguments

- **list-elements** (`any`) _(optional)_ — List elements
  The number of inlets is determined by the number of arguments. Each argument sets an initial type and value for an item in the list produced by pak. If there are no arguments, the object will be created with two inlets, and the two list elements will be set to (int) 0 initially.

## Messages

- `bang` — Output currently stored list
- `int(input: int)` — Store an int list element, output list
  The number is stored as an item in a list, with its position in the list corresponding to the inlet in which it was received, then the entire list is output. If the inlet has been initialized with a float or symbol argument, the incoming number will be converted to a float or a blank symbol.
- `float(input: float)` — Store a float element, output list
  The number is stored as an item in a list, with its position in the list corresponding to the inlet in which it was received, then the entire list is output. If the inlet has been initialized with an int or symbol argument, the incoming number will be converted to an int or a (blank) symbol.
- `list(input: list)` — Set multiple list elements
  Any multi-item message is treated as a list. The first item in the incoming list is stored in in the location that corresponds to the inlet in which it was received. Each subsequent item is stored as if it had arrived in subsequent inlets (limited to the number of inlets available). After all values are stored, the list is output.
- `anything(input: list)` — Store values in the list, output list
  Performs the same function as list.
- `set(message: list)` — Set data without output

## Help patcher examples

### basic

```
Example — [pak 0 0. sym 0]
  fan-in:
    in0 ← [number]
    in1 ← [flonum]
    in2 ← [umenu]
    in3 ← [number]    # input to any inlet triggers output.
  fan-out:
    out0 → [message ""]:in1
```

## See also

`bondo`, `buddy`, `join`, `match`, `swap`, `thresh`, `unjoin`, `unpack`, `zl`
