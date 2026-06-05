# hover

_max · U/I_

> Report object scripting names

Sends out the scripting names of any object over which the cursor is hovering.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | controls/attributes |
| out0 | Reports the scripting name of the object over which the mouse is hovering at any given time. |
| out1 | dumpout |
| out2 | Scripting Name of Object Mouse Has Just Left |
| out3 | none if Object Mouse Has Just Left Has No Scripting Name |

### Port details

**`out0` (Reports the scripting name of the object over which the mouse is hovering at any given time.):** Reports the scripting name of the object over which the mouse is hovering at any given time.

**`out1` (dumpout):** The dumpout sends a 'none' message if no named objects are found under the mouse at a given location.

## Attributes

- `@label` (symbol)
- `@save` (int)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in0` — controls/attributes

### basic

```
Example — [hover]  move your mouse over this button, and other objects in this patch
  fan-out:
    out0 → [select button]:in0
    out1 → [sel none]:in0
    out2 → [select button]:in0
    out3 → [sel none]:in0
```

## See also

`thispatcher`
