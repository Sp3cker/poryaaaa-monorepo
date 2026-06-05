# hint

_max · U/I_

> Display hint text

When you mouse over a hint, you'll see a message appear on the screen below the area defined by the hint. The hint object has a number of messages you can use to change its appearance.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | 1 Enables Hint, 0 Disables |

## Messages

- `int(enable: int)` — Enable or disable text display
  0 will disable any hinting while a non-zero number will enable it.
- `float(enable: float)` — Enable or disable text display
  Converted to int.
- `set(message: symbol)` — Replace the hint text
  The word set, followed by any message, will replace the message stored in hint. This message will be displayed when the mouse is positioned over the hint object after an interval of time specified by the delay message.

## GUI behaviors

- `(mouse)` — Display hint text
  When the cursor moves within the hint object's rectangle, its text message will appear in a colored area beneath the rectangle after the specified delay.

## Attributes

- `@documentable` (int)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `delay` — seen as: `delay 0`, `delay 500`

## Help patcher examples

### basic

> The set message changes the text. All text must be contained in a single symbol argument. You can also select the object and type the text into an info dialog.

```
Example #1 — [hint]
  fan-in:
    in0 ← [toggle]
    in0 ← [message "delay 500"]    # The delay message sets the delay in milliseconds until the hint appears--the default is 1000.
    in0 ← [message "delay 0"]
    in0 ← [message "set "It's bigger than a bread box""]
    in0 ← [message "set "It's blue and green""]
```

```
Example #2 — [hint]
  (no patch cords)
```

## See also

`comment`, `umenu`
