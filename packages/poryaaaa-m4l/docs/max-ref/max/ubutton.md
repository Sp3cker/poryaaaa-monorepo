# ubutton

_max · U/I_

> Transparent button

Creates a transparant click-able region that can be placed over graphics or other objects. Produces a bang message when clicked.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Inverts Button and Sends bang out Left Outlet |
| out0 | bang on Mouse-Up or Incoming Message |
| out1 | bang on Mouse-Down |
| out2 | Click Cursor Location |
| out3 | 1 if Mouse Clicked Inside Button, 0 if Outside |

## Messages

- `bang` — Function depends on mode
  The ubutton object can operate in one of two modes. When the ubutton is in button mode (the default mode), it responds to a bang in its inlet by becoming highlighted briefly and sending a bang out its left outlet. When ubutton is in toggle mode, a bang in its inlet causes it to become (and stay) highlighted and send a bang out its right outlet; or, if it is already highlighted, it becomes unhighlighted and sends a bang out its left outlet.
- `int(input: int)` — Highlight the display area
  If ubutton is waiting for a particular number (its Stay-on Value) and the incoming number matches it, the button is highlighted but nothing is sent out. If the incoming number does not match the number that ubutton is waiting for, the button is unhighlighted (or remains that way). If ubutton has a Stay-on Value of 0, int is the same as bang.
- `float(input: float)` — Highlight the display area
  Converted to int.
- `anything([input: list])` — Function depends on mode
  Converted to bang.
- `set(input: int)` — Function depends on mode
  If ubutton is in toggle mode, set 1 sets the ubutton object's toggle (highlights it) and set 0 clears the ubutton object's toggle (unhighlights it). Other integer arguments for set will send the number to ubutton, for comparison to its Stay-on Value, without causing any output.

## GUI behaviors

- `(mouse)` — Bang when clicked
  In "button" mode, a mouse click on ubutton highlights it for as long as the mouse is held down, sending a bang out the right outlet when the mouse button is pressed down, and another bang out the left outlet when the mouse button is released. In "toggle" mode, a mouse click behaves the same as a bang. When the mouse is clicked, ubutton will send a 1 out the right outlet if the cursor is inside of the ubutton object's rectangle, and 0 if it is not. It will also send these messages when the mouse button is released. When the object is in "Track Mouse While Dragging" mode, these messages are sent continuously while the mouse button is held down after a click.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)
- `@save` (int)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `stay` — seen as: `stay 1`, `stay 2`, `stay 3`

## Help patcher examples

### 'stay' message

> ubuttons can receive the "stay" message. After this, when they receive an int, turns the button on if the number is equal to the argument of the stay message, or off it is not equal. You can use the ubutton to highlight a number of alternatives this way. ubuttons that are in "stay mode" do not output a bang when they receive an integer. "stay 0" will clear stay mode.

```
Example #1 — [ubutton]  button set to 4 / button set to 3
  fan-in:
    in0 ← [message "stay 4"]
    in0 ← [number] ← [slider]    # Move the slider and observe how each button behaves
```

```
Example #2 — [ubutton]  button set to 2
  fan-in:
    in0 ← [message "stay 3"]
    in0 ← [number] ← [slider]    # Move the slider and observe how each button behaves
```

```
Example #3 — [ubutton]  button set to 1
  fan-in:
    in0 ← [number] ← [slider]    # Move the slider and observe how each button behaves
    in0 ← [message "stay 2"]
```

```
Example #4 — [ubutton]
  fan-in:
    in0 ← [message "stay 1"]
    in0 ← [number] ← [slider]    # Move the slider and observe how each button behaves
```

```
Example #5 — [ubutton]  normal button
  fan-in:
    in0 ← [number] ← [slider]    # Move the slider and observe how each button behaves
```

### persistence

```
Example #1 — [ubutton]
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [button]:in0
    out1 → [message "1"]:in0
```

```
Example #2 — [ubutton]
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [button]:in0
    out1 → [message "4"]:in0
```

```
Example #3 — [ubutton]
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [button]:in0
    out1 → [message "2"]:in0
```

```
Example #4 — [ubutton]
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [button]:in0
    out1 → [message "8"]:in0
```

```
Example #5 — [ubutton]
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [button]:in0
    out1 → [message "16"]:in0
```

```
Example #6 — [ubutton]
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [button]:in0
    out1 → [message "32"]:in0
```

### toggle

> Sending the message "toggle 1" or using the Inspector to set the Mode attribute makes a ubutton act like a toggle. The ubutton below has toggle mode on. toggle 0 returns to normal mode.

> Click on the button and the message boxes above it. Observe the behavior of the buttons below.

```
Example — [ubutton]
  fan-in:
    in0 ← [message "1"]
    in0 ← [message "0"]
    in0 ← [toggle]
    in0 ← [button]
  fan-out:
    out0 → [button]:in0
    out1 → [button]:in0
```

### basic

```
Example — [ubutton]  click to light up message
  fan-in:
    in0 ← [button]
  fan-out:
    out0 → [print up]:in0
    out1 → [print down]:in0
    out2 → [print loc]:in0
    out3 → [print inside]:in0    # Watch the Max window when you click on the text above
```

## See also

`button`, `fpic`, `led`, `matrixctrl`, `pictctrl`, `radiogroup`, `tab`, `textbutton`
