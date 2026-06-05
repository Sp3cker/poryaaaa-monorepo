# bitor

_max · Math_

_(reference XML aliased from `|`.)_

> Bitwise union of two numbers

Performs a bit-by-bit OR of two numbers (expressed in binary for the task). Outputs a number composed of all those bits which are 1 in either of the two numbers.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Set Left Operand, Trigger the Calculation |
| in1 | Set Right Operand |
| out0 | Result = Left | Right |

## Arguments

- **initial-value** (`int`) _(optional)_ — Operand value
  Sets an initial value to be OR-ed with a number received in the left inlet.

## Messages

- `bang` — Perform the calculation with the most recent values
  In left inlet: Performs the calculation with the numbers currently stored. If there is no argument, | initially holds 0.
- `int(input: int)` — Bitwise OR value, cause output
  In left inlet: Outputs a number composed of all those bits which are 1 in either of the two numbers.
- `float(input: float)` — Bitwise OR value, cause output
  Converted to int.
- `in1(comparison-number: int)` — Replace right operand
  In right inlet: The number is stored for combination with a number received in the left inlet.
- `set(set-input: int)` — Set the incoming value without output
  In left inlet: The word set followed by a number will set the input to the bitwise-or operation without causing output (a successive bang will output the result).
- `list(input: number, comparison-value: number)` — Bitwise OR two numbers
  In left inlet: Combines the first and second numbers bit-by-bit, and outputs a number composed of all those bits which are 1 in either of the two numbers.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — Result = Left | Right
> - `in0` — Set Left Operand, Trigger the Calculation
> - `in1` — Set Right Operand

## See also

`&`, `&&`, `||`
