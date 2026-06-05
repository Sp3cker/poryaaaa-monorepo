# folder

_max · Files_

> List the files in a folder

Outputs all of the file names in a given folder. This can be especially useful for loading a umenu.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Path Name of Folder to List |
| out0 | File Name List, Connect to Menu |
| out1 | Count of Items Listed |

## Arguments

- **pathname** (`symbol`) _(optional)_ — The absolute path to a folder

## Messages

- `bang` — Output all file names in a folder
  Gets the names of all files of a specific type within a specific folder, and outputs those names to be placed in a message object or a pop-up umenu object.
- `int(input: int)` — Output all file names in a folder
  See the bang entry.
- `anything(pathname: list)` — Specify a new folder
  See the symbol entry.
- `symbol(pathname: symbol)` — Specify a new folder
  Specifies the pathname of a folder in the search path, and causes the contents of that folder to be output for storage in a umenu or a message. Input pathnames can contain slashes, colons, or backslashes as separators.
- `types(typecode: list)` — Set the available file types
  The word types, followed by one or more four-letter filetype codes sets the file types that the folder object will look for in the specified folder.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — Count of Items Listed

### basic

```
Example — [folder C74:/help/max]
  fan-in:
    in0 ← [prepend types] ← [umenu]    # choose a file type to restrict the results
    in0 ← [message "types"]    # return all file types
    in0 ← [dropfile]    # use dropfile to select a folder / drop a folder here!
    in0 ← [button]    # bang to output list
  fan-out:
    out0 → [print @popup 1]:in0
    out0 → [umenu]:in0
```

## See also

`conformpath`, `dropfile`, `filein`, `filepath`, `opendialog`, `pcontrol`, `umenu`
