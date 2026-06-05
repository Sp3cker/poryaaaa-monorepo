# counter

_max · Control_

> Keep count based on bang messages

Outputs the current count of bang message constrained to a specified range. Can be set to count up, down, or up-then-down.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | int, bang are Counted, set Sets Counter Value |
| in1 | Set Direction: 0 = Up, 1 = Down, 2 = UpDown |
| in2 | Resets Counter to Number on Next Clock |
| in3 | Resets Counter to Number Immediately |
| in4 | Sets Count Maximum |
| out0 | Current Count |
| out1 | Underflow (Counter Hit Minimum) Flag |
| out2 | Carry (Counter Hit Maximum) Flag |
| out3 | Carry Count |

## Arguments

- **options** (`int`) _(optional)_ — Function depends on number of arguments
  If there is only one argument, it sets an initial maximum count value for counter. If there are two arguments, the first number sets an initial minimum value, and the second number sets an initial maximum value. If there are three arguments, the first number specifies the direction of the count, the second number is the minimum, and the third number is the maximum. If there are no arguments, the direction is up, the minimum is 0, and the maximum is 2,147,483,647 (the largest possible 32-bit signed integer).

## Messages

- `bang` — Function depends on inlet
  In left inlet: Sends out the current count of the bang messages received in the left inlet.
  In left-middle inlet: Changes the direction of the count.
  In middle inlet: Resets the count to its specified minimum value, which will be sent out the next time a bang is received in the left inlet.
  In right-middle inlet: Resets the count to its specified minimum value, and sends out that value immediately.
  In right inlet: Resets the count to its specified maximum value, which is sent out immediately.
- `int(input: int)` — Function depends on inlet
  In left inlet: Same effect as bang.
  In left-middle inlet: Sets the direction of the count. 0 causes counter to count up, 1 causes it to count down, and 2 causes it to count up and down.
  In middle inlet: The number sets the counter to a new value, to be sent out the next time a bang is received in the left inlet. If the number is less than the current minimum value, the minimum will be reset to that number. If the number is greater than the current maximum value, the counter will be set to that number, but the maximum value actually remains the same and the minimum is set equal to the maximum.
  In middle-right inlet: The number sets the counter to a new value and sends it out immediately. If the number is less than the current minimum value, the minimum will be reset to that number. If the number is greater than the current maximum value, the number is sent out, but the maximum value actually remains the same and the minimum is set equal to the maximum.
  In right inlet: Resets the maximum value sent out by counter. If the number is less than the current minimum, the maximum is equal to the minimum. If the minimum is subsequently changed to a value below the maximum value you input, the counter objects retains the correct maximum value it received through this inlet. Unlike a bang message, an int in this inlet does not cause the counter object to output anything.
- `float(input: float)` — Function depends on inlet
  In all other inlets: Converted to int.
- `carrybang` — Change carry output to a bang message
  In left inlet: Causes counter to send a bang out the right-middle outlet when the count is going upward and reaches its maximum limit, and causes counter to send a bang out the left-middle outlet when the count is going downward and reaches its minimum limit. (By default, counter sends out the number 1 in those situations, instead of bang.) The state of the carrybang message is saved along with the patcher it is used in.
- `carryint` — Change carry output to an int message
  In left inlet: Undoes the effect of a previously received carrybang message. Resets the counter to send the numbers 1 and 0 out the left-middle and right-middle outlets (instead of bang) to signal when the counter reaches and leaves its minimum and maximum values. The state of the carryint message is saved along with the patcher it is used in.
- `dec` — Decrement the counter value
  In left inlet: Decrements the counter (downward) and sends out the new value, regardless of the direction in which the object has been set to count ordinarily.
- `down` — Set direction to down (decreasing)
  In left inlet: Sets the counter to count in a downward direction.
- `flags(carry-mode: int, minimum-mode: int)` — Set operational modes
  The flags message followed by two numbers will set the Underflow/Carry Mode and set the Minimum-Mode resetting capability. If the first number is 0, counter will output a 1 when it hits the maximum or else output a 0. If the first number is 1, counter will output a bang when it hits the maximum. If the second number is 0, an integer in inlets 3 and 4 will override the minimum count temporarily. If the second number is 1, an integer in inlets 3 and 4 will change the minimum count permanently.
- `goto(input: int)` — Set the counter value with no output
  In left inlet: Same effect as set.
- `inc` — Increment the counter value
  In left inlet: Increments the counter (upward) and sends out the new value, regardless of the direction in which the object has been set to count ordinarily.
- `jam(input: int)` — Set the counter value and cause output
  In left inlet: The word jam, followed by a number, sets the counter to that number and sends the number out immediately. If the number is outside the minimum and maximum count range, this message is ignored.
