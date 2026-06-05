# live.thisdevice

_m4l · Live API Objects_

> Send a bang automatically when a Max Device is loaded, report device state

live.thisdevice reports three pieces of information about your Max Device. A bang message is automatically sent from the left outlet when the Max Device is opened and completely initialized, or when the containing patcher is part of another file that is opened. Additionally, a bang will be reported every time a new preset is loaded or the device is saved (and thus reloaded within the Live application). A 1 or 0 will be sent from the middle outlet when the Device is enabled or disabled, respectively. A 1 or 0 will be sent from the right outlet when preview mode for the Device is enabled or disabled, respectively. Used within Max, live.thisdevice functions essentially like the loadbang object. The middle and right outlets are inactive in this case.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | bang, getstate |
| out0 | Outputs bang When Patcher/Device is Loaded |
| out1 | Outputs enabled state of Device |
| out2 | Outputs preview state of Device |

## Messages

- `bang` — Sending a bang message to a live.thisdevice object causes it to output a bang message from the leftmost outlet.
- `getstate` — Sending a getstate message to a live.thisdevice object causes it to output the Max Device state from the rightmost outlet.
- `loadbang` — Same as bang.
- `setwidth(width: int)` — Sets the width of the device.
  The setwidth message will dynamically set the width of the Max for Live device.
  Note: This width is not automatically saved as part of the preset and/or Live set.
  The message setwidth 0 will return to the default condition where the width of the device is calculated by using the devices's visible objects.

## GUI behaviors

- `(mouse)` — Double-clicking on a live.thisdevice object causes it to output a bang message from the leftmost outlet.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@label` (symbol)
- `@selfsave` (int)
- `@style` (symbol)

## Help patcher examples

### basic

> you can dynamically set the device width
>
> double-clicking a live.thisdevice or sending it a bang message also causes it to output a bang
>
> the message 'getstate' will resend the current device state from the center outlet

> Within a non-Device context, live.thisdevice behaves like the 'loadbang' object. The middle and right outlets are inactive in this case.

```
Example — [live.thisdevice]
  fan-in:
    in0 ← [message "setwidth $1"]
  fan-out:
    out0 → [toggle]:in0
    out1 → [toggle]:in0
    out2 → [toggle]:in0
```

## See also

`active`, `button`, `closebang`, `freebang`, `loadbang`, `loadmess`, `thispatcher`
