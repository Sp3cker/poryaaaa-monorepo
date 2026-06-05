# live.text

_m4l · Live UI Objects_

> A user interface button/toggle

live.text object is a user interface object used to create buttons and toggles.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Messages in |
| out0 | Item Index |
| out1 | Item Symbol |

## Messages

- `bang` — A bang message will toggle the state of the object.
  A bang message will toggle the state of the object. If it is off, it will switch on and output a 1. If it is on, it will switch off and output a 0.
- `int(input: int)` — Toggle the button, send its text and a 0/1 message
  In the toggle mode, any non-zero number will toggle the button to the "on" position, send the button text out the middle outlet and send a 1 out the left outlet. A zero sets the toggle to the "off" position, sends the button text out the middle outlet and sends a 0 out the left outlet.
- `float(input: float)` — Toggle the button, send its text and a 0/1 message
  Converted to int.
- `assign(assign-input: float)` — Display and output a value
  The word assign, followed by a floating point value, causes that value to be displayed and sent out the live.text object's outlet. The value, however, will not be stored. If the Parameter Visibility attribute is set to Stored Only, the assign message will not add the new value to the Live application’s undo chain.
- `init` — Restore and output the initial value
- `outputvalue` — Send the current value out the outlet
- `rawfloat(input-value: float)` — A raw normalized value (between 0. and 1.)
  A raw normalized value (between 0. and 1.) received in the inlet is converted to a real value, and then functions like any other received int value in toggle mode.
- `set(set-input: float)` — Toggle the state without sending output
  In the toggle mode, the set messages toggles the "on" or "off" state without sending anything out the outlets. The word set, followed by any non-zero number, sets toggle to on. The message set 0 sets it to "off".
- `setsymbol(button-text-item: list)` — Choose a button by name and toggle it without causing output
  In the toggle mode, the word setsymbol, followed by a symbol that specifies a button text item, causes live.text to display that symbol and act as though the object were toggled to that state.
- `symbol(button-text-item: list)` — Choose a button by name and toggle it
  In the toggle mode, the word symbol, followed by a symbol that specifies a button text item, causes live.text to display that symbol and sends the current values out the outlets.

## GUI behaviors

- `(mouse)` — Highlight the text, send the text and a bang
  In button mode, a mouse click on live.text highlights it for as long as the mouse is held down, sending the text out the second outlet and a bang message out the left outlet.
  In toggle mode, a mouse click behaves the same as a live.toggle. When the mouse is clicked, the live.text object will send a 1 out the left outlet if the cursor is inside of the live.text object's rectangle, and a 0 if it is not. The button text is also sent out the second outlet on mouse click.

## Attributes

- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)
- `@paint` (int)
- `@style` (symbol)

## Help patcher examples

### appearance

```
Example #1 — [live.text]
  fan-in:
    in0 ← [attrui @focusbordercolor]
    in0 ← [attrui @textcolor]
    in0 ← [attrui @activetextcolor]
    in0 ← [attrui @activetextoncolor]
    in0 ← [attrui @activebgcolor]    # enable to see the active color
    in0 ← [attrui @bgcolor]
    in0 ← [attrui @active]
    in0 ← [attrui @activebgoncolor]
    in0 ← [attrui @bordercolor]
```

```
Example #2 — [live.text]
  fan-in:
    in0 ← [attrui @appearance]    # LCD mode offers a simplified set of color attributes
    in0 ← [attrui @focusbordercolor]
    in0 ← [attrui @lcdcolor]
    in0 ← [attrui @lcdbgcolor]
    in0 ← [attrui @inactivelcdcolor]
    in0 ← [attrui @active]
```

Attributes demonstrated: `@active`, `@activebgcolor`, `@activebgoncolor`, `@activetextcolor`, `@activetextoncolor`, `@appearance`, `@bgcolor`, `@bordercolor`, `@focusbordercolor`, `@inactivelcdcolor`, `@lcdbgcolor`, `@lcdcolor`, `@textcolor`

### pictures

```
Example #1 — [live.text]
  fan-in:
    in0 ← [p list] ← [umenu] ← [p list]    # p list emits: "pictures $1 $2" / list inbuilt media files for Max for Live Devices / p list emits: "pictures $1 $2"
```

```
Example #2 — [live.text]
  fan-in:
    in0 ← [attrui @usesvgviewbox]    # Use the viewbox supplied by the svg
```

```
Example #3 — [live.text]
  fan-in:
    in0 ← [attrui @remapsvgcolors]    # Turn this on to remap the colors of the svg to the text colors used by the live.text object
    in0 ← [attrui @activetextcolor]
    in0 ← [attrui @activetextoncolor]
```

Attributes demonstrated: `@activetextcolor`, `@activetextoncolor`, `@remapsvgcolors`, `@usesvgviewbox`

### basic

```
Example #1 — [live.text LCD On]
  fan-in:
    in0 ← [toggle]    # LCD appearance:
  fan-out:
    out0 → [number]:in0
    out1 → [message ""]:in1
```

```
Example #2 — [live.text]
  fan-out:
    out0 → [button]:in0
```

```
Example #3 — [live.text Normal]
  fan-in:
    in0 ← [attrui @active]
  fan-out:
    out0 → [number]:in0
    out1 → [message ""]:in1
```

```
Example — [live.text FILTER] (FILTER)
  fan-in:
    in0 ← [toggle]    # label appearance:
  fan-out:
    out0 → [number]:in0
    out1 → [message ""]:in1
```

```
Example — [live.text On] (1)
  fan-in:
    in0 ← [attrui @active]
    in0 ← [toggle]
  fan-out:
    out0 → [number]:in0
    out1 → [message ""]:in1
```

Attributes demonstrated: `@active`

## See also

`live.button`, `live.tab`, `live.toggle`, `textbutton`