- `max(maximum: int)` — Set the maximum value
  In left inlet: The word max followed by a number, resets the maximum value of counter to that number. If the number is less than the current minimum value, the maximum is considered to be equal to the minimum, although the actual maximum value you set is stored inside the counter object.
- `min(minimum: int)` — Set the minimum value and cause output
  In left inlet: The word min followed by a number, resets the minimum value of counter to that number, and causes the counter object to set itself to that number and output immediately. If the number is greater than the current maximum value, the minimum is set equal to the maximum.
- `next` — Output next count value
  In left inlet: Same as bang.
- `set(input: int)` — Set the counter value with no output
  In left inlet: The word set, followed by a number, sets the counter to that number, which will be sent out the next time a bang is received in the left inlet.
- `setmin(minimum: int)` — Set the minimum value with no output
  In left inlet: The word setmin, followed by a number, sets the counter object's minimum count without affecting its current count value or causing any output.
- `state` — Print current state to the Max Console
  The message state will cause the counter object to report its current state to the Max Console.
- `up` — Set direction to up (increasing)
  In left inlet: Sets the counter to count in an upward direction.
- `updown` — Set direction to up/down (alternating)
  In left inlet: Sets the counter object's direction so that it counts upward until it reaches the specified maximum, then counts down until it reaches the specified minimum, then up, then down, and so on.

## Attributes

- `@carryflag` (int) — Carry output flag
  Sets the type of data output from the third outlet (numbers or bangs).
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### arguments

```
Example #1 — [counter]  with no arguments, counter counts upwards from zero
  fan-in:
    in0 ← [button]
  fan-out:
    out0 → [number]:in0
```

```
Example #2 — [counter 2 1 4]  mode 2: up and down
  fan-in:
    in0 ← [button]
  fan-out:
    out0 → [number]:in0
```

```
Example #3 — [counter 1 -1 -4]  mode 1: loop downwards
  fan-in:
    in0 ← [button]
  fan-out:
    out0 → [number]:in0
```

```
Example #4 — [counter 0 1 4]  mode 0: loop upwards
  fan-in:
    in0 ← [button]
  fan-out:
    out0 → [number]:in0
```

```
Example #5 — [counter -3 4]  with two arguments, counter loops upwards between them
  fan-in:
    in0 ← [button]
  fan-out:
    out0 → [number]:in0    # With three arguments, counter treats the first argument as a mode indicator, and the next 2 arguments as the loop endpoints
```

```
Example #6 — [counter -5]  with one argument, counter loops upwards between 0 and the argument
  fan-in:
    in0 ← [button]
  fan-out:
    out0 → [number]:in0
```

### details

```
Example #1 — [counter 0 0 9]  set count value for next tick
  fan-in:
    in0 ← [button]
    in1 ← [button]
    in1 ← [number] ← [umenu]    # int sets direction
    in2 ← [button]    # min on next tick
    in2 ← [number]    # this int on next tick / bang switches direction
    in3 ← [number]    # set to min now / set this int now
    in3 ← [button]
    in4 ← [number]    # set max count / set to max now
    in4 ← [button]
  fan-out:
    out0 → [number]:in0
```

```
Example #2 — [counter 0 0 9]
  fan-in:
    in0 ← [button]
    in0 ← [message "setmin $1"]    # set min count, no output
    in0 ← [message "min $1"]
    in0 ← [message "max $1"]
    in0 ← [message "set $1"]
    in0 ← [message "jam $1"]
  fan-out:
    out0 → [number]:in0
```

### basic

```
Example #1 — [counter 0 1 4]
  fan-in:
    in0 ← [button]
    in0 ← [message "dec"]
    in0 ← [message "inc"]
  fan-out:
    out0 → [number]:in0
    out1 → [toggle]:in0    # min reached
    out2 → [toggle]:in0    # max reached
    out3 → [number]:in0    # carry count
```

```
Example #2 — [counter 2 1 4]  palindrome
  fan-in:
    in0 ← [button]
  fan-out:
    out0 → [number]:in0
```

```
Example #3 — [counter 1 -1 -4]  loop backwards
  fan-in:
    in0 ← [button]
  fan-out:
    out0 → [number]:in0
```

```
Example #4 — [counter 1 4]  loop forward
  fan-in:
    in0 ← [button]
  fan-out:
    out0 → [number]:in0
```

```
Example #5 — [counter]  no arguments counts upwards from zero
  fan-in:
    in0 ← [button]
  fan-out:
    out0 → [number]:in0    # bang count
```

## See also

`tempo`
