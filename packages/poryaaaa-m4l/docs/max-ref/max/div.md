# div

_max · Math_

_(reference XML aliased from `/`.)_

> Divide two numbers

Divides two numbers (according to the specified divisor assignment), and then outputs the result.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Set Left Operand, Trigger the Calculation |
| in1 | Set Right Operand |
| out0 | Result = Left / Right |

## Arguments

- **initial** (`int or float`) _(optional)_ — Initial divisor
  Sets an initial value for the divisor. If there is no argument, the divisor is set to 1 initially. A float argument causes the numbers to be divided as floats. (Division by 0 is not allowed. Int division by 0 will have the same result as dividing by 1. Float division by 0 will always cause an output of -2

 31

 .)

## Messages

- `bang` — Output most recent calculation
  In left inlet: Performs the division with the numbers currently stored.
- `int(input: int)` — Divide and output results
  In left inlet: The number is divided by the number in the right inlet, and the result is sent out the outlet.
- `float(input: float)` — Divide and output results
  Converted to int, unless / has a float argument.
- `in1(divisor: int)` — Set the divisor
  In right inlet: The number is stored as the divisor (the number to be divided into the number in the left inlet).
- `set(input: int)` — Divide with no output
  Sets the number to be divided without causing output (bang will output it).
- `list(input and divisor: list)` — Set both values, output calculation
  In left inlet: The first number is divided by the second number, and the result is sent out the outlet.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — Result = Left / Right
> - `in0` — Set Left Operand, Trigger the Calculation
> - `in1` — Set Right Operand

## See also

`rdiv`, `expr`, `%`
