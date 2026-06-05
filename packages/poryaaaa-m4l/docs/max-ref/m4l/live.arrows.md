# live.arrows

_m4l · Live UI Objects_

> Vectorized arrow(s) user interface object

live.arrows displays a variable number of directional arrow buttons.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Message In (colors...) |
| out0 | Selected Arrow Symbol |

## Messages

- `int(input: int)` — Select an arrow and send a message
  Sending an integer value to the live.arrows object will cause the displayed arrow whose number corresponds to those arrow(w) currently displayed to flash and will send a corresponding message out the live.arrows object's outlet. Index numbering for arrows starts at 0 for the leftmost arrow. The indexing range will vary depending on the number of arrows displayed (using the downarrow, leftarrow, rightarrow, and uparrow attributes).
- `down` — Flash the down arrow and send a message
  When the down message is received, the down arrow will flash and the message down will be sent to the live.arrows object's outlet if the downarrow attribute is set to 1.
- `left` — Flash the left arrow and send a message
  When the left message is received, the left arrow will flash and the message left will be sent to the live.arrows object's outlet if the leftarrow attribute is set to 1.
- `right` — Flash the right arrow and send a message
  When the right message is received, the right arrow will flash and the message right will be sent to the live.arrows object's outlet if the rightarrow attribute is set to 1.
- `up` — Flash the up arrow and send a message
  When the up message is received, the up arrow will flash and the message up will be sent to the live.arrows object's outlet if the uparrow attribute is set to 1.

## GUI behaviors

- `(mouse)` — Click to output the corresponding arrow symbol

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
Example — [live.arrows]
  fan-in:
    in0 ← [attrui @arrowcolor]
    in0 ← [attrui @blinkcolor]
    in0 ← [attrui @bordercolor]
```

Attributes demonstrated: `@arrowcolor`, `@blinkcolor`, `@bordercolor`

### basic

```
Example #1 — [live.arrows]  You can change the number of visible arrows in the inspector:
  (no patch cords)
```

```
Example #2 — [live.arrows]
  fan-in:
    in0 ← [message "up"]
    in0 ← [message "left"]
    in0 ← [message "down"]
    in0 ← [message "right"]
    in0 ← [live.numbox]
  fan-out:
    out0 → [message "right"]:in1
```

## See also

`live.button`
