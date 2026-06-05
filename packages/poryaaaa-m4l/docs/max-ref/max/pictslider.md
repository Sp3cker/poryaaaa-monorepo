# pictslider

_max · U/I_

> Picture-based slider control

A slider control that uses pictures in external files for its appearance. It uses two pictures--one for the "knob" and one for the background over which the knob moves. The pictslider object has default pictures that are used if you do not want to supply pictures of your own, but its intended use is creating controls with customized appearances.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | int: Horizontal Value; list: Both Values |
| in1 | int: Vertical Value |
| out0 | Horizontal Value |
| out1 | Vertical Value |

## Messages

- `bang` — Output the current value
  In left inlet: Sends the current values of the pictslider to its outlets. The horizontal value is sent out the left outlet; the vertical value out its right outlet.
- `int(input: int)` — Function depends on inlet
  In left inlet: sets the pictslider object's horizontal value. The value is also sent out the left outlet, and the pictslider object's current vertical value is sent out the right outlet.
  In right inlet: sets the pictslider object's vertical value. The value is also sent out the right outlet, and the control's current horizontal value is sent out the left outlet.
- `float(input: float)` — Function depends on inlet
  Converted to int.
- `list(horizontal: int, vertical: int)` — Set horizontal and vertical values
  In left inlet: A list of two numbers sent to the left inlet sets the pictslider object's horizontal value to the first number and its vertical value to the second. The two values are sent out the left and right outlets.
- `bkgndpicture(filename: list)` — Load an image file for background appearance
  The word bkgndpicture, followed by a symbol that specifies a filename, designates the graphics file that the pictslider object will use for the control's background image. By convention, the pictslider object uses images saved in Portable Network Graphics (.png) format. If you are using Max on Windows and want to to work with images other than PNG or PICT files, we recommend that you install QuickTime and choose a complete install of all optional components. The symbol used as a filename must either be the name of a file in Max's current search path, or an absolute pathname for the file (e.g. " MyDisk:/Documents/UI Pictures/CoolBkgnd.png").
- `knobpicture(filename: list)` — Load an image file for knob appearance
  In left inlet: The word knobpicture, followed by a symbol that specifies a filename, designates the graphics file that the pictslider object will use for the control's knob file. The symbol used as a filename must either be the name of a file in Max's current search path, or an absolute pathname for the file (e.g. " MyDisk:/Documents/UI Pictures/CoolKnob.png"). The word knobpicture by itself puts up a standard Open Document dialog box.
- `readanybkgnd(filename: list)` — Load any file for background appearance
  The word readanybkgnd followed by the name of a file will read any type of file into the pictslider object and attempt to interpret it as a background image.
- `readanyknob(filename: list)` — Load any file for knob appearance
  The word readanyknob followed by the name of a file will read any type of file into the pictslider object and attempt to interpret it as a knob image.
- `set(horizontal: int, [vertical: int])` — Set horizontal and vertical values
  In left inlet: The word set, followed by a number, sets the pictcslider object's horizontal value but does not send the value out its left outlet.The word set, followed by two numbers, sets the pictslider object's horizontal value to the first number and its vertical value to the to the second number, but does not send the values out its outlets.
  In right inlet: The word set, followed by a number, sets the pictslider object's vertical value, but does not send the value out its right outlet.
- `track(ratio: float)` — Set tracking ratio
  In left inlet: The word track, followed by a float, sets the tracking ratio for horizontal movements of the pictslider object's knob.
  In right inlet: The word track, followed by a float, sets the tracking ratio for vertical movements of the pictslider object's knob.

## GUI behaviors

- `(drag)` — Load an image using drag-and-drop
  When a image file is dragged from the Max File Browser to a pictslider object, the image will be loaded as the object's background image.
- `(mouse)` — Set current value, cause output
  Clicking on the pictctrl object and dragging sends the current value out the outlet. Additional behaviors depend on how the object is configured using messages or setting attributes.

## Attributes

- `@category` (atom)
- `@default` (int)
- `@label` (symbol)
- `@paint` (int)
- `@save` (int)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `active` — seen as: `active $1`, `active 0`, `active 1`

## Help patcher examples

### picture_format

> Slider Knob Picture Format:
>
> All images in the picture have the same dimensions. The 'clicked' and 'inactive' images are optional, as are the image masks. The corresponding check boxes in the Inspector must be set to correspond to the presence/absence of the images.
>
> Although the mask images shown in the diagram is supported for legacy purposes, Max 5 users should use the alpha channel to create transparent backgrounds.
>
> When you choose a picture for the background, you can choose that the slider is automatically resized to match by checking a checkbox in the open-file dialog. The size of the bounding box depends on both the size of the picture and the settings in the Inspector, so if your picture doesn't look right, check your settings.
>
> Here's an example slider that uses a mask on its knob image:

> The mask images allow you to create knobs with non-rectangular outlines. Black regions of the mask indicate what regions of the images should be drawn, white regions are transparent, i.e. allow the background to be visible. Grey regions create translucency. (Note: if you use a mask with grey regions in a pictslider with an invisible background, the knob will not be drawn correctly when moved. Stick to black & white masks for sliders with invisible backgrounds.)

> The slider background picture is considerably simpler—it has only two images at most. Note that it must have an inactive image if the knob used in the control has an inactive image, and vice versa.

```
Example — [pictslider]
  fan-out:
    out0 → [number]:in0
```

### basic

```
Example #1 — [pictslider]
  fan-in:
    in0 ← [message "active 1"]
    in0 ← [message "active 0"]
    in0 ← [number] ← [pictslider]    # vertical-only motion, with mixing board-like appearance:
  fan-out:
    out0 → [number]:in0    # horizontal-only slider
```

```
Example #2 — [pictslider]  vertical-only motion, with mixing board-like appearance:
  fan-out:
    out1 → [number]:in0
```

```
Example #3 — [pictslider]  left inlet: int sets horizontal value, list sets both values. 'set' can be used to set value(s) without output. / right inlet: int sets vertical value. 'set' message can be use to set value without output.
  fan-in:
    in0 ← [message "active $1"]
    in0 ← [number]
    in0 ← [message "set 100 30"]
    in0 ← [message "30 100"]
    in1 ← [number]
  fan-out:
    out0 → [number]:in0    # horizontal value
    out1 → [number]:in0    # vertical value
```

## See also

`dial`, `kslider`, `multislider`, `nslider`, `pictctrl`, `rslider`, `slider`, `tab`, `textbutton`, `ubutton`
