# minus

_max · Math_

_(reference XML aliased from `-`.)_

> Subtract two numbers, output the result

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Set Left Operand, Trigger the Calculation |
| in1 | Set Right Operand |
| out0 | Result = Left - Right |

## Arguments

- **initial** (`number`) _(optional)_ — Initial
  Sets the initial value to be subtracted from a number received in the left inlet. A float argument causes the numbers to be subtracted as floats.

## Messages

- `bang` — Output most recent calculation
  In left inlet: Performs the subtraction with the numbers currently stored. If there is no argument, - initially holds 0.
- `int(input: int)` — Subtract and cause output
  The number in the right inlet is subtracted from the number, and the result is sent out the outlet.
- `float(input: float)` — Subtract and cause output
  Converted to int, unless - has a float argument.
- `in1(value: int)` — Set subtraction value
  The number is stored, to be subtracted from, by a number received in the left inlet.
- `set(input: int)` — Subtract with no output
  Sets the number to be subtracted without causing output (bang will output it).
- `list(input: list)` — Set values and output difference
  In left inlet: The first number is subtracted from the second number, and the result is sent out the outlet.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — Result = Left - Right
> - `in0` — Set Left Operand, Trigger the Calculation
> - `in1` — Set Right Operand

## See also

`rminus`, `expr`
