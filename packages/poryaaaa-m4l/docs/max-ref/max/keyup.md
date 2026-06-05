# keyup

_max · Interaction_

> Report key information on release

Tracks and outputs the key-codes, ASCII values, and modifier-key values of key releases on the computer keyboard.

## Inlets / Outlets

| port | meaning |
|------|---------|
| out0 | ASCII Code of Key Released |
| out1 | Platform-Specific Keyboard Code of Key Released |
| out2 | Modifier Keys of Key Released |
| out3 | Platform-Independent Keyboard Code of Key Released |

## GUI behaviors

- `(keyboard)` — Output keystroke information
  The input to keyup comes directly from the computer keyboard. There are no inlets.

## Attributes

- `@documentable` (int)

## Help patcher examples

### choosing keys

```
Example — [keyup]
  fan-out:
    out0 → [select 32]:in0    # Search for the space bar (ASCII 32)
```

### basic

```
Example — [keyup]
  fan-out:
    out0 → [number]:in0    # ASCII value
    out1 → [number]:in0    # Platform-Specifc key code value
    out2 → [number]:in0    # Modifier keys down when the key was released
    out3 → [number]:in0    # Platform-Independent key code value
```

## See also

`atoi`, `hi`, `itoa`, `key`, `mousestate`, `numkey`, `spell`, `sprintf`
