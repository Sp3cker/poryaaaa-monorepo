# bitand

_max · Math_

_(reference XML aliased from `&`.)_

> Bitwise intersection of two numbers

Performs a bit-by-bit AND of two numbers as expressed in binary. Outputs a number composed of all those bits which are 1 in both of the two numbers.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Set Left Operand, Trigger the Calculation |
| in1 | Set Right Operand |
| out0 | Result = Left & Right |

## Arguments

- **initial-value** (`int`) _(optional)_ — Operand value
  Sets an initial value to be AND-ed with a number received in the left inlet.

## Messages

- `bang` — Perform the calculation with the most recent values
  In left inlet: Performs the comparison with the numbers currently stored. If there is no argument, & initially holds 0 for comparison.
- `int(input: int)` — Bitwise AND value, cause output
  In left inlet: The number is compared, in binary form, with the number in the right inlet. The output is a number composed of those bits which are 1 in both numbers.
- `float(input: float)` — Bitwise AND value, cause output
  Converted to int.
- `in1(comparison-number: int)` — Replace right operand
  In right inlet: The number is stored for comparison with a number received in the left inlet.
- `set(set-input: int)` — Set the incoming value without output
  In left inlet: The word set followed by a number will set the input to the bitwise-and operation without causing output (a successive bang will output the result).
- `list(input: number, comparison-value: number)` — Bitwise AND two numbers
  In left inlet: Compares the first and second numbers bit-by-bit, and outputs a number composed of those bits which are 1 in both numbers.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — Result = Left & Right
> - `in0` — Set Left Operand, Trigger the Calculation
> - `in1` — Set Right Operand

## See also

`&&`, `|`, `||`
