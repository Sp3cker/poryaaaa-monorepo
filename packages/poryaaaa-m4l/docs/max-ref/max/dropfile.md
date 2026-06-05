# dropfile

_max · U/I_

> Drag and drop files

dropfile defines a region for dragging and dropping files into and then outputs the filepath and filetype upon file input.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | types Specifies Acceptable File Types |
| out0 | Pathname of Dropped File |
| out1 | Type Code of Dropped File |

## GUI behaviors

- `(drag)` — Drag and drop file
  When a file icon is dragged from the Finder or Max File Browser onto a dropfile object in a locked patcher window, the object checks the file's type against those that it has been told to accept. If the file is of an acceptable type, the outline of the dropfile box is highlighted. If the mouse button is released while the cursor is inside the dropfile box, the dropfile object outputs the type and full pathname of the file out its outlets.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (float)
- `@label` (symbol)
- `@paint` (int)
- `@save` (int)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `types` — seen as: `types`, `types $1`, `types MooV PICT`

## Help patcher examples

### appearance

> dropfile

> Appearance:

```
Example — [dropfile]
  fan-in:
    in0 ← [attrui @border]
    in0 ← [attrui @bordercolor]
    in0 ← [attrui @rounded]
```

Attributes demonstrated: `@border`, `@bordercolor`, `@rounded`

### basic

```
Example — [dropfile]  drag and drop a file here...
  fan-in:
    in0 ← [message "types $1"]
    in0 ← [message "types"]
    in0 ← [message "types MooV PICT"]    # match all file types
  fan-out:
    out0 → [message ""]:in1    # full path
    out1 → [message ""]:in1    # type code
```

## See also

`absolutepath`, `filepath`, `folder`, `opendialog`, `relativepath`, `strippath`
