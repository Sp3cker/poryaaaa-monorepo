# number

_max · U/I_

> Display and output numbers, lists, and messages

The number object displays and outputs either integers, floats, lists of numbers, or any message, depending on its format. flonum is a synonym for number in Float format, listbox is a synonym for number in List format, and textbox is a synonym for number in Text format.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Set Displayed Input and Repeat to Output |
| out0 | Output Incoming or Entered Values |
| out1 | bang When Tab Key Pressed |

### Port details

**`out1` (bang When Tab Key Pressed):** Use this outlet to trigger a select message to another number box to create an interface for tabbing between objects.

## Messages

- `bang` — Output the stored value
  Sends the currently displayed value out the outlet
- `int(input: int)` — Display and output the value
  An int value received will be displayed and sent out the outlet. The value will be converted to a float if the number box's format attribute is set to Float.
- `float(input: float)` — Display and output the value
  float value received will be displayed and sent out the outlet. The value will be converted to an int if the number box's format attribute is set to Int or another integer-based format.
- `list(ARG_NAME_0: list)` — Display and output the value(s)
  list received is displayed and sent out the outlet. If the format is set to List or Text, the entire list is stored and output. Otherwise only the first value in the list is stored and output.
- `anything(ARG_NAME_0: list)` — Display and output the value(s)
  If any message is received and the number box's format is set to Text, the entire message is stored and output. If the format is set to Int or Float, an error message is posted to the Max console and no value is stored or output. If the format is set to List, no value is stored or output, but no error message is posted.
- `clear` — Clear list or message
  When the format atrribute of number is set to List or Text, clear will clear the current list or message displayed.
- `max(maximum: list)` — Set the maximum allowed value
  The word max, followed by a number, sets the maximum value that can be displayed or sent out by the number box. The word max by itself sets the maximum to None, removing any previously set maximum value.
- `min(minimum: list)` — Set the minimum allowed value
  The word min, followed by a number, sets the minimum value that can be displayed or sent out by the number box. The word min by itself sets the minimum to None, removing any previously set minimum value.
- `select` — Select the object for entry
  The word select will highlight the number box so you can type values into it. Clicking on any empty space in a locked patcher will deselect the object.
- `set(input: int)` — Set the value with no output
  The word set, followed by a number, list or message, sets the stored and displayed value to that number without triggering output.

## GUI behaviors

- `(mouse)` — Edit and output the value
  Clicking and dragging up and down on a number box with the mouse (when the patcher window is locked) moves the displayed value up and down, and outputs the new values continuously.
  In a float number box, dragging to the left of the decimal point changes the value in increments of 1. Dragging to the right of the decimal point changes the fractional part of the number in increments of 0.01.
  When the active patcher window is locked, numbers can be entered into a number box by clicking on it with the mouse and typing in a number on the computer keyboard. Typing the Return or Enter keys on Macintosh or the Enter key on Windows, or clicking outside the number box, sends the number out the outlet. You can also click on the number box and use the up or down arrows on your keyboard to increase or decrease the number by one. Shift+arrow jumps by tens. For the float number box, alt/option+arrow jumps by tenths.
  When the format of the number box is set to List, you can drag up or down on individual numbers within the list to change them.
  When the format of the number box is set to List or Text, you can select individual items in a list or message to replace them by typing, or select the entire contents to replace everything by typing.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)
- `@save` (int)

## Help patcher examples

### format

```
Example #1 — [number]  MIDI (C4)
  fan-in:
    in0 ← [number]    # MIDI
  fan-out:
    out0 → [flonum]:in0    # Decimal (Floating-Point)
```

```
Example #2 — [number]  MIDI
  fan-in:
    in0 ← [number]    # Binary
  fan-out:
    out0 → [number]:in0    # MIDI (C4)
```

```
Example #3 — [number]  Binary
  fan-in:
    in0 ← [number]    # Roland Octal
  fan-out:
    out0 → [number]:in0    # MIDI
```

```
Example #4 — [number]  Roland Octal
  fan-in:
    in0 ← [number]    # Hex
  fan-out:
    out0 → [number]:in0    # Binary
```

```
Example #5 — [number]  Hex
  fan-in:
    in0 ← [number]    # Decimal (Integer)
  fan-out:
    out0 → [number]:in0    # Roland Octal
```

```
Example #6 — [number]  Decimal (Integer)
  fan-out:
    out0 → [number]:in0    # Hex
```

### tab

> use the bang output and the 'select' message to set the keyboard focus on another number box, or trigger some other operation

```
Example #1 — [number]
  fan-in:
    in0 ← [message "select"]
  fan-out:
    out1 → [button]:in0
```

```
Example #2 — [number]
  fan-in:
    in0 ← [message "select"]
  fan-out:
    out1 → [button]:in0
```

```
Example #3 — [number]
  fan-in:
    in0 ← [message "select"]
  fan-out:
    out1 → [button]:in0
```

```
Example #4 — [number]
  fan-in:
    in0 ← [message "select"]
  fan-out:
    out1 → [button]:in0
```

```
Example #5 — [number]  select a number box, then press the tab key to select connected number boxes
  fan-in:
    in0 ← [message "select"]
  fan-out:
    out1 → [button]:in0
```

```
Example #6 — [number]
  fan-in:
    in0 ← [message "select"]
  fan-out:
    out1 → [button]:in0
```

```
Example #7 — [number]
  fan-in:
    in0 ← [message "select"]
  fan-out:
    out1 → [button]:in0
```

### appearance

Attributes demonstrated: `@bgcolor`, `@htricolor`, `@style`, `@textcolor`, `@tricolor`

### basic

```
Example #1 — [number]
  fan-in:
    in0 ← [number]    # click and drag after sending min and/or max messages
```

```
Example #2 — [number]  click and drag after sending min and/or max messages
  fan-in:
    in0 ← [message "min 10"]    # impose limits
    in0 ← [message "max 100"]
    in0 ← [message "min"]    # remove limits
    in0 ← [message "max"]
  fan-out:
    out0 → [number]:in0
```

```
Example #3 — [number]
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
