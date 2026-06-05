# loadbang

_max · Patching_

> Send a bang when a patcher is loaded

Outputs a bang automatically when the file is opened or when the patch is part of another file that is opened.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Outputs bang When Patcher Window is Loaded |
| out0 | Outputs bang When Patcher Window is Loaded |

## Messages

- `bang` — Output a bang message
  Sending a bang message to a loadbang object causes it to output a bang message.
- `loadbang` — Output a bang message
  Same as bang.

## GUI behaviors

- `(mouse)` — Manually fire output
  Double-clicking on a loadbang object causes it to output a bang message.

## Help patcher examples

### basic

```
Example #1 — [loadbang]  hold down Shift-Command (Mac) or Shift-Ctrl (PC) to disable loadbang firing at patcher load
  fan-out:
    out0 → [message "active"]:in0
```

```
Example #2 — [loadbang]  double-clicking a loadbang or sending it a bang message also causes it to output a bang
  fan-in:
    in0 ← [button]
  fan-out:
    out0 → [button]:in0
```

```
Example #3 — [loadbang]
  fan-out:
    out0 → [toggle]:in0
```

## See also

`active`, `button`, `closebang`, `freebang`, `loadmess`, `thispatcher`, `savebang`
