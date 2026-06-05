# logand

_max · Math_

_(reference XML aliased from `&&`.)_

> Perform a logical AND

Compares one number to another and outputs a 1 if the two numbers are both non-zero or a 0 if either number is 0.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Set Left Operand, Trigger the Calculation |
| in1 | Set Right Operand |
| out0 | Result = Left && Right |

## Arguments

- **comparison-value** (`int`) _(optional)_ — Comparison value
  Sets an initial value to be stored by &&. A number in the right inlet changes the value set by the argument.

## Messages

- `bang` — Perform the comparison with the most recent values
  In left inlet: Performs the operation with the numbers currently stored. If there is no argument, && initially holds 0.
- `int(input: int)` — Compare to the stored value, cause output
  If the number in both inlets is not 0, then the output is 1. If the number in one or both of the inlets is 0, then the output is 0. A number in the left inlet triggers the output.
- `float(input: float)` — Compare to the stored value, cause output
  Converted to int.
- `in1(comparison-value: int)` — Set the comparison value
  In right inlet: The number is compared to the number in the left inlet to determine output but only a number in the left inlet will trigger output.
- `set(set-input: int)` — Set the incoming value without output
  The word set followed by a number will set the input value without causing output.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — Result = Left && Right
> - `in0` — Set Left Operand, Trigger the Calculation
> - `in1` — Set Right Operand

## See also

`&`, `|`, `||`
