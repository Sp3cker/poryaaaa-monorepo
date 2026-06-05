# fpic

_max · U/I_

> Display an image

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | — | read Changes File, offset Changes Visible Area |
| out0 | matrix | out |

## Messages

- `bang` — Output as matrix
  The image loaded by fpic will be sent via the outlet as a matrix in response to a bang.
- `noscale` — Disable image scaling
- `offset(horizontal: int, vertical: int)` — Adjust image coordinates
  The word offset, followed by two numbers, specifies the number of pixels by which the left upper corner of the picture is to be offset horizontally and vertically from the left upper corner of the fpic box. By default the left upper corner of the picture is located at the left upper corner of fpic (that is, with an offset of 0,0). With successive slightly different offset messages, a picture can be moved inside fpic, and fpic can window different portions of a large picture. (In order to give the appearance of smooth transitions when moving an image, the old image is not erased when using the offset message. This may cause an undesired appearance if your picture contains a blank background that doesn't cover up what's beneath it.)
- `pict(filename: list)` — Load a new image
  The word pict, followed by the name of a graphics file in Max's search path, opens the file and displays the picture, replacing whatever picture was previously displayed. The fpic object accepts PNG files and, if QuickTime Version 7.1 or later is installed, other picture file formats that are listed in the QuickTime appendix.
- `read(filename: list)` — Load a new image
  The word read, followed by a symbol which specifies a filename, looks for a QuickTime graphic file with that name in Max's file search path, and opens it if it exists, displaying it in a graphic window. If the filename contains any spaces or special characters, the name should be enclosed in double quotes or each special character should be preceded by a backslash (\). The word read by itself puts up a standard Open Document dialog box and displays the common graphics files supported by QuickTime.
- `readany(filename: list)` — Load a new file without file filtering
  The word readany, followed by a symbol which specifies a filename, functions in the same manner as the read message, except that the Open Document dialog box does not filter its display by the currently supported filetypes.
- `rect(horizontal: int, vertical: int, width: int, height: int)` — Set the display size
  The word rect, followed by four numbers that specify the size of scaling rectangle to apply to fit the input image within, loads the graphics file from disc into RAM and displays it. The first two numbers specify the placement in the graphic window as offset values, and the second two numbers specify the width and height, in pixels, of the rectangle.

## GUI behaviors

- `(drag)` — Drag-and-drop picture loading
  When a image file is dragged from the Max File Browser to an fpic object, the image will be loaded.
- `(mouse)` — Offset picture coordinates
  In an unlocked patcher, you can change the offset of the picture by holding down the Shift and Command keys on Macintosh or Shift and Control keys on Windows and dragging on fpic; the current offset of the picture is shown in the Assistance portion of the patcher window as you drag.

## Attributes

- `@introduced` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `alpha` — seen as: `alpha $1`
- `pic` — seen as: `pic fpic_blue.png`, `pic fpic_house.png`, `pic fpic_interior.jpg`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — out

### basic

```
Example — [fpic]
  fan-in:
    in0 ← [attrui @destrect]    # set rect
    in0 ← [attrui @xoffset]    # offset image on x axis
    in0 ← [attrui @yoffset]    # offset image on y axis
    in0 ← [message "pic fpic_blue.png"]    # open a file dialog and choose a supported QuickTime graphic file
    in0 ← [message "pic fpic_house.png"]
    in0 ← [message "pic fpic_interior.jpg"]    # read a picture in Max's search path by name
    in0 ← [message "read"]
    in0 ← [message "readany"]    # open a file dialog and choose any file
    in0 ← [attrui @autofit]    # scale to object size
```

Attributes demonstrated: `@autofit`, `@destrect`, `@xoffset`, `@yoffset`

### advanced

> Offset example:

```
Example #1 — [fpic]  two overlapping fpics
  fan-in:
    in0 ← [message "alpha $1"]
```

```
Example #2 — [fpic]
  fan-in:
    in0 ← [message "alpha $1"]    # set alpha for both fpic objects
```

```
Example #3 — [fpic]
  fan-in:
    in0 ← [message "offset 0 0"]
    in0 ← [message "offset -122 0"]    # offset message tells the fpic what to show
```

## See also

`imovie`, `lcd`, `matrixctrl`, `panel`, `pictctrl`, `pictslider`, `ubutton`
