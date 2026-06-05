# live.colors

_m4l · Live UI Objects_

> Get the colors of the active Ableton Live Theme via a Max for Live device.

Get the dynamic colors from the active Ableton Live application color theme and use these color names to ensure compatibility between various Max objects and Ableton Live themes. These colors adapt dynamically to the active Live theme, eliminating the need to change RGBA values, since each color name is a token that references specific colors across Live themes.

 In Max 8.2 or later, you can change a Max object's color by setting dynamic colors using the color picker in the Max object's inspector window, instead of using live.colors.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Message in |
| out0 | Color in RGBA Format (float numbers from 0. to 1.) |
| out1 | Bang when the Live colors change |

## Messages

- `anything(Color Name: symbol)` — The dynamic color name in Live
  Sends the color for Color Name from the live.colors object's left outlet in RGBA format (float numbers from 0. to 1.).
- `everything` — Report all current color RGBA values as a series of messages.
  Sends all the available colors out the outlet as a series of messages. Each message consists of the color id followed by four floating-point values that describe the color scheme color in RGBA format (float numbers from 0. to 1.).

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `"Control` — seen as: `"Control Border"`
- `"Slider` — seen as: `"Slider Range Value"`
- `"Text` — seen as: `"Text / Icon"`

## Help patcher examples

### dynamic colors javascript

> Every dynamic color in Max is associated with an "identifier" or "token". You can use these identifiers in JSUI by passing them as an argument to the max.getcolor(identifier) function, which will return RGBA values for that dynamic color. These update in realtime, allowing you to make color schemes for interfaces that will match Live's.

> This is itself a JSUI drawn by accessing a dictionary by filename "maxcolors.json". This dictionary of colors exposes many colors. Only the "essential" ones are shown here, which you can also find in the drop down menu of the color picker in the inspector window. Double click to open the relevant JSUI file.

> Each color in maxcolors.json is represented by a dictionary containing various properties. The "id" is the property you need to get in order to use Live Color Tokens in JSUI.

### dynamic colors

Attributes demonstrated: `@activedialcolor`

### basic

```
Example — [live.colors]
  fan-in:
    in0 ← [message ""Control Border""]
    in0 ← [message ""Text / Icon""]
    in0 ← [message ""Slider Range Value""]
  fan-out:
    out0 → [route "Control Border" "Text / Icon" "Slider Range Value"]:in0
    out0 → [message ""Control Border" 0.313725 0.313725 0.313725 1."]:in1    # Outputs the color name and it's RGBA (Red, Blue, Green, Alpha) values from 0. to 1.
    out1 → [message ""Control Border""]:in0
    out1 → [message ""Text / Icon""]:in0
    out1 → [message ""Slider Range Value""]:in0
```

## See also

`dynamic_colors`, `panel`, `colorpicker`, `suckah`
