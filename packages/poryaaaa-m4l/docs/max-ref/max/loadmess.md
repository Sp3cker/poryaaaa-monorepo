# loadmess

_max · Messages, Patching_

> Send a message when a patch is loaded

Outputs a message automatically when the file is opened, or when the patch is part of another file that is opened.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Double-Click or bang to Re-Trigger Message |
| out0 | Message Output When Patcher Is Loaded |

## Arguments

- **message** (`symbol`) — Message to output
  Any arguments you type into a loadmess object are treated as a message to be sent when output is triggered.

## Messages

- `bang` — Output the message
  Sending a bang message to a loadmess object causes it to output its typed message.
- `set(message: list)` — Set the message with no output
  The word set followed by any message will set the message held by loadmess without any output. (Can be used for output in conjunction with bang.)

## GUI behaviors

- `(mouse)` — Output the message
  Double-clicking on a loadmess object causes it to output its typed message.

## Attributes

- `@basic` (int)
- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### basic

> hold down Shift-Command (Mac) or Shift-Ctrl (PC) to disable loadmess firing at patcher load

```
Example #1 — [loadmess]
  fan-out:
    out0 → [print Bang]:in0
```

```
Example #2 — [loadmess 5.5]
  fan-out:
    out0 → [print Float]:in0
```

```
Example #3 — [loadmess 42]  double-clicking a loadmess causes it to output its message
  fan-out:
    out0 → [print Int]:in0
```

```
Example #4 — [loadmess 3.14 22 1.41 0.707 9]
  fan-out:
    out0 → [print List]:in0
```

```
Example #5 — [loadmess cycling 74 9 666]
  fan-in:
    in0 ← [button]    # bang also causes output
  fan-out:
    out0 → [print Message]:in0
```

## See also

`active`, `button`, `closebang`, `defer`, `freebang`, `loadbang`, `thispatcher`, `savebang`
