# patcher

_max · Control, Patching_

> Create a subpatch within a patch

Creates patches within patches.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 |  |

## Arguments

- **subpatch** (`symbol`) _(optional)_ — Subpatch name
  The subpatch can be given a name by the argument, so that its name appears in the title bar of the subpatch window. The name in the title bar of the subpatch window is displayed in brackets to indicate that it is part of another file. If there is no argument typed in, the subpatch window is named [sub patch]. Different patcher objects that share the same name are still distinct subpatches, and do not share the same contents.

## Attributes

- `@attr_attr_save` (int)
- `@category` (symbol)
- `@dynamiccolor_default` (symbol)
- `@label` (symbol)
- `@paint` (int)
- `@preview` (symbol)
- `@save` (int)
- `@set` (pointer)
- `@style` (symbol)

## Help patcher examples

### basic

> You can embed a Patcher in another one with a "Patcher" box, as above. The inlets and outlets on the box correspond to the inlet and outlet boxes in the embedded Patcher. You can close the embedded window by clicking in its close box and reopen it by double-clicking the Patcher box below.

```
Example — [patcher]  double-click to see the embedded patch
  fan-in:
    in0 ← [dial]
  fan-out:
    out0 → [number]:in0
    out1 → [number]:in0
```

## See also

`bpatcher`, `inlet`, `outlet`, `pcontrol`, `thispatcher`
