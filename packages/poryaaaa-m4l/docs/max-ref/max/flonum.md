# flonum

_max · U/I_

> Display and output a number

Display, input, and output floating-point numbers in a number box.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Set Displayed Number and Repeat to Output |
| out0 | Output Incoming or Entered Number |
| out1 | bang When Tab Key Pressed |

## Messages

- `bang` — Output the current value
  Sends the currently displayed number out the outlet.
- `int(input: int)` — Convert, store, display and output value
  The number received in the inlet is stored and displayed in the number box and sent out the outlet.
- `float(input: float)` — Store, display and output value
  The number received in the inlet is stored and displayed in the number box and sent out the outlet.
- `max(maximum-value: list)` — Set the maximum value limit
  The word max, followed by a number, sets the maximum value that can be displayed or sent out by the number box. The word max by itself sets the maximum to None (removes a prior maximum value constraint).
- `min(minimum: list)` — Set the minimum value limit
  The word min, followed by a number, sets the minimum value that can be displayed or sent out by the number box. The word min by itself sets the minimum to None (removes a prior minimum value constraint).
- `select` — Select for keyboard input
  The word select will make the number box active so that you can type numbers straight into it (click on any empty space in a locked patcher to deselect it).
- `set(input: int)` — Store and display value with no output
  The word set, followed by a number, sets the stored and displayed value to that number without triggering output.

## GUI behaviors

- `(mouse)` — Edit and output the float value
  Clicking and dragging up and down on the number box with the mouse (when the patcher window is locked) moves the displayed value up and down, and outputs the new values continuously.
  In the float number box, dragging to the left of the decimal point changes the value in increments of 1. Dragging to the right of the decimal point changes the fractional part of the number in increments of 0.01.
  When the active patcher window is locked, numbers can be entered into a number box by clicking on it with the mouse and typing in a number on the computer keyboard. Typing the Return or Enter keys on Macintosh or the Enter key on Windows, or clicking outside the number box, sends the number out the outlet.

## Attributes

- `@basic` (int)
- `@category` (atom)
- `@defaultname` (float, size 4)
- `@label` (symbol)
- `@paint` (int)
- `@save` (int)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — bang When Tab Key Pressed

### appearance

```
Example — [flonum]
  fan-in:
    in0 ← [attrui @bgcolor]    # change background color
    in0 ← [attrui @bordercolor]    # change border color
    in0 ← [attrui @textcolor]    # change text color
    in0 ← [attrui @tricolor]    # change triangle color
    in0 ← [attrui @hbgcolor]    # change highlighted background color
    in0 ← [attrui @htextcolor]    # change highlighted text color
    in0 ← [attrui @htricolor]    # change highlighted triangle color
```

Attributes demonstrated: `@bgcolor`, `@bordercolor`, `@hbgcolor`, `@htextcolor`, `@htricolor`, `@textcolor`, `@tricolor`

### basic

```
Example #1 — [flonum]  click and drag after sending min and/or max messages
  fan-in:
    in0 ← [message "min 10"]    # impose limits
    in0 ← [message "max 100"]
    in0 ← [message "min"]    # remove limits
    in0 ← [message "max"]
  fan-out:
    out0 → [number]:in0
```

```
Example #2 — [flonum]
  fan-in:
    in0 ← [message "set 25"]    # click on messages
    in0 ← [message "42.5"]
    in0 ← [message "37"]
    in0 ← [message "21"]
  fan-out:
    out0 → [print @popup 1]:in0
```

## See also

`float`, `int`
