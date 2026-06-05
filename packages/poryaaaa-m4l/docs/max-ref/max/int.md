# int

_max · Data_

> Store an integer value

int can store and output any given integer number.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | bang Outputs Value, int Sets and Outputs Value |
| in1 | Set Value Without Output |
| out0 | Value |

## Arguments

- **initial-value** (`number`) _(optional)_ — Sets an initial value to be stored
  Sets an initial value to be stored in int . If there is no argument, the initial value is 0. An int argument by itself, without the word int, is another way of creating and initializing an int object. Float values are converted to integers.

## Messages

- `bang` — In left inlet: Sends the stored value out the outlet.
- `int(input: int)` — In left inlet: The number replaces the currently stored value and is sent out the outlet.
- `float(input: float)` — Converted to int.
- `in1(set-input: int)` — In right inlet: The number replaces the stored value without triggering output.
- `send(receive-object-name: list)` — In left inlet: The word send, followed by the name of a receive object, sends the value stored in int to all receive objects with that name, without sending it out the outlet of the int.
- `set(set-input: int)` — In left inlet: The word set, followed by a number, replaces the stored value without triggering output.

## Help patcher examples

### basic

```
Example #1 — [int 74]
  fan-in:
    in0 ← [message "send goom"]    # send sends value to named receive object:
  fan-out:
    out0 → [button]:in0
```

```
Example #2 — [int 74]
  fan-in:
    in0 ← [number] ← [slider]
    in0 ← [button]    # bang outputs the current value without changing it.
    in1 ← [number] ← [slider]    # int in right inlet sets value with no output
  fan-out:
    out0 → [number]:in0
    out0 → [button]:in0
```

## See also

`float`, `pv`, `value`
