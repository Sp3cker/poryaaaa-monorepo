# live.push

__

> Configuration of Push 2 and 3

live.push allows a Max for Live device to override the settings of a connected Push instrument. The Push settings are only overridden if the Max for Live device containing live.push is selected, or if the device is the first in the device chain. This behavior can be configured with the play_usage attribute.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages in |

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@label` (symbol)

## Help patcher examples

### Grabbing Controls

> Repurposing individual Push controls can be done without live.push, via the Live API.
>
> The Live API only functions when used inside of Max for Live devices.
>
> When a control is grabbed, it will stop functioning as it usually does and only pass values to and receive values from Max.

### Pad Configuration

> On the Push device, after pressing the settings (cogwheel) button, you can go to the Expression tab and set Expression Mode to MPE. Then, you can see the pad calibration settings, which live.push can override.

> At 10 mm, the vertical position on the pad covers the slide range quicker than at 16 mm. -1 means using the value set on Push.

> Set to 0 mm, pressing a pad will always result in a non-0 pitch bend. -1 means using the value set on Push.

> If set to "pad", you will notice that pressing a pad on its side results in a non-0 pitch bend.

> To receive MPE, make sure the "Patch Supports MPE" (is_mpe) attribute is set to 1 for the device you paste this in.

### Note Colors

> In serial pad map mode, the MIDI note colors can be specified with the play_note_colors attribute.

> By specifying a list of 128 values, we assign a color to every MIDI note individually. When providing a shorter list, the colors wrap over the notes.

### Note Mapping Mode

> The play_pad_map attribute allows changing how MIDI notes are mapped to pads in the Push pad matrix. 'scale' mode is the default on Push when a melodic instrument is selected, where notes are mapped and colored according to the selected Scale and In-Key / Chromatic mode. 'serial' mode simply maps all MIDI notes to all pads linearly.

```
Example — [live.push @play_usage first_or_selected]
  fan-in:
    in0 ← [r ---to_live_push]
    in0 ← [attrui @play_usage]    # play_usage determines when live.push's settings override the Push's behavior
    in0 ← [attrui @play_pad_map]    # Switch modes and see how this affects the incoming notes when pressing pads on Push.
```

Attributes demonstrated: `@play_pad_map`, `@play_usage`

## See also

`live.banks`
