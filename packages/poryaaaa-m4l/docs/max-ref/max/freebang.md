# freebang

_max · Patching_

> Send a bang when a patcher is freed

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Sends bang When Patcher Is Freed |
| out0 | Sends bang When Patcher Is Freed |

## Messages

- `bang` — Output a bang message
  Sending a bang message to a freebang object causes it to output a bang message.

## GUI behaviors

- `(mouse)` — Manually fire output
  Double-clicking on a freebang object causes it to output a bang message.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — Sends bang When Patcher Is Freed
> - `in0` — Sends bang When Patcher Is Freed

### basic

```
Example — [freebang]
  (no patch cords)
```

## See also

`active`, `button`, `closebang`, `loadbang`, `loadmess`, `savebang`
