# deferlow

_max · Timing_

> Defer the execution of a message (always)

The deferlow object places all incoming messages at the tail of the low priority queue. This is unlike the defer object, however, which places high priority messages at the front of the low priority queue, and passes low priority messages immediately. The deferlow object is useful to preserve message sequencing that might otherwise be reversed with the defer object and/or guarantee that an incoming message will be deferred to a future servicing of the low priority queue even if that message is low priority itself.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Message To Be Deferred |
| out0 | The Deferred Message |

## Messages

- `bang` — Pass through, deferred.
  The deferlow object places the received message at the tail of the low priority queue for deferred execution.
- `int(value: int)` — Pass through, deferred.
  The deferlow object places the received message at the tail of the low priority queue for deferred execution.
- `float(value: float)` — Pass through, deferred.
  The deferlow object places the received message at the tail of the low priority queue for deferred execution.
- `list(args: list)` — Pass through, deferred.
  The deferlow object places the received message at the tail of the low priority queue for deferred execution.
- `anything(args: list)` — Pass through, deferred.
  The deferlow object places the received message at the tail of the low priority queue for deferred execution.

## Help patcher examples

### basic

```
Example — [deferlow]
  fan-in:
    in0 ← [trigger l l]
  fan-out:
    out0 → [print b @popup 1]:in0
```

## See also

`defer`, `delay`, `qlim`, `uzi`
