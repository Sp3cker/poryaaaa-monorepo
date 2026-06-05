# accum

_max · Math_

> Store, add to, and multiply a number

Stores a value (int or float), then adds or multiplies into it. If the argument is an integer, the multiplication is done in floating-point then converted to integer.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | int Sets and Outputs Value, bang Outputs Value |
| in1 | Add Input to Current Value |
| in2 | Multiplies Current Value by Input |
| out0 | Accumulator Output |

## Arguments

- **initial** (`int or float`) _(optional)_ — Initial stored value
  Sets the initial value stored in accum. An argument with a decimal point causes the value to be stored as a float.

## Messages

- `bang` — Output the stored value
  In left inlet: Outputs the value currently stored in accum.
- `int(input: int)` — Store value and cause output
  In left inlet: Replaces the value stored in accum, and sends the new value out the outlet.
- `float(input: float)` — Function depends on inlet
  In left and middle inlet: Converted to int, unless accum has a float argument.
  In right inlet: Multiplication is done with floats, even if the value is stored as an int.
- `ft1(input: float)` — Add to the current value without triggering output
  In left inlet: The message ft1, followed by a number, adds the number to the stored value without triggering output.
- `in1(input: int)` — Add to the stored value with no output
  In middle inlet: The number is added to the stored value, without triggering output.
- `in2(input: float)` — Multiply by stored value with no output
  In right inlet: The stored value is multiplied by the input, without triggering output.
- `set(input: int)` — Set the stored value with no output
  In left inlet: The word set, followed by a number, sets the stored value to that number, without triggering output.

## Help patcher examples

### basic

```
Example #1 — [accum 0.]  can also use floats
  fan-in:
    in0 ← [flonum]
    in0 ← [trigger b 0.1] ← [button]    # add 0.1 and output
    in1 ← [trigger b 0.1] ← [button]    # add 0.1 and output
  fan-out:
    out0 → [flonum]:in0
```

```
Example #2 — [accum 0]
  fan-in:
    in0 ← [number]    # sets and outputs number
    in0 ← [message "set $1"]
    in0 ← [button]    # outputs current number
    in1 ← [message "1"]
    in1 ← [message "2"]
    in1 ← [message "-1"]    # add to current number
    in2 ← [message "-1"]    # multiply current number
    in2 ← [message "0.5"]
    in2 ← [message "2"]
  fan-out:
    out0 → [number]:in0
```

## See also

`counter`, `float`, `int`
