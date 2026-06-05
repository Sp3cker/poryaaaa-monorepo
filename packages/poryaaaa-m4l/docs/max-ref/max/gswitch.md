# gswitch

_max · U/I_

> Select output from two inlets

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | bang/int | Control Inlet (0=Left, 1=Right) |
| in1 | bang/int | Left Data Inlet |
| in2 | bang/int | Right Data Inlet |
| out0 | — | Output of Selected Inlet |

## Messages

- `bang` — Switch the current inlet
  In left inlet: Causes the connection to be made to the other inlet.
- `int(inlet: int)` — Select an inlet
  In left inlet: The number specifies which one of the other inlets is to be open. 0 specifies the middle inlet, any number other than 0 specifies the right inlet. The gswitch icon shows the open inlet.
- `float(inlet: float)` — Select an inlet
  In left inlet: Converted to int.
- `list(input: list)` — Send a list
  A list sent to one of the gswitch inlets will pass through if the appropriate connection is made.
- `anything(input: list)` — Send any message
  Any message sent to one of the gswitch inlets will pass through if the appropriate connection is made.

## GUI behaviors

- `(mouse)` — Switch the current inlet
  Clicking on gswitch causes the connection to be made to the other inlet.

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
Example — [gswitch]
  fan-in:
    in0 ← [attrui @style]    # Create and apply styles using the format palette or type a style name here
    in0 ← [attrui @bgcolor]
    in0 ← [attrui @color]
    in0 ← [attrui @elementcolor]
```

Attributes demonstrated: `@bgcolor`, `@color`, `@elementcolor`, `@style`

### basic

```
Example — [gswitch]
  fan-in:
    in0 ← [toggle]    # zero sets to middle input, non-zero sets to right
    in0 ← [button]    # bang to toggle
    in1 ← [number]
    in2 ← [number]    # try both inputs
    in2 ← [message "whatever works"]    # works with any input
  fan-out:
    out0 → [button]:in0
    out0 → [message "-9"]:in1
```

## See also

`gate`, `gswitch2`, `pictctrl`, `receive`, `route`, `router`, `switch`
