# live.tab

_m4l · Live UI Objects_

> A user interface tab/multiple button object in the style of Ableton Live.

live.tab is used to create multiple-button and multi-column displays and interfaces.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Messages in |
| out0 | Item Index |
| out1 | Item Symbol |
| out2 | Parameter Raw Value (0.-1.) |

## Messages

- `bang` — Output the current item
  Sends the current item out the outlets.
- `int(input: int)` — Display a tab item and send the current item
  The number specifies a tab item to be sent out, and causes live.tab to display that item. The items are numbered starting at 0.
- `float(input: float)` — Display a tab item and send the current item
  The number specifies a tab item to be sent out, and causes live.tab to display that item. The items are numbered starting at 0.
- `assign(assign-input: float)` — Display and output a value
  The word assign, followed by a floating point value, causes that value to be displayed and sent out the live.tab object's outlet. The value, however, will not be stored. If the Parameter Visibility attribute is set to Stored Only, the assign message will not add the new value to the Live application’s undo chain.
- `init` — Restore and output the initial value
- `outputvalue` — Output the current value
  Sends the current value out the outlet.
- `rawfloat(input: float)` — Store a raw normalized value, convert to real, display, and output
  A raw normalized value (between 0. and 1.) received in the inlet is converted to a real value, stored, displayed by live.tab, and the current value is sent out the outlet.
- `set(tab index: float)` — Display a menu item without triggering output
  The word set, followed by a number, specifies a menu item to be displayed by live.tab without triggering any output.
- `setsymbol(tab item: list)` — Display a tab item without triggering output
  The word setsymbol, followed by a symbol that specifies a tab item, causes live.tab to display that item, but does not cause any output.
- `symbol(tab item: list)` — Display a tab item and report output
  The word symbol, followed by a symbol that specifies a tab item, causes live.tab to display that item and send the tab text out the second outlet and the index out the first outlet.

## GUI behaviors

- `(mouse)` — Click to set a tab selection
  Clicking on a tab button will highlight and set the selection and send the tab text out the second outlet and the index out the first outlet.

## Attributes

- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)
- `@paint` (int)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `active` — seen as: `active $1`

## Help patcher examples

### appearance

```
Example — [live.tab] (live.drop)
  fan-in:
    in0 ← [attrui @textcolor]
    in0 ← [attrui @activebgoncolor]
    in0 ← [attrui @bgoncolor]
    in0 ← [attrui @bordercolor]
    in0 ← [attrui @focusbordercolor]
    in0 ← [attrui @livemode]
    in0 ← [attrui @active]    # Click here and click on the button to see the active color
    in0 ← [attrui @activebgcolor]
    in0 ← [attrui @bgcolor]
    in0 ← [attrui @appearance]
```

Attributes demonstrated: `@active`, `@activebgcolor`, `@activebgoncolor`, `@appearance`, `@bgcolor`, `@bgoncolor`, `@bordercolor`, `@focusbordercolor`, `@livemode`, `@textcolor`

### pictures

```
Example — [live.tab] (live.toggle)
  fan-in:
    in0 ← [p list] ← [umenu] ← [p list]    # p list emits: "pictures $1 $2" / list inbuilt media files for Max for Live Devices / p list emits: "pictures $1 $2"
```

```
Example #2 — [live.tab]
  fan-in:
    in0 ← [attrui @usesvgviewbox]    # use the SVG file "viewbox" to set the layout
    in0 ← [attrui @usepicture]    # use pictures instead of text
```

```
Example — [live.tab] (live.toggle)
  fan-in:
    in0 ← [attrui @remapsvgcolors]
    in0 ← [attrui @textcolor]    # use object color attributes to remap svg colors
    in0 ← [attrui @textoncolor]
```

```
Example #4 — [live.tab]
  fan-in:
    in0 ← [attrui @usesvgviewbox]    # use the SVG file "viewbox" to set the layout
    in0 ← [attrui @usepicture]    # use pictures instead of text
```

```
Example #5 — [live.tab]
  fan-in:
    in0 ← [attrui @usesvgviewbox]    # use the SVG file "viewbox" to set the layout
    in0 ← [attrui @usepicture]    # use pictures instead of text
```

Attributes demonstrated: `@remapsvgcolors`, `@textcolor`, `@textoncolor`, `@usepicture`, `@usesvgviewbox`

### basic

```
Example #1 — [live.tab]  LCD mode
  fan-in:
    in0 ← [attrui @lcdcolor]
    in0 ← [attrui @lcdbgcolor]
```

```
Example #2 — [live.tab]  proportional spacing
  (no patch cords)
```

```
Example #3 — [live.tab]  equal spacing / Spacing:
  (no patch cords)
```

```
Example #4 — [live.tab]  multilines mode
  (no patch cords)
```

```
Example #5 — [live.tab]  equal spacing
  fan-in:
    in0 ← [message "active $1"]
    in0 ← [number]
  fan-out:
    out0 → [message ""]:in1
    out1 → [message ""]:in1
    out2 → [flonum]:in0
```

Attributes demonstrated: `@lcdbgcolor`, `@lcdcolor`

## See also

`live.text`, `live.toggle`, `tab`
