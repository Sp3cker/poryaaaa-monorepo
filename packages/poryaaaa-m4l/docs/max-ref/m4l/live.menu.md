# live.menu

_m4l · Live UI Objects_

> Dropdown menu

The live.menu object can be used to display text associated with incoming numbers and provide a general user interface. Item numbering starts from zero (0).

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Messages in |
| out0 | Item Index |
| out1 | Item Symbol |
| out2 | Parameter Raw Value (0.-1.) |

## Messages

- `bang` — Send the current item out the outlets
- `int(item-index: int)` — Display a menu item and output its index and symbol
  An integer specifies a menu item to be displayed, and causes the live.menu object to display that item and output information about its index and the symbol associated with that menu index. Menu item numbering starts at 0.
- `float(item-index: float)` — Display a menu item and output its index and symbol
  Converted to int.
- `assign(assign-input: float)` — Display and output a value
  The word assign, followed by a floating point value, causes that value to be displayed and sent out the live.menu object's outlet. The value, however, will not be stored. If the Parameter Visibility attribute is set to Stored Only, the assign message will not add the new value to the Live application’s undo chain.
- `init` — Restore and output the initial value
- `outputvalue` — Send the current value out the outlet
- `rawfloat(input-value: float)` — Store a raw normalized value, convert to real, display, and output
  A raw normalized value (between 0. and 1.) received in the inlet is converted to a real value, stored, displayed by live.menu, and the current item is sent out the outlets.
- `set(item index: float)` — Display a menu item without triggering output
  The word set, followed by a number, specifies a menu item to be displayed by the live.menu object without triggering any output.
- `setsymbol(menu item: list)` — Select a menu item for display without triggering output
  The word setsymbol, followed by a message, selects a menu item to be displayed by name without triggering any output.
- `symbol(menu item: list)` — Select a menu item for display by name
  The word symbol, followed by a message, selects a menu item to be displayed by name. If the item is found, the menu item is displayed and information about its index and the symbol associated with that menu index.

## GUI behaviors

- `(mouse)` — Click to select a menu itam
  Clicking with the mouse lets you select a menu item to be displayed and outputs information about its index and the symbol associated with that menu index.

## Attributes

- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)
- `@paint` (int)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `pictures` — seen as: `pictures $1`
- `usepicture` — seen as: `usepicture $1`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out2` — Parameter Raw Value (0.-1.)

### appearance

```
Example — [live.menu] (live.text[5])
  fan-in:
    in0 ← [attrui @hltcolor]
    in0 ← [attrui @textcolor]
    in0 ← [attrui @tricolor]
    in0 ← [attrui @activebgcolor]
    in0 ← [attrui @active]
    in0 ← [attrui @bordercolor]
    in0 ← [attrui @focusbordercolor]
```

Attributes demonstrated: `@active`, `@activebgcolor`, `@bordercolor`, `@focusbordercolor`, `@hltcolor`, `@textcolor`, `@tricolor`

### pictures

```
Example — [live.menu] (live.tab[1])
  fan-in:
    in0 ← [message "pictures $1"]
```

```
Example — [live.menu] (live.tab[1])
  fan-in:
    in0 ← [message "usepicture $1"]
```

### basic

```
Example — [live.menu]
  fan-in:
    in0 ← [live.numbox]
    in0 ← [message "set $1"]
    in0 ← [message "symbol two"]
    in0 ← [message "setsymbol three"]
  fan-out:
    out0 → [live.numbox]:in0
    out0 → [button]:in0
    out1 → [message "one"]:in1
```

## See also

`live.tab`, `umenu`
