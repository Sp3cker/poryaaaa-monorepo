# filedate

_max · Files_

> Report the modification date of a file

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | File/Folder to Check Modification Date |
| out0 | Mod Date (Month/Day/Year/Hour/Min/Sec) |

## Messages

- `anything(pathname: list)` — Report the modification of a file
  Outputs the modification date of a provided file path.

## Help patcher examples

### basic

```
Example — [filedate]
  fan-in:
    in0 ← [opendialog] ← [button]    # bang opendialog and choose a file
  fan-out:
    out0 → [unpack 0 0 0 0 0 0]:in0
```

## See also

`date`, `filein`, `filepath`, `folder`, `opendialog`
