# absolutepath

_max · Files_

> Convert a file name to an absolute path

Converts a file name or path to an absolute path. If the file is not found, the symbol notfound is output.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | symbol | (symbol) file path |
| out0 | symbol | (symbol) resolved path |

## Messages

- `anything(pathname: symbol)` — Convert a file to an absolute path
  A file name or path as a symbol. Input pathnames can contain slashes, colons, or backslashes as separators.
- `types(filetypes: list)` — Designate the recognized file types
  The types message followed by a list of four-letter filetype codes will designate the types of files which absolutepath can recognize.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `absolutepath.maxhelp` — seen as: `absolutepath.maxhelp`

## Help patcher examples

### basic

```
Example — [absolutepath]
  fan-in:
    in0 ← [message "absolutepath.maxhelp"]
  fan-out:
    out0 → [message ""]:in1
```

## See also

`search_path`, `conformpath`, `opendialog`, `relativepath`, `savedialog`, `strippath`
