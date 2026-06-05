# bangbang

_max · Right-to-Left_

> Output a bang from many outlets

Outputs bang messages out of each outlet (in right-to-left order) when it receives any input. The number of outlets is determined by an argument.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Any Message, Repeated as One or More bangs |
| out0 | Last bang Output |
| out1 | First bang Output |

## Arguments

- **outlets** (`number`) _(optional)_ — Define the number of outlets
  Sets the number of outlets. The number of outlets can be any number between 1 and 40. Floats are converted to ints.

## Messages

- `bang` — Send bang out all outlets
  Causes a bang to be sent out all outlets in right-to-left order.
- `int(input: int)` — Send bang out all outlets
  Causes a bang to be sent out all outlets in right-to-left order.
- `float(input: float)` — Send bang out all outlets
  Causes a bang to be sent out all outlets in right-to-left order.
- `anything(input: list)` — Send bang out all outlets
  Causes a bang to be sent out all outlets in right-to-left order.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `pure` — seen as: `pure nonsense`

## Help patcher examples

### basic

```
Example — [bangbang 3]
  fan-in:
    in0 ← [button]    # trigger output
    in0 ← [message "pure nonsense"]    # same as bang
    in0 ← [message "0.1111 90 4"]    # same as bang
  fan-out:
    out0 → [print @popup 1]:in0
    out0 → [button]:in0
    out1 → [print @popup 1]:in0
    out1 → [button]:in0
    out2 → [print @popup 1]:in0
    out2 → [button]:in0
```

## See also

`button`, `jstrigger`, `trigger`
