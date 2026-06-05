# live.button

_m4l · Live UI Objects_

> Flash on any message, send a bang

live.button is used to trigger other messages and processes.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Messages in |
| out0 | Bang for Transition From 0 to 1 |

## Messages

- `bang` — Performs the same function as anything.
- `int(input: int)` — Performs the same function as anything.
- `float(input: float)` — Performs the same function as anything.
- `anything(input: list)` — Flash the button and send a bang message
  When any message is received in the inlet, the button flashes briefly and a bang is sent out the outlet.
- `assign(assign-input: float)` — Display and output a value
  The word assign, followed by a floating point value, causes that value to be displayed and sent out the live.button object's outlet. The value, however, will not be stored. If the Parameter Visibility attribute is set to Stored Only, the assign message will not add the new value to the Live application’s undo chain.

## GUI behaviors

- `(mouse)` — Click to flash the button and send a bang message
  Clicking on the live.button object will cause it to flash briefly and send a bang message out the outlet.

## Attributes

- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)
- `@paint` (int)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `wash` — seen as: `wash the cat`

## Help patcher examples

### appearance

```
Example — [live.button]
  fan-in:
    in0 ← [attrui @bgoncolor]
    in0 ← [attrui @bordercolor]
    in0 ← [attrui @focusbordercolor]
    in0 ← [attrui @activebgcolor]
    in0 ← [attrui @active]    # Click here and click on the button to see the active color
    in0 ← [attrui @activebgoncolor]
    in0 ← [attrui @bgcolor]
```

Attributes demonstrated: `@active`, `@activebgcolor`, `@activebgoncolor`, `@bgcolor`, `@bgoncolor`, `@bordercolor`, `@focusbordercolor`

### basic

```
Example — [live.button]  or click here
  fan-in:
    in0 ← [message "bang"]    # click here
    in0 ← [message "wash the cat"]
    in0 ← [message "1"]
  fan-out:
    out0 → [print]:in0    # and watch the Max window
```

## See also

`button`, `trigger`
