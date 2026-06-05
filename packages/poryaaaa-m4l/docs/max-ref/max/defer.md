# defer

_max · Timing_

> Defer execution of a message

Defers the output of all messages sent through it to the lower priority main thread. This is most applicable when using Overdrive mode.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Message To Be Deferred |
| out0 | The Deferred Message |

## Messages

- `bang` — Defer a message
  Same as anything.
- `int(input: int)` — Defer a message
  Same as anything.
- `float(input: float)` — Defer a message
  Same as anything.
- `list(input: list)` — Defer a message
  Same as anything.
- `anything(input: list)` — Defer a message
  Reduces the priority of the message received. This allows other messages (which may be more time-critical) to execute first.

## Help patcher examples

### basic

> the delay object is used to put the bang into the high-priority queue

```
Example — [defer]
  fan-in:
    in0 ← [gswitch2]
  fan-out:
    out0 → [message "first"]:in0    # output is reversed when the defer object is used
```

## See also

`deferlow`, `qlim`, `uzi`
