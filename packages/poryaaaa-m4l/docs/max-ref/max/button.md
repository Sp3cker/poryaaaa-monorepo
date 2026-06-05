# button

_max · U/I_

> Blink and send a bang

button blinks when you send it any message, and it sends out a bang when you click on it.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Cause Indicator to Flash and bang Output |
| out0 | Output Received Message as bang |

## Messages

- `bang` — See the anything listing
- `int(input: int)` — See the anything listing
- `float(input: float)` — See the anything listing
- `list(input: list)` — See the anything listing
- `anything(input: list)` — When any message is received in the inlet, button flashes briefly and a bang is sent out the outlet.

## GUI behaviors

- `(mouse)` — Blink and output a bang
  Clicking on the button object will cause it to blink briefly and send a bang out the outlet.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)
- `@save` (int)

## Help patcher examples

### appearance

```
Example — [button]  click to see the blink color
  fan-in:
    in0 ← [attrui @blinkcolor]
    in0 ← [attrui @outlinecolor]
    in0 ← [attrui @bgcolor]
```

Attributes demonstrated: `@bgcolor`, `@blinkcolor`, `@outlinecolor`, `@style`

### basic

```
Example — [button]  click to send bang
  fan-in:
    in0 ← [message "bang"]
    in0 ← [flonum]
    in0 ← [message "anything at all"]
  fan-out:
    out0 → [print @popup 1]:in0    # double-click in locked patcher to open Max Window
```

## See also

`bangbang`, `loadbang`, `loadmess`, `matrixctrl`, `pictctrl`, `trigger`, `ubutton`
