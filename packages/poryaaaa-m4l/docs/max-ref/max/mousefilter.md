# mousefilter

_max · Interaction_

> Gate messages with the mouse

Allows messages to pass only when the mouse button is up (un-clicked).

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Values To Be Filtered |
| out0 | Output If Mouse Button Is Up |

## Messages

- `bang` — Pass the message if the mouse button is up
  If the mouse button is up, the bang is sent out the outlet. Otherwise, the bang is ignored.
- `int(input: int)` — Pass the message if the mouse button is up
  If the mouse button is up, the number is sent out the outlet. Otherwise, the number is ignored.
- `float(input: float)` — Pass the message if the mouse button is up
  If the mouse button is up, the number is sent out the outlet. Otherwise, the number is ignored.
- `list(input: list)` — Pass the message if the mouse button is up
  If the mouse button is up, the list is sent out the outlet. Otherwise, the list is ignored.
- `anything(input: list)` — Pass the message if the mouse button is up
  Performs the same as list.

## Help patcher examples

### basic

```
Example — [mousefilter]
  fan-in:
    in0 ← [number] ← [slider]    # drag the slider
  fan-out:
    out0 → [number]:in0    # does not change until you release the mouse
```

## See also

`mousestate`
