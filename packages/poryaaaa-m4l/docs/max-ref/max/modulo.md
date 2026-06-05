# modulo

_max · Math_

_(reference XML aliased from `%`.)_

> Divide two numbers, output the remainder

% takes two numbers, divides one by the other and outputs the remainder of the division.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Set Left Operand, Trigger the Calculation |
| in1 | Set Right Operand |
| out0 | Result = Left % Right |

## Arguments

- **initial-value** (`int`) _(optional)_ — Initial value
  Sets an initial value for the divisor. If there is no argument, the divisor is set to 1 initially.

## Messages

- `bang` — In left inlet: Performs the operation with the numbers currently stored.
- `int(input: int)` — In left inlet: The number is divided by the number in the right inlet, and the remainder is sent out the outlet.
- `float(input: float)` — Converted to int.
- `in1(divisor: int)` — In right inlet: The number is stored as the divisor (the number to be divided into the number in the left inlet)
  In right inlet: The number is stored as the divisor (the number to be divided into the number in the left inlet) for calculating the remainder.
- `set(set-input: int)` — Sets the number to be divided without causing output (bang will output it).
- `list(number-divided and divisor: list)` — In left inlet: The first number is divided by the second number, and the remainder is sent out the outlet.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — Result = Left % Right
> - `in0` — Set Left Operand, Trigger the Calculation
> - `in1` — Set Right Operand

## See also

`expr`, `!/`, `/`
