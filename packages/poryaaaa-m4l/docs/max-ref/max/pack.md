# pack

_max · Lists_

> Create a list

Combine items into an output list. The arguments determine the list format and types of the list elements. The number of inlets is based on the number of arguments.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | value for the first list element, causes output |
| in1 | value for the second list element |
| out0 | Output list |

## Arguments

- **list-elements** (`any`) _(optional)_ — List elements
  The number of inlets is determined by the number of arguments. Each argument sets an initial type and value for an item in the list produced by pack. If there are no arguments, the object will be created with two inlets, and the two list elements will be set to (int) 0 initially.

## Messages

- `bang` — Output currently stored list
- `int(input: int)` — Store an int list element
  The number is stored as an item in a list, with its position in the list corresponding to the inlet in which it was received. A number in the left inlet also causes the list to be output. If the inlet has been initialized with a float or symbol argument, the incoming number will be converted to a float or a blank symbol.
- `float(input: float)` — Store a float list element
  The number is stored as an item in a list, with its position in the list corresponding to the inlet in which it was received. A number in the left inlet also causes the entire list to be the output. If the inlet has been initialized with an int or symbol argument, the incoming number will be converted to an int or a (blank) symbol.
- `list(input: list)` — Set multiple list elements
  Any multi-item message is treated as a list. The first item in the incoming list is stored in in the location that corresponds to the inlet in which it was received. Each subsequent item is stored as if it had arrived in subsequent inlets (limited to the number of inlets available). A list received in the left inlet causes the entire stored list to be sent out the outlet.
- `anything(input: list)` — Store values in the list
  Performs the same function as list.
- `nth(index: int)` — Return one list element
  The nth message will output the stored list element at the index. Output is sent from the first outlet.
- `send(receive-name: list)` — Send the list to receive objects
  Sends the stored list to all receive objects with a matching name.
- `set(input: list)` — Set data without output
  Sets the values without causing list output. Although the set message works with any inlet, it is only meaningful in the left inlet, which is the only inlet that will trigger output.
- `symbol(input: symbol)` — Store a symbol list element
  Stores the symbol in the list at the position corresponding to the inlet it was received. If the list element was initialized as a number, the symbol is converted to 0 before being stored. A symbol in the left inlet triggers output of the list.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `dog` — seen as: `dog`

## Help patcher examples

### conversion

> For float inlets:

> For symbol inlets:

> - integer input is unchanged

> - integer input is converted

> - integer input is blank

> - float input is truncated

> - float input is unchanged

> - float input is blank

```
Example #1 — [pack s symbol]
  fan-in:
    in0 ← [message "dog"]
    in0 ← [flonum]
    in0 ← [number]    # - symbols are unchanged
  fan-out:
    out0 → [message ""]:in1
```

```
Example #2 — [pack 0. float]
  fan-in:
    in0 ← [message "dog"]
    in0 ← [flonum]
    in0 ← [number]    # - symbols become 0.0
  fan-out:
    out0 → [message ""]:in1
```

```
Example #3 — [pack 0 integer]
  fan-in:
    in0 ← [flonum]
    in0 ← [number]    # - symbols become 0
    in0 ← [message "dog"]
  fan-out:
    out0 → [message "0 integer"]:in1
```

### output

```
Example #1 — [pack A B C D]
  fan-in:
    in0 ← [message "nth $1"]
  fan-out:
    out0 → [message ""]:in1
```

```
Example #2 — [pack 0 is my number]
  fan-in:
    in0 ← [button]
    in0 ← [prepend set] ← [number]    # The 'set' message updates the first element without causing output.
  fan-out:
    out0 → [message ""]:in1
```

```
Example #3 — [pack my dog has X]
  fan-in:
    in0 ← [trigger b s]
    in3 ← [trigger b s]
  fan-out:
    out0 → [message ""]:in1
```

### basic

```
Example — [pack 0 0. sym]
  fan-in:
    in0 ← [button]
    in0 ← [number]    # input to left inlet to replace value and force output
    in1 ← [flonum]    # replace value with no output
    in2 ← [umenu]    # replace value with no output
  fan-out:
    out0 → [message ""]:in1
```

## See also

`bondo`, `buddy`, `join`, `match`, `pak`, `swap`, `thresh`, `unjoin`, `unpack`, `zl`
