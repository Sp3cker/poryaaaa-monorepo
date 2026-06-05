# float

_max · Data_

> Store a decimal number

float can store and output any given floating-point number.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | bang Outputs Value, int Sets and Outputs Value |
| in1 | Set Value Without Output |
| out0 | Value |

## Arguments

- **initial-value** (`float`) _(optional)_ — TEXT_HERE
  Sets an initial value to be stored in float. If there is no argument, the initial value is 0.0. A float argument by itself, without the word float, is another way of creating and initializing a float object.

## Messages

- `bang` — In left inlet: Sends the stored value out the outlet
- `int(input: int)` — Converted to float.
- `float(input: float)` — In left inlet: The number replaces the currently stored value and is sent out the outlet
- `in1(set-input: float)` — In right inlet: The number replaces the stored value without triggering output
- `send(receive-object-name: list)` — In left inlet: The word send, followed by a name of a receive object, sends the number stored in the float object to all receive objects with that name, without sending it out the float object's outlet.
- `set(set-input: float)` — In left inlet: The word set, followed by a number, replaces the stored value without triggering output.

## Help patcher examples

### basic

```
Example #1 — [float 7.4]
  fan-in:
    in0 ← [message "send goom"]    # send value to named receive object
  fan-out:
    out0 → [button]:in0
```

```
Example #2 — [float 5.8]
  fan-in:
    in0 ← [flonum] ← [* 0.5] ← [slider]    # float in left inlet sets and outputs value
    in0 ← [button]    # bang outputs the current value without changing it.
    in1 ← [flonum] ← [* 0.5] ← [slider]    # float in right inlet sets value with no output
  fan-out:
    out0 → [flonum]:in0
    out0 → [button]:in0
```

## See also

`int`, `pv`, `value`
