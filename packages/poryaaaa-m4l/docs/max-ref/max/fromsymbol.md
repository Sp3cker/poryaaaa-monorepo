# fromsymbol

_max · Messages_

> Convert a symbol into numbers/messages

fromsymbol will take the individual characters in a symbol and convert them from a symbol back to numbers/messages.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Symbol Input to be Analyzed |
| out0 | Input Converted from Symbol |

## Messages

- `bang` — Convert bang to message
  The word bang sent as a part of a symbol will be converted to a message. The message bang will simply pass through to the output.
- `int(value: int)` — Pass integer to output
  Any integer will simply pass through to the output.
- `float(value: float)` — Pass float to output
  Any float will simply pass through to the output.
- `anything(any symbol: list)` — Convert symbol to numbers/messages
  Any symbol will be converted to numbers/messages.

## Attributes

- `@basic` (int)
- `@default` (symbol)
- `@label` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `"-23.6` — seen as: `"-23.6 4.56 0.01"`
- `"3` — seen as: `"3 more characters"`
- `/Users/Documents/misc` — seen as: `/Users/Documents/misc`
- `/my_device/param/osc/42` — seen as: `/my_device/param/osc/42`
- `e0:91:f5:97:1a:b5` — seen as: `e0:91:f5:97:1a:b5`
- `separator` — seen as: `separator .`, `separator :`
- `toto/titi/tutu` — seen as: `toto/titi/tutu`

## Help patcher examples

### separator

> Define the separator character or symbol to be used when converting the symbol to a message. The default separator is a space.

```
Example #1 — [fromsymbol]
  fan-in:
    in0 ← [message "separator :"]
    in0 ← [message "separator ."]    # set separator
    in0 ← [message "127.12.3.01"]    # send symbol
    in0 ← [message "e0:91:f5:97:1a:b5"]
  fan-out:
    out0 → [message ""]:in1
```

```
Example #2 — [fromsymbol @separator /]
  fan-in:
    in0 ← [message "/Users/Documents/misc"]
    in0 ← [message "toto/titi/tutu"]
    in0 ← [message "/my_device/param/osc/42"]
  fan-out:
    out0 → [message ""]:in1
```

### basic

```
Example #1 — [fromsymbol]
  fan-in:
    in0 ← [message ""3 more characters""]    # works with any symbol
  fan-out:
    out0 → [unpack i s s]:in0
```

```
Example #2 — [fromsymbol]
  fan-in:
    in0 ← [message ""-23.6 4.56 0.01""]    # turn a symbol into a list
  fan-out:
    out0 → [unpack f f f]:in0
```

## See also

`regexp`, `sprintf`, `tosymbol`, `zl`
