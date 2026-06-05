# gswitch2

_max · U/I_

_(reference XML aliased from `ggate`.)_

> Send input to one of two outlets

Switches the right inlet between two output pathways.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | bang/int | Control Inlet (0=Left, 1=Right) |
| in1 | bang/int | Data inlet |
| out0 | — | Left outlet |
| out1 | — | Right outlet |

## Messages

- `bang` — Switch the current outlet
  Causes the connection to be made to the other outlet. Clicking on the graphic display with the mouse has the same effect.
- `int(outlet: int)` — Select an outlet
  The number specifies which one of the two outlets is to be open. 0 specifies the left outlet, any number other than 0 specifies the right outlet. The graphical icon shows the open outlet.
- `float(outlet: float)` — Select an output
  In left inlet: Converted to int.

## GUI behaviors

- `(mouse)` — Switch the current outlet
  Clicking on gswitch2 causes the connection to be made to the other outlet.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)
- `@save` (int)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `whatever` — seen as: `whatever works`

## Help patcher examples

### appearance

```
Example — [gswitch2]
  fan-in:
    in0 ← [attrui @style]    # Create and apply styles using the format palette or type a style name here
    in0 ← [attrui @bgcolor]
    in0 ← [attrui @color]
    in0 ← [attrui @elementcolor]
```

Attributes demonstrated: `@bgcolor`, `@color`, `@elementcolor`, `@style`

### basic

```
Example — [gswitch2]
  fan-in:
    in0 ← [toggle]    # zero sets to left output, non-zero sets to right
    in0 ← [button]    # bang to toggle
    in1 ← [number]
    in1 ← [message "whatever works"]    # works with any input
  fan-out:
    out0 → [button]:in0
    out0 → [message "-2"]:in1
    out1 → [button]:in0
    out1 → [message "-9"]:in1
```

## See also

`gate`, `gswitch`, `onebang`, `pictctrl`, `route`, `router`, `send`, `switch`
