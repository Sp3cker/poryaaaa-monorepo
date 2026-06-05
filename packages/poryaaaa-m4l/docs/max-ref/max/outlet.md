# outlet

_max · U/I_

> Send messages out of a patcher

Show up as an outlet at the bottom of an object box when the patcher is used inside another patcher (as an object or a subpatch). Messages received by the outlet object will come out of the corresponding outlet in the subpatch's object box.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Sends Messages Out Patcher's Outlet |
| in1 | Sends Messages Out Patcher's Outlet |

## GUI behaviors

- `(mouse)` — Open the parent patch
  Double-clicking on an outlet object will open the parent patch or bring it to front.

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
> - `in0` — Sends Messages Out Patcher's Outlet
> - `in1` — Sends Messages Out Patcher's Outlet

## See also

`bpatcher`, `forward`, `inlet`, `patcher`, `receive`, `send`
