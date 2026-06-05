# combine

_max · Data_

> Combine multiple items into a single symbol

Combines a list of items into a single symbol. It works similar to pack and sprintf. The behavior can be modified with attributes that provide number padding and triggered output.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | (0) |
| out0 | merged output |
| out1 | dumpout |

### Port details

**`in0` ((0)):** First element to be formatted into the combined symbol.

## Arguments

- **inlets** (`anything`) _(optional)_ — Number of inlets
  The number of inlets is determined by the number of arguments. Each argument sets an initial type and value for an item in the list stored by the combine object. If a number argument contains a decimal point, that item will be stored as a float. If the argument is a symbol, that item will be stored as a symbol.

## Messages

- `bang` — Output stored and formatted symbol
  Causes combine to send out a list of the items currently stored.
- `int(input: int)` — Store data and format output
  The number is stored in combine as an item in a list, with its position in the list corresponding to the inlet in which it was received. The combined output is then generated from this list and sent out the outlet.
- `float(input: float)` — Store data and format output
  The number is stored in combine as an item in a list, with its position in the list corresponding to the inlet in which it was received. The combined output is then generated from this list and sent out the outlet.
- `list(input: list)` — Store data and format output
  When a list is sent into any inlet of the combine object, each item in the list is converted to symbols and stored. Its position in the list corresponds to the inlet in which it was received. If the list is sent to the left inlet, the combined output is then generated and sent out the outlet.
- `anything(input: list)` — Store data and format output
  The symbol is stored in combine as an item in a list, with its position in the list corresponding to the inlet in which it was received. The combined output is then generated from this list and sent out the outlet.

## Attributes

- `@basic` (int)
- `@label` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `Users/Shared` — seen as: `Users/Shared`
- `anotherfile` — seen as: `anotherfile - 360 . html`
- `bar` — seen as: `bar`
- `foo` — seen as: `foo`
- `frog` — seen as: `frog`
- `pizza` — seen as: `pizza`
- `tadpole` — seen as: `tadpole :/ Users/polywog / bar`
- `txt` — seen as: `txt`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — dumpout

### IP-address

```
Example — [combine 127 . 0 . 0 . 1 @triggers 0 2 4 6]
  fan-in:
    in0 ← [number]
    in2 ← [number]
    in4 ← [number]
    in6 ← [number]
  fan-out:
    out0 → [prepend host]:in0
```

### time-examples

> Please note that for every single unique time that is output by combine a new symbol will be added to Max's symbol table consuming an ever growing amount of Max's resources. If at all possible, you are urged to pass time values as lists of numbers and not single symbols.

```
Example #1 — [combine h : m : s . ms @triggers 0 2 4 6 @padding 2 0 2 0 2 0 3]
  fan-in:
    in0 ← [button]
    in0 ← [number]
    in2 ← [number]
    in4 ← [number]
    in6 ← [flonum]
    in6 ← [message "foo"]
    in6 ← [number]
  fan-out:
    out0 → [message "90:43:11.016"]:in1
```

```
Example #2 — [combine h : m : s . ms @triggers 2 6]
  fan-in:
    in0 ← [button]
    in0 ← [number]
    in2 ← [number]
    in4 ← [number]
    in6 ← [flonum]
    in6 ← [message "foo"]
    in6 ← [number]
  fan-out:
    out0 → [message "11:3:57.123"]:in1
```

```
Example #3 — [combine h : m : s . ms]
  fan-in:
    in0 ← [number]
    in0 ← [button]
    in2 ← [number]
    in4 ← [number]
    in6 ← [flonum]
    in6 ← [message "foo"]
    in6 ← [number]
  fan-out:
    out0 → [message "6:9:5.12"]:in1
```

### basic

> only first inlet triggers output
>
> @triggers attribute: list of inputs that will trigger output. Inlet numbering starts at 0.
>
> @padding attribute: determines the number of zeros to add to a number if appropriate.

```
Example #1 — [combine sound 74 @triggers 1]
  fan-in:
    in1 ← [number]    # changes second argument and triggers output
  fan-out:
    out0 → [message "sound74"]:in1
```

```
Example #2 — [combine frog :/ Users/Shared / filename - version . txt @triggers 4 6 @padding 0 0 0 0 0 0 2]
  fan-in:
    in0 ← [button]
    in4 ← [message "pizza"]
    in4 ← [message "bar"]
    in4 ← [message "foo"]
    in6 ← [number]
  fan-out:
    out0 → [message "frog:/Users/Shared/pizza-66.txt"]:in1
```

```
Example #3 — [combine drivename :/ foldername / filename - version . extension]
  fan-in:
    in0 ← [message "tadpole :/ Users/polywog / bar"]
    in0 ← [button]
    in0 ← [message "frog"]
    in2 ← [message "Users/Shared"]
    in4 ← [message "foo"]
    in4 ← [message "anotherfile - 360 . html"]
    in6 ← [number]
    in8 ← [message "txt"]
  fan-out:
    out0 → [message "tadpole:/Users/polywog/bar-360.html"]:in1
```

## See also

`join`, `pack`, `pak`, `sprintf`, `transport`
