# live.toggle

_m4l · Live UI Objects_

> Switch between off and on (0/1)

live.toggle sends a 0 as output when it is turned off and a 1 as output when it is turned on (when giving input, a non-zero number will turn it on, a 0 will turn it off, and a bang will alternate the state of the toggle).

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | — | int Sets Toggle, bang Reverses It |
| out0 | int | Output 1 or 0 When Toggle Is Set |

## Messages

- `bang` — Alternate toggle state
  Switches live.toggle on if it is off; switches it off if it is on.
- `int(input: int)` — Switch toggle and pass through number
  The number is sent out the outlet. If the number is not 0, live.toggle displays an X, showing it is on. If it is 0, live.toggle is blank, showing it is off.
- `float(input: float)` — Converted to int. See int listing.
- `assign(assign-input: float)` — Display and output a value
  The word assign, followed by a floating point value, causes that value to be displayed and sent out the live.toggle object's outlet. The value, however, will not be stored. If the Parameter Visibility attribute is set to Stored Only, the assign message will not add the new value to the Live application’s undo chain.
- `init` — Restore and output the initial value
  Restores and outputs the initial value.
- `outputvalue` — Send the current value out the outlet
- `set(set-input: int)` — Switch toggle state without output
  Switches the live.toggle on or off without sending anything out the outlet. The word set, followed by any non-zero number, sets toggle to on; set 0 sets it to off.

## GUI behaviors

- `(mouse)` — Switch toggle state
  A mouse click on live.toggle switches the object on if it is off and off if it is on.

## Attributes

- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)
- `@paint` (int)
- `@style` (symbol)

## Help patcher examples

### appearance

```
Example — [live.toggle]
  fan-in:
    in0 ← [attrui @bgcolor]
    in0 ← [attrui @bordercolor]    # change border color
    in0 ← [attrui @activebgcolor]
    in0 ← [attrui @activebgoncolor]
    in0 ← [attrui @bgoncolor]
    in0 ← [attrui @focusbordercolor]    # change border color for focused live.toggle object
    in0 ← [attrui @active]    # Click here and click on the button to see the active color
```

Attributes demonstrated: `@active`, `@activebgcolor`, `@activebgoncolor`, `@bgcolor`, `@bgoncolor`, `@bordercolor`, `@focusbordercolor`

### basic

```
Example — [live.toggle]
  fan-in:
    in0 ← [message "set 0"]    # non-zero turns on and outputs '1'
    in0 ← [message "-27"]    # 1 to turn on
    in0 ← [message "0"]
    in0 ← [message "bang"]    # reverse current value and output new value
    in0 ← [message "1"]    # 0 to turn off
    in0 ← [message "set 1"]    # turn on without output / turn off without output
  fan-out:
    out0 → [live.numbox]:in0
    out0 → [button]:in0
    out0 → [print @popup 1]:in0
```

## See also

`led`, `live.tab`, `matrixctrl`, `pictctrl`, `radiogroup`, `live.text`, `togedge`, `toggle`, `ubutton`
