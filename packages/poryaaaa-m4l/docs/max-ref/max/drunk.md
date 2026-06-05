# drunk

_max · Math_

> Output random numbers within a step range

Performs a "drunken" walk by outputting random numbers within a specified step range.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | bang Outputs Random Step, int Sets Value |
| in1 | Sets Range Size |
| in2 | Sets Step Size |
| out0 | Random Walk Output |

## Arguments

- **maximum value** (`int/float`) _(optional)_ — Set the maximum number for output
  Sets the maximum number produced. If this argument is not provided, the maximum value is set to 128. In floatoutput mode, the default maximum value is 1.. By default, the minimum value produced is 0, but this can be overridden with the range attribute.

 If this argument is a floating-point value, floatoutput will be enabled, and the object will generate floating-point output.
- **step size** (`int/float`) _(optional)_ — Set the step size
  Sets an initial limit on the size of random steps taken; the absolute value of the step size will always be less than (in floatoutput mode, less than or equal to) the absolute value of this limit. If a negative value is specified as a step size, steps of size zero are never generated.

 If this argument is not provided, the step size is set to 2 (movement up or down by no more than 1); in floatoutput mode, the default step size is 0.1

## Messages

- `bang` — Output a random value
  Causes drunk to take a step of random size up or down from its currently stored value. It updates the stored value and outputs it.
- `int(input: int)` — Function depends on inlet
  In the left inlet: Replace the current value, send new value to the outlet.
  In the middle inlet: Set the maximum value that can be output by the drunk object. If the specified maximum is less than 0 it is set to 0.
  In the right inlet: Set the step size taken in response to a bang in the left inlet. The step (up or down) will always be less than (in floatoutput mode, less than or equal to) the absolute value of this number.
- `float(input: float)` — See the int message.
  In floatoutput mode, the functions are the same, but in the floating-point domain. When floatoutput is disabled, arriving values are converted to int during processing.
- `list(ARG_NAME_0: list)` — TEXT_HERE
- `reset([cycle-index: int])` — Reset cycle counter
  With no arguments, the reset message resets the cycle counter to zero. (The cycle counter is used when the cycle attribute is non-zero.) When an optional int argument is present, reset assigns that value to the cycle counter and "fast-forwards" from the beginning of the cycle so that the next output value will be at the specified cycle position. reset is useful immediately after changing the seed or cycle to ensure the cycle starts or continues a consistent pattern.
- `set(input: int/float)` — Set the stored value, no output
  The word set, followed by a number, sets the stored value to that number without triggering output. The stored value is initially set in the center of the total range (half of the maximum value).
- `setresetvalue([stored-value: int])` — Assign reset output value
  With no arguments, the setresetvalue message assigns the most recent output value to be the value assigned when a cycle reset occurs. With an int argument sets the reset value to a specific number.

## Attributes

- `@default` (atom_long)
- `@label` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `range` — seen as: `range -0.75 0.25`, `range 0. 1.`
- `seed` — seen as: `seed 5150`

## Help patcher examples

### float_range

```
Example #1 — [drunk 5 1 @floatoutput 1]
  fan-in:
    in0 ← [button] ← [metro 100] ← [toggle]    # bang to output a random step
  fan-out:
    out0 → [multislider]:in0
```

```
Example #2 — [drunk 0 0.1 @range -0.5 0.5]
  fan-in:
    in0 ← [message "range 0. 1."]
    in0 ← [button] ← [metro 100] ← [toggle]
    in0 ← [message "range -0.75 0.25"]    # bang to output a random step
    in2 ← [message "0.1"]
    in2 ← [message "0.4"]
  fan-out:
    out0 → [multislider]:in0
```

### cycle

```
Example — [drunk 20 5 @cycle 20 @seed 1]
  fan-in:
    in0 ← [attrui @cycle]    # adjust pattern length
    in0 ← [attrui @seed]    # change the pattern by changing the seed
    in0 ← [message "setresetvalue"]    # change the reset value
    in0 ← [message "reset"]    # reset now
    in0 ← [button] ← [metro 150] ← [toggle]    # start the sequence
  fan-out:
    out0 → [multislider]:in0    # observe repeating patterns
    out0 → [number]:in0
```

Attributes demonstrated: `@cycle`, `@seed`

### basic

```
Example #1 — [drunk 500 -25]  negative step size supresses duplicates
  fan-in:
    in0 ← [button] ← [metro 100] ← [toggle]
    in0 ← [message "set 100"]    # set current value with no output
    in0 ← [message "seed 5150"]    # seed the random number system
    in1 ← [number]    # set output range
    in2 ← [number]    # set step size
  fan-out:
    out0 → [number]:in0
```

```
Example #2 — [drunk 128 5]
  fan-in:
    in0 ← [button] ← [metro 100] ← [toggle]    # bang to output a random step
  fan-out:
    out0 → [multislider]:in0
    out0 → [number]:in0
```

## See also

`decide`, `random`, `urn`
