# array.expr

_max · Array_

> Evaluate a math expression for an array

Performs mathematical calculations using C language-style mathematical operations. Operates on inputs that are arrays rather than collections of single values.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| in1 | array in |
| out0 | array result out |

## Arguments

- **expression** (`list`) — Mathematical expression
  The argument to the vexpr object is a mathematical expression composed of numbers, arithmetic operators such as + or *, comparisons such as or >, C functions such as min () or pow (), names of table objects, and changeable arguments ($i, $f, and $s) for ints, floats, and symbols received in the inlets.
- **constant** (`number`) — A numeric constant
  Numbers can be used as constants in the mathematical expression.
- **format** (`symbol`) — The data format for an inlets
  Changeable arguments that specify data formats associated with an inlet are described using a combination of a data type ($i or $f) and an inlet number (example: $i2). The argument will be replaced by numbers received in the specified inlet.
- **table** (`symbol`) — An accessible table
  Changeable arguments that specify accessing data from a table are described using the argument $s and an inlet number which is replaced by the name of a table to be accessed. The argument should be immediately followed by a number in brackets specifying an address in the table . (Examples: $s2[7] or $s3[$i1].)
- **(other)** (`symbol`) — An arithmetic operator
  The vexpr object understands the following arithmetic operators: +, -, *, /, %. Other operators are ~ (one's complement), ^ (bitwise exclusive or), &, &&, |, ||, and ! (not).

 Many C language math functions can be understood by vexpr. A function must be followed immediately by parentheses containing any arguments necessary to the function. If the function requires a comma between arguments, the comma must be preceded by a backslash (\) so that Max will not be confused by it. For example: (pow ($i1\,2) + $f2).

 C language functions understood by vexpr are: abs, min, max, sin, cos, tan, asin, acos, atan, atan2, sinh, cosh, tanh, int (convert to integer), float (convert to float), pow, sqrt, fact (factorial), exp (power of e to x), log10 (log), ln or log (natural log), and random. Additional functions can be added by means of external code resources placed in Max's startup folder.

 The array.expr also understands the special function arrayidx() (no arguments) which provides an int representing the current array index being processed.

## Messages

- `bang` — Trigger output
  Reprocess previously received arrays and trigger output.
- `int(value: int)` — Convert an integer to an array.
  Convert an incoming integer to an array, then process as described for the array message.
- `float(value: float)` — Convert a floating-point number to an array.
  Convert an incoming floating-point number to an array, then process as described for the array message.
- `list(list-value: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `anything(list-value: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `array(ARG_NAME_0: list)` — Evaluate the expression, cause output
  The elements of each array are used individually, in order from left to right, to replace the changeable argument in a series of evaluations of the expression. When an array is received in the left inlet, the expression is first evaluated using the first element of each array, then using the second element of each array, etc. The series of results of these evaluations is then sent out as a array.
  If the arrays are not of the same length, the fillmode will be used to determine how to handle the length discrepancy.
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Attributes

- `@fillmode` (int) — Fill Mode
  The fillmode determines how to resize arrays which don't match the length of the array sent into the left inlet. Several options are available.
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### basic

```
Example #1 — [array.expr $f1 * $f2 @fillmode 3]
  fan-in:
    in0 ← [message "1 2 3 4"]
    in1 ← [message "0. 1"]    # @fillmode automatically resizes the non-left inputs to match the left-hand input. @fillmode 3 (linear interpolation) does this by interpolating across the array elements, similar to array.fill
  fan-out:
    out0 → [print expr_mode2 @popup 1]:in0
```

```
Example #2 — [array.expr $f1 * $f2]
  fan-in:
    in0 ← [message "1 2 3 4"]
    in1 ← [message "10 11 12 13"]    # generate a nested array for expring
  fan-out:
    out0 → [print expr @popup 1]:in0
```

## See also

`array`, `array.fill`, `expr`, `vexpr`
