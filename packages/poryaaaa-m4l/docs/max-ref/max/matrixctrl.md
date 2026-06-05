# matrixctrl

_max · U/I_

> Matrix switch control

Provides a user interface control containing a group of cells in a grid. Cell states can either be on/off or incremental steps. This object is especially useful for controlling the matrix~ object.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | bang Outputs All Cells, list Sets Cells |
| out0 | Cell Value (Column, Row, Value) |
| out1 | Row/Column Values As list |

## Messages

- `bang` — Output all cell states
  bang causes matrixctrl to dump its current state in lists of three values for each cell pair, in the format
  horizontal-coordinate vertical-coordinate value
- `list(values: list)` — Set a group of cell states
  A list of ints sets cells in the matrixctrl object using the format . Multiple triplets of values can be used to set more than one cell. Coordinates for the cells start at 0 in the upper-left hand corner and the values for each cell start at 0 and go up to the value range minus one, set by the object's inspector. Substituting the symbols inc and dec in place of the value will increment or decrement that cell coordinate by a value of one. Changing the cell state with a list causes the list to be output from matrixctrl.
- `bkgndpicture(filename: symbol)` — Set background image (obsolete)
  This is a legacy message - it has been superseded by the bkgndpict attribute.
- `cellpicture(filename: symbol)` — Set cell image (obsolete)
  This is a legacy message - it has been superseded by the cellpict attribute.
- `clear` — Clear all cell states
  The word clear will set the value of all cells to 0.
- `dictionary(dictionary name: symbol)` — Set cell states
  The word dictionary followed a symbol naming a Max dictionary object, replaces all existing cell states with the ones in the dictionary. If the dictionary has no cell states, the existing connections are cleared. The format of the dictionary is an entry named "connections" containing an array of dictionaries, each one with entries for "in" (row), "out" (column), and "gain" (cell value).
- `disable(coordinates: list)` — Disable cells for editing
  Performs the same as disablecell.
- `disablecell(coordinates: list)` — Disable cells for editing
  The word disablecell, followed by a list of number pairs which specify the horizontal and vertical coordinates of a cell or cells, sets the designated cell or cells so that they do not respond to mouse clicks. The disablecell message expects at least one pair of numbers, but more may be added to disable multiple cells (e.g., disable 0 0 3 4 9 12). Although disabled cells will ignore mouse clicks, their values can be set using messages.
- `enablecell(coordinates: list)` — Enable a disabled cell
  The word enablecell, followed by a list of number pairs which specify the horizontal and vertical coordinates of a cell or cells, will set any designated cell or cells which have been disabled using the disablecell message to respond to mouse clicks again. The enablecell message expects at least one pair of numbers, but more may be added to enable multiple cells (e.g., enable 1 1 1 2 2 2).
- `getcolumn(column: int)` — Get all cell states in one column
  The word getcolumn, followed by a number, sends the values of the cells in the column designated by the number out its right outlet. Column numbers start at 0.
- `getrow(row: int)` — Get all cell states in one row
  The word getrow, followed by a number, sends the values of the cells in the row designated by the number out its right outlet. Row numbers start at 0.
- `readanybkgnd(filename: list)` — Read any file for a background image
  The word readanybkgnd followed by the name of a file will read any type of file into the matrixctrl object and attempt to interpret it as a background image.
- `readanycell(filename: list)` — Read any file for a cell image
  The word readanycell followed by the name of a file will read any type of file into the matrixctrl object and attempt to interpret it as a cell image.
- `set(input: list)` — Set cell states with no output
  The word set, followed by a list as described above, changes the state of matrixctrl without echoing the values to the output.

## GUI behaviors

- `(mouse)` — Set cell state
  A mouse click on a cell will increase its value by one. Values in matrixctrl will wrap back to 0 once they have reached their maximum possible state. Dragging across several cells will set their values to that of the first cell clicked. Dragging across cells while holding down the Shift key will allow you to drag in straight horizontal or vertical lines only.

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

### dialmode 2

> dialmode 2 draws a dial without an image file and is designed to work with a cell range of 2 (0 - 1) where it will output float values. You can use it with matrix~ or matrix to set both connections and gain values

```
Example — [matrixctrl]  drag cells up and down to change cell values
  fan-out:
    out0 → [message "5 3 0.64"]:in1
```

### appearance

```
Example — [matrixctrl]
  fan-in:
    in0 ← [attrui @style]    # Create and apply styles using the format palette or type a style name here
    in0 ← [message "3 1 1"]
    in0 ← [attrui @elementcolor]
    in0 ← [attrui @bgcolor]
    in0 ← [attrui @color]
```

Attributes demonstrated: `@bgcolor`, `@color`, `@elementcolor`, `@style`

### dialmode 1

> note: dialmode 2 is strongly recommended over dialmode 1 particularly when working with matrix~ or matrix

```
Example — [matrixctrl]  drag cells up and down to change cell values / Have a look at the "cell image file" referred to in the inspector of this instance of matrixctrl for an example of how to format the dials. The cell range should match the number of images in the file. Unexpected and undesirable results will happen when these don't match! / dialmode 1 uses an image file to animate the movement of a "dial" which is an alternative way of specifying a cell. Instead of clicking cells on and off you dial their values up and down.
  fan-out:
    out0 → [message "2 0 0"]:in1
```

### matrixctrl graphics

> Columns are all the same width, rows are all the same height. The 'Clicked' and 'Inactive' rows are optional. You can have neither, either, or both. You must set the corresponding checkboxes in the object's Info dialog box accordingly.
>
> Although the mask images shown in the diagram are supported for legacy purposes, Max 5 users should use the alpha channel to create transparent backgrounds.
>
> Once you choose a picture file, matrixctrl automatically recalculates the size of the cells to match. The cell size depends on both the size of the picture and the settings of the attributes in the Info dialog, so if your cells don't look right, check your attributes...

### basic

```
Example — [matrixctrl]
  fan-in:
    in0 ← [message "getcolumn 1"]
    in0 ← [message "getrow 0"]
    in0 ← [message "clear"]
    in0 ← [message "2 2 dec"]    # row/column data comes out right outlet
    in0 ← [message "2 2 inc"]
    in0 ← [message "0 0 1 1 1 1 2 2 1 3 3 1"]    # set cell state
    in0 ← [message "1 1 0"]
    in0 ← [message "4 2 1"]
    in0 ← [message "1 1 1"]
    in0 ← [message "active $1"]
  fan-out:
    out0 → [unpack 0 0 0]:in0
    out1 → [message "0 0 0 0"]:in1
```

## See also

`crosspatch`, `dial`, `kslider`, `matrix~`, `pictctrl`, `pictslider`, `router`, `rslider`, `slider`, `ubutton`
