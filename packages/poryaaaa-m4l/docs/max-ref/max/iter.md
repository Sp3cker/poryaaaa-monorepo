# iter

_max · Lists_

> Break a list into individual messages

Unpacks and outputs list contents one element at a time.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | list to be Unraveled |
| out0 | Sequential Output of Incoming list |

## Messages

- `bang` — Output the most recent values
  Sends the number or list most recently received, in sequential order.
- `int(input: int)` — Output as a single message
  The number is sent out the outlet.
- `float(input: float)` — Output as a single message
  The number is sent out the outlet.
- `list(input: list)` — Output list elements as individual elements
  The numbers in the list are sent out the outlet in sequential order.
- `anything(input: list)` — Output list elements as individual elements
  See the list entry.

## Help patcher examples

### basic

```
Example #1 — [iter]
  fan-in:
    in0 ← [message "99 98 97 96 95"]
  fan-out:
    out0 → [print]:in0    # Watch the Max window
```

```
Example #2 — [iter]
  fan-in:
    in0 ← [message "60 63 67"]    # Can be used to play chords from lists
  fan-out:
    out0 → [makenote 64 500]:in0
```

## See also

`cycle`, `thresh`, `unjoin`, `unpack`, `zl`
