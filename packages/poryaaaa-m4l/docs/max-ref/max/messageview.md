# messageview

__

> View a stream of messages

Use the messageview object to view a stream of messages.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | message input |
| out0 | message output when double clicked |

## Messages

- `bang` — Add a bang message to the message stream
- `int(ARG_NAME_0: int)` — Add an int message to the message stream
- `float(ARG_NAME_0: float)` — Add a float message to the message stream
- `list(ARG_NAME_0: list)` — Add a list message to the message stream
- `anything(ARG_NAME_0: list)` — Add a message to the message stream
- `append(ARG_NAME_0: list)` — Append a message to the message stream
  Append a message to the message stream (useful for adding messages to the stream that are otherwise used by the messageview object)
- `clear` — Clear the message stream
- `scrolltoend` — Scroll to the end of the message stream

## Attributes

- `@derived` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `one` — seen as: `one two three`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — message output when double clicked

### basic

```
Example — [messageview]
  fan-in:
    in0 ← [message "append clear"]
    in0 ← [message "clear"]
    in0 ← [attrui @autoscroll]
    in0 ← [message "one two three"]
    in0 ← [button]
    in0 ← [number]
    in0 ← [flonum]
```

Attributes demonstrated: `@autoscroll`

## See also

`dict.view`, `message`
