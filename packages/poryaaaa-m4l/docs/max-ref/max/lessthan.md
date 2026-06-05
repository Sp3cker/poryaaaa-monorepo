# lessthan

_max · Math_

_(reference XML aliased from `<`.)_

> Compare numbers for less than condition

Compares two values to see if one value is less than a second. Outputs a 1 if the number is less than the comparison-number or 0 if it is equal to or greater than it.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Set Left Operand, Trigger the Calculation |
| in1 | Set Right Operand |
| out0 | Result = Left < Right |

## Arguments

- **initial-comparison-value** (`number`) _(optional)_ — Set initial comparison number
  Sets the initial value, to be compared with a number received in the left inlet. A float argument forces a float comparison.

## Messages

- `bang` — Perform the comparison with the most recent values
  In left inlet: Performs the comparison with the numbers currently stored. If there is no argument, initially holds 0 for comparison.
- `int(input: int)` — Compare to the stored value, cause output
  In left inlet: If the number is less than the number in the right inlet, outputs 1. Otherwise, outputs 0.
- `float(input: float)` — See the int message
  Converted to int before comparison, unless has a float argument.
- `in1(comparison-value: int)` — Set the comparison value
  In right inlet: The number is stored to be compared with a number received in the left inlet.
- `set(set-input: int)` — Set the incoming value without output
  The word set followed by a number will set the input value without causing output.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — Result = Left < Right
> - `in0` — Set Left Operand, Trigger the Calculation
> - `in1` — Set Right Operand

### basic

> Input and arguments may be int or float

## See also

`!=`, `<=`, `==`, `>`, `>=`
