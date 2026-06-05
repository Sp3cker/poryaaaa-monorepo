# expr

_max · Math_

> Evaluate a mathematical expression

Evaluate an expression using a C-like language. Variables and operators are used to create output values.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Evaluate Expression, int $i1, float $f1, table $s1 |
| out0 | Expression Result |

## Arguments

- **expression** (`list`) — Mathematical expression
  The argument to the expr object is a mathematical expression composed of numbers, arithmetic operators such as + or *, comparisons such as or >, C functions such as min () or pow (), names of table objects, and changeable arguments ($i, $f, and $s) for ints, floats, and symbols received in the inlets.
- **constant** (`number`) — A numeric constant
  Numbers can be used as constants in the mathematical expression.
- **inlet-format** (`symbol`) — Inlet format
  Changeable arguments that specify data formats associated with an inlet are described using a combination of a data type ($i or $f) and an inlet number (example: $i2). The argument will be replaced by numbers received in the specified inlet.
- **table-info** (`$s`) — Table to access
  Changeable arguments that specify accessing data from a table are described using the argument $s and an inlet number which is replaced by the name of a table to be accessed. The argument should be immediately followed by a number in brackets specifying an address in the table . (Examples: $s2[7] or $s3[$i1].)
- **(other)** (`symbol`) — Arithmetic operator
  The expr object understands the following arithmetic operators: +, -, *, /, %. Other operators are ~ (one's complement), ^ (bitwise exclusive or), &, &&, |, ||, and ! (not).

## Messages

- `bang` — Evaluate with most recent values
  In left inlet: Evaluates the expression using the values currently stored.
- `int(input: int)` — Replace an argument and evaluate
  The number received in each inlet will be stored in place of the $i or $f argument associated with it. (Example: The number in the second inlet from the left will be stored in place of the $i2 and $f2 arguments, wherever they appear.)
- `float(input: float)` — Replace an argument and evaluate
  The number in each inlet will be stored in place of the $f or $i argument associated with it. The number will be truncated by a $i argument.
- `ft1(input: float)` — Replace an argument and evaluate
  See the float listing.
- `ft2(input: float)` — Replace an argument and evaluate
  See the float listing.
- `ft3(input: float)` — Replace an argument and evaluate
  See the float listing.
- `ft4(input: float)` — Replace an argument and evaluate
  See the float listing.
- `ft5(input: float)` — Replace an argument and evaluate
  See the float listing.
- `ft6(input: float)` — Replace an argument and evaluate
  See the float listing.
- `ft7(input: float)` — Replace an argument and evaluate
  See the float listing.
- `ft8(input: float)` — Replace an argument and evaluate
  See the float listing.
- `ft9(input: float)` — Replace an argument and evaluate
  See the float listing.
- `in1(input: int)` — Replace an argument and evaluate
  See the int listing.
- `in2(input: int)` — Replace an argument and evaluate
  See the int listing.
- `in3(input: int)` — Replace an argument and evaluate
  See the int listing.
- `in4(input: int)` — Replace an argument and evaluate
  See the int listing.
- `in5(input: int)` — Replace an argument and evaluate
  See the int listing.
- `in6(input: int)` — Replace an argument and evaluate
  See the int listing.
- `in7(input: int)` — Replace an argument and evaluate
  See the int listing.
- `in8(input: int)` — Replace an argument and evaluate
  See the int listing.
- `in9(input: int)` — Replace an argument and evaluate
  See the int listing.
- `set(input: list)` — "Unpack" a list and distribute it elements without evaluation
  If the word set precedes the items of a list received in the left inlet, the list of numbers are treated as if each had come in a different inlet, replacing the stored value with the new value. However, the expression is not evaluated and nothing is sent out the outlet. If there are fewer numbers in the message than there are inlets, the stored value in each remaining inlet stays unchanged.
- `sm1(table: list)` — See the symbol listing
- `sm2(table: list)` — See the symbol listing
- `sm3(table: list)` — See the symbol listing
- `sm4(table: list)` — See the symbol listing
- `sm5(table: list)` — See the symbol listing
- `sm6(table: list)` — See the symbol listing
- `sm7(table: list)` — See the symbol listing
- `sm8(table: list)` — See the symbol listing
- `sm9(table: list)` — See the symbol listing
- `symbol(table: list)` — Access values stored in a table
  The word symbol, followed by the name of a table, will be stored in place of the $s argument associated with that inlet, for accessing values stored in the table object.
- `list(input: list)` — "Unpack" a list, distribute elements, and evaluate
  The items of a list received in the left inlet are treated as if each had come in a different inlet, and the expression is evaluated. If the list contains fewer items than there are inlets, the most recently received value in each remaining inlet is used. Any of the above messages in the left inlet will evaluate the expression and send out the result. If a value has never been received for each changeable argument, that value is considered 0 when the expression is evaluated. The number of inlets is determined by how many changeable arguments are typed in. The maximum number of inlets is 9.

## Help patcher examples

### numbers

> Random numbers:

> Find the minimum or maximum of two numbers:

> noise() outputs random float between 0 - 1; multiply output to generate a value within a range

```
Example #1 — [expr noise() * $f1]
  fan-in:
    in0 ← [flonum] ← [button]
    in0 ← [message "2.5"]
  fan-out:
    out0 → [flonum]:in0
```

```
Example #2 — [expr random($f1\, $f2)]
  fan-in:
    in0 ← [message "8 10"]    # between 8 and 9
    in0 ← [flonum]
    in0 ← [button]    # first argument is low end of range (inclusive)
    in0 ← [message "0 10"]    # between 0 and 9 / examples (click repeatedly)
    in1 ← [flonum] ← [loadmess 127]    # second argument is high end of range (exclusive -- highest value will be one less than the second argument)
  fan-out:
    out0 → [flonum]:in0
```

```
Example #3 — [expr $i1]
  fan-in:
    in0 ← [number]    # Converting from int to float; useful for changing output type
  fan-out:
    out0 → [typeroute~]:in0
    out0 → [number]:in0
```

```
Example #4 — [expr float($i1)]
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [flonum]:in0
    out0 → [typeroute~]:in0
```

```
Example #5 — [expr ceil($f1)]
  fan-in:
    in0 ← [flonum]
    in0 ← [message "0.5"]
    in0 ← [message "-1.5"]
    in0 ← [message "2.2"]
    in0 ← [message "3.9"]
    in0 ← [message "-3.9"]    # Converting between from float to int:
  fan-out:
    out0 → [flonum]:in0    # upper int
```

```
Example #6 — [expr floor($f1)]
  fan-in:
    in0 ← [flonum]
    in0 ← [message "0.5"]
    in0 ← [message "-1.5"]
    in0 ← [message "2.2"]
    in0 ← [message "3.9"]
    in0 ← [message "-3.9"]    # Converting between from float to int:
  fan-out:
    out0 → [flonum]:in0    # lower int
```

```
Example #7 — [expr round($f1)]
  fan-in:
    in0 ← [flonum]
    in0 ← [message "0.5"]
    in0 ← [message "-1.5"]
    in0 ← [message "2.2"]
    in0 ← [message "3.9"]
    in0 ← [message "-3.9"]    # Converting between from float to int:
  fan-out:
    out0 → [flonum]:in0    # nearest int
```

```
Example #8 — [expr int($f1)]
  fan-in:
    in0 ← [flonum]
    in0 ← [message "0.5"]
    in0 ← [message "-1.5"]
    in0 ← [message "2.2"]
    in0 ← [message "3.9"]
    in0 ← [message "-3.9"]    # Converting between from float to int:
  fan-out:
    out0 → [flonum]:in0    # "integer part"
```

```
Example #9 — [expr max($f1\, $f2)]
  fan-in:
    in0 ← [message "50 60"]
    in0 ← [flonum]    # first
    in1 ← [flonum]    # second
  fan-out:
    out0 → [flonum]:in0
```

```
Example #10 — [expr min($f1\, $f2)]
  fan-in:
    in0 ← [message "50 60"]
    in0 ← [flonum]    # first
    in1 ← [flonum]    # second
  fan-out:
    out0 → [flonum]:in0
```

### bitwise

```
Example #1 — [expr 1 << $i1]  <<
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [number]:in0    # decimal
```

```
Example #2 — [expr 4096 >> $i1]  >>
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [number]:in0    # decimal
```

```
Example #3 — [expr $i1 ^ 255]  ^
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [number]:in0    # decimal
```

```
Example #4 — [expr ~$i1]  ~
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [number]:in0    # decimal
```

```
Example #5 — [expr $i1 | 3]  |
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [number]:in0    # decimal
```

```
Example #6 — [expr $i1 & 1]  &
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [number]:in0    # bitwise and
```

### operators

```
Example #1 — [expr $i1 >= 10]  >=
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [number]:in0    # greater than or equals
```

```
Example #2 — [expr !($i1 > 10 || $i1 < 0)]  !
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [number]:in0    # logical not -- what is true is false and what is false is true
```

```
Example #3 — [expr $i1 > 10 || $i1 < 0]  ||
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [number]:in0    # logical or -- either can be true
```

```
Example #4 — [expr $i1 > 10 && $i1 < 20]  &&
  fan-in:
    in0 ← [number]    # Logical...
  fan-out:
    out0 → [number]:in0    # logical and -- both must be true
```

```
Example #5 — [expr $i1 == 10]  ==
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [number]:in0    # equals
```

```
Example #6 — [expr $i1 <= 10]  <=
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [number]:in0    # less than or equals
```

```
Example #7 — [expr $i1 != 10]  !=
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [number]:in0    # not equals
```

```
Example #8 — [expr $i1 < 10]  <
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [number]:in0    # less than
```

```
Example #9 — [expr $i1 > 10]  >
  fan-in:
    in0 ← [number]    # Comparison...
  fan-out:
    out0 → [number]:in0    # greater than
```

```
Example #10 — [expr $i1 - 10]  -
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [number]:in0
```

```
Example #11 — [expr $i1 % 10]  %
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [number]:in0
```

```
Example #12 — [expr $i1 / 10]  /
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [number]:in0
```

```
Example #13 — [expr $i1 * 10]  *
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [number]:in0
```

```
Example #14 — [expr $i1 + 10]  +
  fan-in:
    in0 ← [number]    # Arithmetic...
  fan-out:
    out0 → [number]:in0
```

### functions

> Trigonometric functions:

```
Example #1 — [expr fact($i1)]
  fan-in:
    in0 ← [flonum]    # Exponential and logarithmic functions:
  fan-out:
    out0 → [number]:in0    # factorial
```

```
Example #2 — [expr sqrt($f1)]
  fan-in:
    in0 ← [flonum]    # Exponential and logarithmic functions:
  fan-out:
    out0 → [flonum]:in0    # square root
```

```
Example #3 — [expr pow($f1\, $f2)]
  fan-in:
    in0 ← [flonum]    # Exponential and logarithmic functions:
    in1 ← [flonum]    # Y
  fan-out:
    out0 → [flonum]:in0    # X raised to the Y power
```

```
Example #4 — [expr exp($f1)]
  fan-in:
    in0 ← [flonum]    # Exponential and logarithmic functions:
  fan-out:
    out0 → [flonum]:in0    # e to the N
```

```
Example #5 — [expr ln($f1)]
  fan-in:
    in0 ← [flonum]    # Exponential and logarithmic functions:
  fan-out:
    out0 → [flonum]:in0    # natural logarithm
```

```
Example #6 — [expr log10($f1)]
  fan-in:
    in0 ← [flonum]    # Exponential and logarithmic functions:
  fan-out:
    out0 → [flonum]:in0    # base 10 logarithm
```

```
Example #7 — [expr tanh($f1)]
  fan-in:
    in0 ← [slider]    # input is in radians / 2π / 0
  fan-out:
    out0 → [flonum]:in0
```

```
Example #8 — [expr cosh($f1)]
  fan-in:
    in0 ← [slider]    # input is in radians / 2π / 0
  fan-out:
    out0 → [flonum]:in0
```

```
Example #9 — [expr sinh($f1)]
  fan-in:
    in0 ← [slider]    # input is in radians / 2π / 0
  fan-out:
    out0 → [flonum]:in0
```

```
Example #10 — [expr atan2($f1\, $f2)]
  fan-in:
    in0 ← [message "1 1"]
    in0 ← [message "-1 -1"]    # compare with atan...
  fan-out:
    out0 → [flonum]:in0
```

```
Example #11 — [expr atan($f1)]
  fan-in:
    in0 ← [slider]    # input is in radians / 1 / π/2 / 0
    in0 ← [message "1 1"]
    in0 ← [message "-1 -1"]    # compare with atan...
  fan-out:
    out0 → [flonum]:in0
```

```
Example #12 — [expr acos($f1)]
  fan-in:
    in0 ← [slider]    # input is in radians / 1 / π/2 / 0
  fan-out:
    out0 → [flonum]:in0
```

```
Example #13 — [expr asin($f1)]
  fan-in:
    in0 ← [slider]    # input is in radians / 1 / π/2 / 0
  fan-out:
    out0 → [flonum]:in0
```

```
Example #14 — [expr tan($f1)]
  fan-in:
    in0 ← [slider]    # input is in radians / 2π / 0
  fan-out:
    out0 → [flonum]:in0
```

```
Example #15 — [expr cos($f1)]
  fan-in:
    in0 ← [slider]    # input is in radians / 2π / 0
  fan-out:
    out0 → [flonum]:in0
```

```
Example #16 — [expr sin($f1)]
  fan-in:
    in0 ← [slider]    # input is in radians / 2π / 0
  fan-out:
    out0 → [flonum]:in0
```

### tables

> table-based functions like sum() and store() need to have what is inside the brackets evaluate to -1

```
Example #1 — [expr store(tab3[-1]\,$i1\,$i2)]
  fan-in:
    in0 ← [number]
    in1 ← [number]
  fan-out:
    out0 → [number]:in0
```

```
Example #2 — [expr sum($s1[-1])]
  fan-in:
    in0 ← [message "symbol tab1"]
    in0 ← [message "symbol tab2"]
  fan-out:
    out0 → [number]:in0
```

```
Example #3 — [expr $s2[$i1]]
  fan-in:
    in0 ← [number]
    in1 ← [message "symbol tab1"]
    in1 ← [message "symbol tab2"]
  fan-out:
    out0 → [number]:in0
```

Attributes demonstrated: `@name`

### basic

> Expression arguments can be either int ($i1 - $i9) or float ($f1 - $f9). Compare what happens:

```
Example #1 — [expr $i3 * $i1 + $i2]
  fan-in:
    in0 ← [message "1 2 3"]    # Use parentheses to set operator precedence. Compare these two expressions:
  fan-out:
    out0 → [flonum]:in0    # multiplication is performed before addition unless there are parentheses
```

```
Example #2 — [expr $i3 * ($i1 + $i2)]
  fan-in:
    in0 ← [message "1 2 3"]
  fan-out:
    out0 → [flonum]:in0
```

```
Example #3 — [expr $i1 * $i2 * $i3]
  fan-in:
    in0 ← [button]
    in0 ← [message "1 2 3"]
    in0 ← [message "set 10 20 30"]    # set does not trigger the calcuation; list does trigger it
  fan-out:
    out0 → [flonum]:in0
```

```
Example #4 — [expr $i1 * $i2 * 0.1]
  fan-in:
    in0 ← [flonum] ← [message "10"]
    in1 ← [flonum] ← [message "5.3"]
  fan-out:
    out0 → [flonum]:in0
```

```
Example #5 — [expr $i1 + $i2 * $i3]  an inlet is created for each argment
  fan-in:
    in0 ← [button]    # bang or leftmost inlet triggers the calculation:
    in0 ← [number]
    in1 ← [number]
    in2 ← [number]
  fan-out:
    out0 → [number]:in0
```

```
Example #6 — [expr $f1 * $f2 * 0.1]
  fan-in:
    in0 ← [flonum] ← [message "10"]
    in1 ← [flonum] ← [message "5.3"]
  fan-out:
    out0 → [flonum]:in0
```

## See also

`if`, `vexpr`, `round`
