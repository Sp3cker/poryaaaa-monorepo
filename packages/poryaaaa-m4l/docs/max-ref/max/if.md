# if

_max · Control, Math_

> Conditional statement in if/then/else form

Evaluates input according to a conditional statement specified in an if-then-else form.

## Arguments

- **if** (`symbol`) — Conditional statement
  The arguments for the if object start with a conditional statement that uses the same syntax as expr. The word then follows the conditional statement, which is then followed by a message expression. After the message expression, there is an optional else and a second message expression.

 if evaluates the conditional expression, and if the result is non-zero, evaluates the message expression after the word then. Otherwise, it evaluates the second message expression after the word else if an else message is provided.
- **then, else** (`symbol`) — Message expressions
  Message expressions are similar to what you type into a message box.
- **$i1, $f1, $s1** (`symbol`) — Variable arguments for replacement
  You use $i1, $f1, or $s1 instead of $1 for replaceable arguments. The number of inlets is determined by how many different changeable arguments are typed in. The maximum number of inlets is 9.
- **send** (`symbol`) — Send messages to receive objects
  No commas or semicolons are allowed. Messages can be sent to remote receive objects by preceding the message expression with send, followed by the name of the receive object.
- **out2** (`symbol`) — Output from second outlet
  The keyword out2 in a message expression creates a second, right outlet for the if object. If out2 precedes a message expression, the result of the expression is sent out the right outlet instead of the left outlet.

## Messages

- `bang` — Evaluate the expression
  In left inlet: Evaluates the conditional statement using the values currently stored.
- `int(input: int)` — Replace a value and evaluate expression
  The number in each inlet will be stored in place of the $i or $f argument associated with it, and the expression will be evaluated.
- `float(input: float)` — Replace a value and evaluate expression
  The number in each inlet will be stored in place of the $i or $f argument associated with it, and the expression will be evaluated.
- `ft1(input: float)` — Replace the $f1 value
- `ft2(input: float)` — Replace the $f2 value
- `ft3(input: float)` — Replace the $f3 value
- `ft4(input: float)` — Replace the $f4 value
- `ft5(input: float)` — Replace the $f5 value
- `ft6(input: float)` — Replace the $f6 value
- `ft7(input: float)` — Replace the $f7 value
- `ft8(input: float)` — Replace the $f8 value
- `ft9(input: float)` — Replace the $f9 value
- `in1(input: int)` — Replace the $i1 value
- `in2(input: int)` — Replace the $i2 value
- `in3(input: int)` — Replace the $i3 value
- `in4(input: int)` — Replace the $i4 value
- `in5(input: int)` — Replace the $i5 value
- `in6(input: int)` — Replace the $i6 value
- `in7(input: int)` — Replace the $i7 value
- `in8(input: int)` — Replace the $i8 value
- `in9(input: int)` — Replace the $i9 value
- `set(set-input: list)` — Replace values without evaluation
  The word set, followed by one or more numbers, treats those numbers as if each had come in a different inlet, replacing the stored value with the new value, but the conditional statement is not evaluated and nothing is sent out the outlet. If there are fewer numbers in the message than there are inlets, the stored value in each remaining inlet is left unchanged.
- `symbol(input: symbol)` — Replace a value and evaluate expression
  Symbols can only be received in the first inlet. If received in the first inlet, a symbol will be stored in place of the $s argument associated with it, and the expression will be evaluated, with the exception that you cannot do comparisons or use other operators with symbols. If the symbol shares the name of a table object, you can specify a position in the table to be evaluated.

## Help patcher examples

### symbols

> Symbols can only be received in the first inlet. If received in the first inlet, a symbol will be stored in place of the $s argument associated with it, and the expression will be evaluated, with the exception that you cannot do comparisons or use other operators with symbols. If the symbol shares the name of a table object, you can specify a position in the table to be evaluated.

```
Example — [if $s1[0] == 0 then yep else nope]  the first position in the referenced table is evaluated
  fan-in:
    in0 ← [message "symbol trippin"]    # the symbol value is $s1, which refers to a table object
    in0 ← [message "symbol slippin"]
  fan-out:
    out0 → [message "yep"]:in1
```

### conditional options

```
Example #1 — [if ($i1 > 0) || ($i1 < 0) then nonzero else zero]
  fan-in:
    in0 ← [number]    # multiple conditions
  fan-out:
    out0 → [message "nonzero"]:in1
```

```
Example #2 — [if $i1 > 0 then $i1 else out2 $i1]
  fan-in:
    in0 ← [number]    # out2
  fan-out:
    out0 → [number]:in0
    out1 → [number]:in0
```

```
Example #3 — [if $i1 > 0 then 1 else 0]
  fan-in:
    in0 ← [number]    # if/then/else form
  fan-out:
    out0 → [number]:in0
```

```
Example #4 — [if $i1 > 0 then $i1]
  fan-in:
    in0 ← [number]    # if/then form
  fan-out:
    out0 → [number]:in0
```

### basic

```
Example #1 — [if $f1 > 0. then 1. else -1.]
  fan-in:
    in0 ← [flonum]    # a floating point value is $f1
  fan-out:
    out0 → [flonum]:in0
```

```
Example #2 — [if $i1 < $i2 then less else not less]
  fan-in:
    in0 ← [number]    # the first integer value is $i1
    in0 ← [trigger b i] ← [number]    # the second integer value is $i2
    in1 ← [trigger b i] ← [number]    # the second integer value is $i2
  fan-out:
    out0 → [message "less"]:in1
```

## See also

`!=`, `<`, `<=`, `==`, `>`, `>=`, `expr`, `select`
