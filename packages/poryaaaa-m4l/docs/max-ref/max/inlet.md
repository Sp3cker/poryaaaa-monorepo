# inlet

_max · U/I_

> Receive messages from outside a patcher

Receive messages from outside the patcher wherever it is embedded. Each inlet object in a patcher will show up as an inlet at the top of an object box when the patch is used inside another patcher (as an object or a subpatch). Messages sent into such an inlet will be received by the inlet object in the subpatch.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Outputs Incoming Messages to Patcher |
| out0 | Outputs Incoming Messages to Patcher |

## GUI behaviors

- `(mouse)` — Open the parent patch
  Double-clicking on an inlet object will open the parent patch or bring it to front.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@introduced` (symbol)
- `@label` (symbol)
- `@order` (int)
- `@selfsave` (int)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — Outputs Incoming Messages to Patcher
> - `in0` — Outputs Incoming Messages to Patcher

## See also

`bpatcher`, `outlet`, `pcontrol`, `receive`, `send`
