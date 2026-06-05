# colorpicker

_max · System_

> Select and output a color

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | bang opens picker, list sets initial color value |
| out0 | List of RGB Color Values |
| out1 | bang when mouse is released and when color window closes |

### Port details

**`out0` (List of RGB Color Values):** Out left outlet: a list of the RGBA or RGB color values that correspond to the color selected (the output format is set using the compatibility attribute.

**`out1` (bang when mouse is released and when color window closes):** Out right outlet: A bang message is sent out the right outlet when the user releases their mouse after picking a color, and when the user closes the color window.

## Messages

- `bang` — Open the color picker dialog
  Same as double-clicking the object. See the entry for (mouse).
- `list(red: number, green: number, blue: number, [alpha: number])` — Set the selected color
  A list of numbers can be used to set the RGB or RGBA color components of the default color that initially appears in the Color Picker dialog when it is opened. A list of four floating poing numbers in the range 0. - 1.0 will specify the default color in RGBA format. For compatibility, a list of three integers in the range 0 - 255 will specify the color in the old style RGB format.

## GUI behaviors

- `(mouse)` — Open the color picker dialog
  Double-clicking the object opens the Color Picker dialog box. If the patcher is unlocked, hold down the Command key on Macintosh or the Control key on Windows while double-clicking to open the dialog.

## Attributes

- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### basic

```
Example #1 — [colorpicker @compatibility 0]
  fan-in:
    in0 ← [message "0.3 1. 0. 0.5"]
    in0 ← [button]    # Open Color dialog box.
  fan-out:
    out0 → [prepend set]:in0
    out0 → [prepend bgcolor]:in0
    out1 → [button]:in0    # Double-click opens dialog box, too.
```

```
Example #2 — [colorpicker]  compatibility = 1 (default)
  fan-in:
    in0 ← [button]    # Open Color dialog box.
    in0 ← [message "21 0 255"]
  fan-out:
    out0 → [prepend brgb]:in0
    out0 → [prepend set]:in0
    out1 → [button]:in0    # bangs when mouse is released after picking a color, and when the color window closes
```

## See also

`dynamic_colors`, `panel`, `swatch`
