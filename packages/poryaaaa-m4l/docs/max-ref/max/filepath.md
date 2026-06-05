# filepath

_max · Files_

> Manage and report on the Max search path

Provides access to the Max search path, and allows modification to the search path used by a patch.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | set Changes Path, append Adds Path |
| out0 | Path Stored in Preferences |

## Arguments

- **path-type** (`symbol`) — The search path to access
  Specifies one of the Max search path types (search, startup, help, action, or default)
- **preference** (`int`) _(optional)_ — The preference file file
  A number greater than zero specifies a slot in the Preferences file. If the argument is 0 or no number is supplied, the path will not be saved in the Preferences file.

## Messages

- `bang` — Output the current path
  bang causes the currently saved path name(s) to be output as a list.
- `append(folder: list, [subfolder-flag: int])` — Add a folder to a path
  The word append, followed by a symbol which specifies a folder, adds the folder to the list of paths (but does not save it in the Preferences file). An optional integer subfolder flag will also add any subfolders when set to 1.
- `clear(input: int)` — Obsolete method
  This method is obsolete. Currently, it has no effect.
- `revert` — Revert to the saved path contents
  Causes the pathnames to be reset to the last set of Max file preferences to be saved.
- `set(pathtype: list, [subfolder-flag: int])` — Set the path to access
  The word set, followed by the name of a Max search path type (search, startup, help, action, or default), sets the current search path to the type specified. An optional integer subfolder flag will also add any subfolders when set to 1.

## Help patcher examples

### basic

```
Example — [filepath search 4]
  fan-in:
    in0 ← [prepend set] ← [relativepath] ← [opendialog fold]    # 'set' message
    in0 ← [prepend append] ← [relativepath] ← [opendialog fold]    # 'append' message
    in0 ← [message "revert"]    # use last saved Max preferences
    in0 ← [button]    # report current path name
  fan-out:
    out0 → [message ""]:in1
```

## See also

`conformpath`, `filedate`, `folder`, `opendialog`
