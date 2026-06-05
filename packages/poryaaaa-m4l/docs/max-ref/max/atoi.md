# atoi

_max · Lists_

> Convert characters to integers

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | — | Any message, bang triggers output |
| in1 | — | Appends message to current contents |
| in2 | — | Any message sets current contents |
| out0 | list | List of ints output |

## Messages

- `bang` — Perform the most recent conversion
  In left inlet: a bang message can be used to trigger the output of the currently stored numerical list. A bang in the right two inlets is treated as a symbol.
- `int(input: int)` — Convert a number to character code representation
  In left inlet: The character code value of each of the digits of the number is stored internally and sent out the outlet as a list.
- `float(input: float)` — Convert a number to character code representation
  In left inlet: The character code value of each of the digits of the number is stored internally and sent out the outlet as a list.
- `list(input: list)` — Convert a list of values
  Each int in the list is converted to a character code as described above, and a space character (ASCII value 32) is inserted between items in the list. The middle inlet is used to append to the currently stored list, and the right inlet will set the contents of the internally stored list, without causing output.
- `anything(input: list)` — Convert characters into numbers
  In left inlet: The character code value of each letter, digit, or other character in the symbol is stored internally and sent out the outlet as a list.
  In middle inlet: The character code value of each letter, digit, or other character in the symbol is appended to the currently stored list. No output is triggered.
  In right inlet: The character code value of each letter, digit, or other character in the symbol is stored internally, replacing the previously stored list, but not output.
- `clear` — Clear stored data
  In left inlet: The clear message is used to clear the contents of the internally-stored numerical list. The word clear in the right two inlets is treated as a symbol.

## Attributes

- `@basic` (int)
- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `Gödel` — seen as: `Gödel`
- `alphabet` — seen as: `alphabet`
- `hot` — seen as: `hot 5`
- `three` — seen as: `three and 1.05`
- `vision` — seen as: `vision`

## Help patcher examples

### basic

```
Example #1 — [atoi @utf8 1]  set utf8 output mode
  fan-in:
    in0 ← [message "Gödel"]
  fan-out:
    out0 → [message "71 195 182 100 101 108"]:in1    # UTF-8 Unicode output
```

```
Example #2 — [atoi]
  fan-in:
    in0 ← [message "vision"]
    in0 ← [number]
    in0 ← [flonum]
    in0 ← [button]    # output list
    in0 ← [message "clear"]    # clear list
    in0 ← [message "72 trombones"]    # set and output
    in1 ← [message "hot 5"]    # append to list with no output
    in1 ← [message "Gödel"]
    in1 ← [flonum]
    in1 ← [number]
    in2 ← [message "three and 1.05"]    # set with no output
    in2 ← [message "alphabet"]
    in2 ← [flonum]
    in2 ← [number]
  fan-out:
    out0 → [message "71 246 100 101 108"]:in1    # UTF-32 Unicode output
```

## See also

`itoa`, `key`, `keyup`, `message`, `regexp`, `spell`, `sprintf`
