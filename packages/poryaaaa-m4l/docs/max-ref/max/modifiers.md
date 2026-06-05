# modifiers

_max · Interaction_

> Report modifier key presses

Polls and reports the state of the keyboard's modifier keys.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | bang Triggers Modifier Info |
| out0 | 1 if Shift Key Down, 0 if Up |
| out1 | 1 if Caps Lock Down, 0 if Up |
| out2 | 1 if Option/Alt Key Down, 0 if Up |
| out3 | 1 if Ctrl Key Down, 0 if Up |
| out4 | 1 if Command/Fn Key Down, 0 if Up |

## Arguments

- **rate** (`int`) _(optional)_ — Polling rate
  Specifies a polling rate in milliseconds. The default value is 0 (no polling).

## Messages

- `bang` — Output modifier key states
  Sends out a report of the current modifier key states.
- `interval(rate: int)` — Set the polling rate
  The word interval followed by a number, specifies the rate, in milliseconds, used when polling the state of the modifier keys. A value of zero disables polling.

## GUI behaviors

- `(keyboard)` — Update modifier values
  The keyboard input to modifiers comes directly from the computer keyboard.

## Attributes

- `@documentable` (int)

## Help patcher examples

### basic

```
Example — [modifiers]
  fan-in:
    in0 ← [button]    # bang to report
    in0 ← [message "interval 100"]    # Set the polling interval in ms. zero = no polling (default)
    in0 ← [message "interval 0"]
  fan-out:
    out0 → [toggle]:in0    # Shift
    out1 → [toggle]:in0    # Caps Lock
    out2 → [toggle]:in0    # Mac Option / Win Alt
    out3 → [toggle]:in0    # Mac Control / Win Right Click
    out4 → [toggle]:in0    # Mac Cmd / Win Ctrl
```

## See also

`key`, `keyup`, `modifiers`, `numkey`
