# gate

_max · Control_

> Pass input to an outlet

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | 0 Closes gate, Non-zero Opens Gate Outlet |
| in1 | Incoming Gated Messages |
| out0 | Output of Messages if Open |

## Arguments

- **outlets** (`int`) _(optional)_ — Number of outlets
  Specifies the number of outlets. If there is no argument, there is only one outlet.
- **initial-state** (`int`) _(optional)_ — Initial state
  Specifies which outlet is initially open. The default is 0 (none).

## Messages

- `bang` — Report the current outlet
  Reports the current open outlet (0 if closed) out the left outlet.
- `int(outlet-number: int)` — Set outlet number
  The number specifies an open outlet through which to pass all messages received in the right inlet. A number in the left inlet does not trigger any output.
- `float(outlet-number: float)` — Set outlet number
  Converted to int.
- `next` — Cycle to next inlet
  Sending the next message to the left inlet will close the current outlet and open the next one, wrapping accross all outlets. If currently closed (set to 0) or set to the rightmost outlet, outlet 1 will be opened.

## Help patcher examples

### advanced

```
Example #1 — [gate 1 1]
  fan-in:
    in1 ← [number]
    in1 ← [flonum]
    in1 ← [umenu]    # gate passes all message types:
  fan-out:
    out0 → [print @popup 1]:in0
```

```
Example #2 — [gate 3 2]
  fan-in:
    in1 ← [counter 500] ← [metro 500 @active 1]    # optional second argument sets initially open outlet:
  fan-out:
    out0 → [number]:in0
    out1 → [number]:in0
    out2 → [number]:in0
```

### basic

```
Example — [gate 3]
  fan-in:
    in0 ← [number] ← [radiogroup]    # 3 - open third outlet / 2 - open second outlet / 1 - open first outlet / 0 - all outlets off
    in1 ← [counter 500] ← [metro 200 @active 1]
  fan-out:
    out0 → [number]:in0
    out1 → [number]:in0
    out2 → [number]:in0
```

## See also

`gswitch2`, `crosspatch`, `gswitch`, `route`, `router`, `send`, `switch`
