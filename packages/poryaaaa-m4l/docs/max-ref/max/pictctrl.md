# pictctrl

_max · U/I_

> Picture-based control

Creating buttons, switches, knobs, and other controls using images from a picture file for its appearance.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | int (0-1) Sets Value, read Opens File |
| out0 | Button or Dial Value |

## Messages

- `bang` — Output the current value
  Sends the current value of the pictctrl to the outlet.
- `int(input: int)` — Set current value, cause output
  Sets the value of the button or knob set by the control, and sends the current value out the outlet. In button and toggle mode, the value must be either 0 or 1. In dial mode, the range of values is determined by pictctrl object's Range attribute.
- `float(input: float)` — Set current value, cause output
  Converted to int.
- `link(filename: symbol)` — Link image to pictctrl
  The word link, followed by a filename to a file which has already been loaded into the pictctrl object will link the object with that file.
- `picture(imagefile-name: list)` — Read an image file from disk
  The word picture followed by the name of an image file will read that file into the pictctrl object. The word with no name following opens a standard file dialog for choosing an image file.
- `read([filename: list])` — Read an image file from disk
  The word read followed by the name of an image file will read that file into the pictctrl object. The word with no name following opens a standard file dialog for choosing an image file.
- `readany(filename: list)` — Read any file as an image
  The word readany followed by the name of a file will read any type of file into the pictctrl object and attempt to interpret it as a picture.
- `set(input: int)` — Set current value with no output
  The word set, followed by a number, sets the value of the button or knob to that number, without triggering output.

## GUI behaviors

- `(drag)` — Load an image using drag-and-drop
  When a image file is dragged from the Max File Browser to a pictctrl object, the image will be loaded.
- `(mouse)` — Set current value, cause output
  Clicking on the pictctrl object and dragging sends the current value out the outlet. Additional behaviors depend on how the object is configured using messages or setting attributes using the Inspector.

## Attributes

- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)
- `@paint` (int)
- `@save` (int)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `active` — seen as: `active $1`

## Help patcher examples

### pictureFormats

> All control modes require at least the top row of images. The inactive images are optional. Toggle controls usually need the second row of images also to represent the "on" or value = 1 state. In all formats, columns are all the same width, rows are all the same height.
>
> The mask images shown in these diagrams are supported for legacy purposes, but standard practise is to use the alpha channel to create transparent backgrounds.
>
> Once you choose a picture file, pictctrl automatically changes its bounding-box size to match. The box size depends on both the size of the picture and the control mode, so if your picture doesn't look right, maybe you're in the wrong mode...

> Pictures for dial controls must have one column for each position of the dial. Note that the number of positions can be different from the range of values.

> As an example, here's the picture for the toggle button seen in the basic tab of this help file

### basic

```
Example #1 — [pictctrl]
  fan-out:
    out0 → [number]:in0
```

```
Example #2 — [pictctrl]  A dial—click and drag horizontally or vertically:
  fan-out:
    out0 → [number]:in0
```

```
Example #3 — [pictctrl]
  fan-out:
    out0 → [number]:in0
```

```
Example #4 — [pictctrl]  You can make controls out of any picture:
  fan-in:
    in0 ← [decide] ← [metro 100] ← [toggle]    # click here to make the cat twitch:
```

```
Example #5 — [pictctrl]  toggle
  fan-in:
    in0 ← [message "active $1"]
    in0 ← [message "set $1"]
    in0 ← [message "1"]
    in0 ← [message "0"]
  fan-out:
    out0 → [number]:in0
```

```
Example #6 — [pictctrl]  buttons:
  fan-out:
    out0 → [number]:in0
```

## See also

`dial`, `kslider`, `matrixctrl`, `pictslider`, `rslider`, `slider`, `tab`, `textbutton`, `ubutton`
