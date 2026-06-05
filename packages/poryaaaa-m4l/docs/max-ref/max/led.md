# led

_max · U/I_

> Color on/off button

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | int Sets LED, bang Reverses It |
| out0 | Outputs 1 or 0 When LED is Changed or Set |

## Messages

- `bang` — Flash display and cause output
  Flashes led on and off quickly, and outputs 0.
  Clicking on an led toggles it back and forth between bright and dark, outputting 1 and 0.
- `int(input: int)` — Set on/off state and cause output
  If the number is 0, led shows its darkened state, and outputs 0. If the number is not 0, led shows its brightened state and outputs 1.
- `float(input: float)` — Set on/off state and cause output
  Converted to int.
- `pict(color: int)` — Set graphic color
  In left inlet: the word pict, followed by an integer, changes the color used by led.
- `set(on/off-flag (0 or non-zero): int)` — Set on/off state with no output
  The word set, followed by a non-zero number causes led to show its brightened state, but causes no output; set 0 shows the led object in a darkened state, but causes no output.
- `toggle` — Toggle on/off state and cause output
  Switches the led from dark to bright and sends 1 out the outlet; or vice-versa, from bright to dark, sending 0 out the outlet.

## GUI behaviors

- `(mouse)` — Toggle on/off state and cause output
  Clicking on an led object toggles it back and forth between bright and dark, outputting 1 and 0.

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
Example — [led]  Click to see the color
  fan-in:
    in0 ← [attrui @oncolor]
    in0 ← [attrui @style]    # Create and apply styles using the format palette or type a style name here
    in0 ← [attrui @bgcolor]
    in0 ← [toggle]
```

Attributes demonstrated: `@bgcolor`, `@oncolor`, `@style`

### basic

```
Example #1 — [led]
  fan-in:
    in0 ← [led]
```

```
Example #2 — [led]
  fan-in:
    in0 ← [message "11"]    # any input other than zero will turn on the led
    in0 ← [message "1"]
    in0 ← [message "0"]
    in0 ← [message "-1"]
    in0 ← [toggle]
    in0 ← [attrui @blinktime]    # change the length of the blink
  fan-out:
    out0 → [number]:in0
    out0 → [led]:in0
    out0 → [toggle]:in0
```

Attributes demonstrated: `@blinktime`

## See also

`button`, `pictctrl`, `togedge`, `toggle`
