# live.dial

_m4l · Live UI Objects_

> Output numbers by moving a dial onscreen

live.dial works like a circular slider that outputs numbers according to its degree of rotation.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | int/float | Parameter Value (0.00-127.00) |
| out0 | int/float | Parameter Value (0.00-127.00) |
| out1 | int/float | Parameter Raw Value (0.-1.) |

## Messages

- `bang` — Send the current value out the outlet
- `int(input: int)` — Store, display, and output a value
  The number received in the inlet is stored and displayed by the live.dial object and sent out the outlet.
- `float(input: float)` — Store, display, and output a value
  The number received in the inlet is stored and displayed in the live.dial and sent out the outlet.
- `assign(assign-input: float)` — Display and output a value
  The word assign, followed by a floating point value, causes that value to be displayed and sent out the live.dial object's outlet. The value, however, will not be stored. If the Parameter Visibility attribute is set to Stored Only, the assign message will not add the new value to the Live application’s undo chain.
- `init` — Restore and output the initial value
  Restores and outputs the initial value.
- `outputvalue` — Send the current value out the outlet
- `rawfloat(input: float)` — Store a raw normalized value, convert to real, display, and output
  A raw normalized value (between 0. and 1.) received in the inlet is converted to a real value, stored, displayed by live.dial, and the current value is sent out the outlet.
- `set(set-input: float)` — Set a value without causing output
  Sets the current value without causing any output.

## GUI behaviors

- `(mouse)` — Click and drag to change the dial value.
  Click and drag in the dial to change the value. Hold down the Shift key for more precise mouse control.

## Attributes

- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)
- `@paint` (int)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `active` — seen as: `active $1`
- `triangle` — seen as: `triangle $1`

## Help patcher examples

### appearance

> You can change the look of live.dial by adjusting the colors and appearance attributes. Note: Max for Live objects are not currently compatible with the style feature.

```
Example — [live.dial] (enum)
  fan-in:
    in0 ← [attrui @textcolor]
    in0 ← [attrui @tricolor]
    in0 ← [attrui @needlecolor]
    in0 ← [attrui @panelcolor]
    in0 ← [attrui @dialcolor]
    in0 ← [attrui @activeneedlecolor]
    in0 ← [attrui @bordercolor]
    in0 ← [attrui @activedialcolor]
    in0 ← [attrui @focusbordercolor]
    in0 ← [attrui @appearance]    # Change the appearance
    in0 ← [attrui @active]    # Toggle on to see active color settings.
    in0 ← [attrui @activefgdialcolor]
    in0 ← [attrui @fgdialcolor]
```

Attributes demonstrated: `@active`, `@activedialcolor`, `@activefgdialcolor`, `@activeneedlecolor`, `@appearance`, `@bordercolor`, `@dialcolor`, `@fgdialcolor`, `@focusbordercolor`, `@needlecolor`, `@panelcolor`, `@textcolor`, `@tricolor`

### basic

> You can change the unit style in the inspector of live.dial. These are set to % and Semitones.

```
Example — [live.dial] (Cutoff Frequency)
  fan-out:
    out0 → [flonum]:in0    # param value
    out1 → [flonum]:in0    # raw value (0.-1.)
```

```
Example — [live.dial] (Transpo)
  fan-out:
    out0 → [flonum]:in0    # param value
    out1 → [flonum]:in0    # raw value (0.-1.)
```

```
Example — [live.dial] (Basic)
  fan-in:
    in0 ← [message "active $1"]
    in0 ← [message "triangle $1"]
  fan-out:
    out0 → [flonum]:in0
```

## See also

`live.numbox`, `live.slider`, `dial`
