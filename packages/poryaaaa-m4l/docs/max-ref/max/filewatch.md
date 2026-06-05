# filewatch

_max · Files_

> Watch a file for changes

Watch a specific file and reports a bang whenever that file is altered.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | File/Folder Name, bang Starts Watching |
| out0 | bang If File Changes |

## Arguments

- **filename** (`symbol`) _(optional)_ — The file name to watch
  An optional symbol argument specifies the file name to watch.

## Messages

- `bang` — Start reporting on file changes
  See the int entry.
- `int(flag: int)` — Set reporting of file changes
  Turns on the filewatch object. Sending a non-zero number causes the filewatch object to commence watching the file for changes. Sending a 0 causes the object to ignore changes to the file.
- `anything(filepath: list)` — Set the file to watch
  A filepath or filename within the Max search path will set which file is to be watched by filepath.
- `stop` — Stop reporting on file changes
  The stop message functions the same as sending a 0, causing the object to ignore any file changes.

## Help patcher examples

### basic

```
Example — [filewatch]
  fan-in:
    in0 ← [opendialog] ← [button]    # choose a file
    in0 ← [toggle]    # send a 1 to filewatch to start watching the file, 0 to stop
  fan-out:
    out0 → [button]:in0    # bangs when file is changed
```

## See also

`absolutepath`, `opendialog`, `relativepath`, `savedialog`
