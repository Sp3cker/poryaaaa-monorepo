# equals

_max · Math_

_(reference XML aliased from `==`.)_

> Compare numbers for equal-to condition

Compares two values to see if one value is equal to a second. Outputs a 1 if the number is equal to the comparison-number or 0 if it is not.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Set Left Operand, Trigger the Calculation |
| in1 | Set Right Operand |
| out0 | Result = Left == Right |

## Arguments

- **comparison-value** (`int or float`) _(optional)_ — Comparison value
  Sets the initial value, to be compared with a number received in the left inlet. A float argument forces a float comparison.

## Messages

- `bang` — Perform the comparison with the most recent values
  In left inlet: Performs the comparison with the numbers currently stored. If there is no argument, == initially holds 0 for comparison.
- `int(input: int)` — Compare to the stored value, cause output
  In left inlet: The number is compared with the number in the right inlet. If the two numbers are equal, == outputs 1. If they are not equal == outputs 0.
- `float(input: float)` — Compare to the stored value, cause output
  Converted to int before comparison, unless == has a float argument.
- `in1(comparison-value: int)` — Set the comparison value
  In right inlet: The number is stored to be compared with a number received in the left inlet.
- `set(input: int)` — Set the incoming value without output
  Sets the number to be compared without causing output (bang will output it).
- `list(input: number, comparison-value: number)` — Compare two numbers
  In left inlet: Compares first and second number, outputs 1 if they are equal, 0 if they are not equal.

## Attributes

- `@default` (atom)
- `@label` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — Result = Left == Right
> - `in0` — Set Left Operand, Trigger the Calculation
> - `in1` — Set Right Operand

### basic

> Input and arguments may be int or float

## See also

`select`, `split`, `!=`, `<`, `<=`, `>`, `>=`
