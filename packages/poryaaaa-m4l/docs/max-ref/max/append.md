# append

_max · Messages_

> Append arguments to the end of a message

append will add arguments to the end of any message you input.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Message to append to |
| out0 | Resulting Appended Message |

## Arguments

- **appended-message** (`anything`) _(optional)_ — Sets message that will be appended

## Messages

- `bang` — Triggers output
  bang will cause append to output the message which was most recently stored in memory (bang is particularly useful for triggering output following the set message).
- `int(input: int)` — Message or list to append to
  The incoming integer value(s) will be appended by the message stored in append, preceded by a space, and the combined message is sent out the outlet.
- `float(input: float)` — Message or list to append to
  The incoming floating-point value(s) will be appended by the message stored in append, preceded by a space, and the combined message is sent out the outlet.
- `list(input: list)` — Message or list to append to
  The incoming list values will be appended by the message stored in append, preceded by a space, and the combined message is sent out the outlet.
- `anything(input-message: list)` — Message or list to append to
  The incoming message or list will be appended by the message stored in append, preceded by a space, and the combined message is sent out the outlet.
- `set(set: list)` — Replaces stored message without triggering output
  The word set, followed by any message, will replace the message stored in append, without triggering output.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `exactly` — seen as: `exactly`
- `only` — seen as: `only`

## Help patcher examples

### basic

```
Example #1 — [append]
  fan-in:
    in0 ← [message "set green apples"]    # use the 'set' message to change the appended item(s)
    in0 ← [message "set oranges"]
    in0 ← [number]
  fan-out:
    out0 → [message ""]:in1
```

```
Example #2 — [append 0.3 hours remain]
  fan-in:
    in0 ← [message "only"]
    in0 ← [message "exactly"]
  fan-out:
    out0 → [message ""]:in1
```

```
Example #3 — [append minutes]
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [message ""]:in1
```

## See also

`combine`, `join`, `pack`, `pak`, `prepend`, `zl`
