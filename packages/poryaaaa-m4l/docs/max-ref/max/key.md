# key

_max · Interaction_

> Report keyboard presses

Tracks and outputs the key-codes, ASCII values, and modifier-key values of key presses on the computer keyboard.

## Inlets / Outlets

| port | meaning |
|------|---------|
| out0 | ASCII Code of Key Pressed |
| out1 | Platform-Specific Keyboard Code of Key Pressed |
| out2 | Modifier Keys of Key Pressed |
| out3 | Platform-Independent Keyboard Code of Key Pressed |

### Port details

**`out0` (ASCII Code of Key Pressed):** The key and keyup objects may be used to capture key presses on your computer keyboard. The objects will receive keyboard output only if its patcher window has focus (i.e., it is the topmost window).

 Note to Max for Live users: Given that the key object will only receive output if its patcher window has focus and the many uses that the keyboard already has within the Live application (both assigned keys and user-assignable keys), the use of the key object in Max for Live is strongly discouraged.

## GUI behaviors

- `(keyboard)` — Report keystrokes
  The input to key comes directly from the computer keyboard. There are no inlets.

## Attributes

- `@documentable` (int)

## Help patcher examples

### modifiers

```
Example — [key]
  fan-out:
    out2 → [>> 7]:in0
    out2 → [number]:in0    # flags & 128
```

### basic

> ASCII value

```
Example #1 — [key]
  fan-out:
    out0 → [select 32]:in0
```

```
Example #2 — [key]
  fan-out:
    out0 → [number]:in0
    out1 → [number]:in0
    out2 → [number]:in0
    out3 → [number]:in0    # Platform-Independent key code value / modifier keys down when the key was pressed / Platform-Specifc key code value
```

## See also

`atoi`, `hi`, `itoa`, `keyup`, `modifiers`, `numkey`, `spell`, `sprintf`
