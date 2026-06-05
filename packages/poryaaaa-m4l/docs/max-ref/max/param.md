# param

__

> Define a polyphonic-compatible parameter in poly~

The param object appears as an attribute on the poly~ object, in addition to being published to the Parameter system, and can be attached to UI objects via the Parameter Connect feature. It currently only supports single numeric values (int/float), but not symbols nor lists.

 Parameters defined with the param object will only appear once in the Parameter Window for the top-level patcher containing the hosting poly~ object, regardless of how many voices of polyphony are configured, and changes will be propagated to all voices. This feature is useful for defining "monophonic" parameters which apply to every voice of a polyphonic structure (such as amplitude envelope attack).

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages or data in |
| out0 | value out |
| out1 | normalized value out |

## Arguments

- **name** (`symbol`) _(optional)_ — The parameter name
  If no name is provided, the parameter will be named param.

## Messages

- `bang` — Output the current value
  The current value of the object will be sent from the outlets.
- `int(value: int)` — Set the parameter value
  The current value of the object will be updated and sent from the outlets.
- `float(value: float)` — Set the parameter value
  The current value of the object will be updated and sent from the outlets.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)
- `@save` (int)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — value out
> - `out1` — normalized value out
> - `in0` — messages or data in

### basic

> The 'param' object's value is propagated to all of the poly~ object's voices when you change the dial.

## See also

`poly~`, `rnbo`, `gen~`, `amxd~`
