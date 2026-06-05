# bgcolor

_max · Patching_

> Set background color

Set the background color of the patcher window. The bgcolor object's functionality is equivalent to a brgb message sent to thispatcher.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Red Value or list of RGB Values |
| in1 | Green Value |
| in2 | Blue Value |
| in3 | Alpha Value (0.-1.) |

## Arguments

- **red** (`int`) _(optional)_ — Set the red value
  A number in the range 0-255 sets the red value for the background color of the patcher.
- **green** (`int`) _(optional)_ — Set the green value
  A number in the range 0-255 sets the green value for the background color of the patcher.
- **blue** (`int`) _(optional)_ — Set the blue value
  A number in the range 0-255 sets the blue value for the background color of the patcher.

## Messages

- `bang` — Reset background to recent colors
  bang will reset the patcher's background to the RGB values most currently received by bgcolor.
- `int(red: int)` — Set the red value
  A number in the range 0-255 sets the red value of the patcher background color.
- `list(red: int, green: int, blue: int)` — Set the background color
  A list of three numbers in the range 0-255 sets the background color of the patcher in RGB format.
- `ft3(alpha: float)` — Set the alpha value
  A floating-point number in the range 0.0 to 1.0 sets the alpha value of the patcher background color.
- `in1(green: int)` — Set the green value
  A number in the range 0-255 sets the green value of the patcher background color.
- `in2(blue: int)` — Set the blue value
  A number in the range 0-255 sets the blue value of the patcher background color.
- `set(red: int, green: int, blue: int)` — Set the background color
  The word set followed by a list of three numbers in the range 0-255 sets the background color of the patcher in RGB format.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in1` — Green Value
> - `in2` — Blue Value
> - `in3` — Alpha Value (0.-1.)

### basic

```
Example — [bgcolor]
  fan-in:
    in0 ← [swatch]
```

## See also

`dynamic_colors`, `thispatcher`
