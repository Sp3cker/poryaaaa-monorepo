# fswap

_max · Right-to-Left_

_(reference XML aliased from `swap`.)_

> Swap position of two numbers

Swaps the values of its inlets, preserving right-to-left ordering. The first outlet type is determined by its argument. The second outlet's type is always a float.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Sets Value for Right Outlet, Causes Output |
| in1 | Sets Value for Left Outlet |
| out0 | Value From Right Inlet |
| out1 | Value From Left Inlet |

## Arguments

- **initial** (`int or float`) _(optional)_ — Set initial value and type
  Sets an initial value for the number which is to be sent out the left outlet. If there is no argument, the initial value is 0. If there is an int argument or no argument, an int is sent out the left outlet. (The number sent out the right outlet is always a float.)

## Messages

- `bang` — Send out current values
  In left inlet: Swaps and sends out the numbers currently stored in fswap.
- `int(input: int)` — Store and swap data
  If there is a float argument, the numbers are converted to float. If there is an int argument or no argument, the number received in the right inlet is stored as an int. See the float entry for more details.
- `float(input: float)` — Store and swap data
  In left inlet: The number is sent out the right outlet, then the number in the right inlet is sent out the left outlet.
- `ft1(input: float)` — Store data to swap
  In right inlet: The number is stored to be sent out the left outlet when a number is received in the left inlet.
- `in1(input: int)` — Store data to swap
  In right inlet: The number is stored to be sent out the left outlet when a number is received in the left inlet.

## Help patcher examples

### swap and fswap

```
Example #1 — [fswap 5]
  fan-in:
    in0 ← [flonum]
    in1 ← [flonum]
  fan-out:
    out0 → [flonum]:in0
    out1 → [flonum]:in0
```

```
Example #2 — [fswap 5.]
  fan-in:
    in0 ← [flonum]
    in1 ← [flonum]
  fan-out:
    out0 → [flonum]:in0
    out1 → [flonum]:in0
```

## See also

`join`, `pack`, `swap`, `unjoin`, `unpack`
