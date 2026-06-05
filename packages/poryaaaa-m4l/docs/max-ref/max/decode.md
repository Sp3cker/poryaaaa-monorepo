# decode

_max · Data_

> Send 1 or 0 out a specific outlet

Provides hierarchical switching. The right inlet turns all outlets off, switch, while the middle inlet turns all outlets on. The right inlet overrides the middle inlet, and the middle inlet overrides numnbers sent to the left inlet that turn individual outlets on or off.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Number to decode (lowest priority) |
| in1 | Enable (medium priority) |
| in2 | Disable All (highest priority) |
| out0 | Decode 0 (high if input = 0) |

## Arguments

- **outlets** (`int`) _(optional)_ — The number of outlets
  Sets the number of outlets. The default is one outlet.
- **outlets** (`float`) _(optional)_ — The number of outlets
  Converted to int.

## Messages

- `bang` — Output current state
  The message bang causes decode to output its current state.
- `int(outlet: int)` — Select an outlet to turn on
  In left inlet: An index (starting with 0 for the left outlet) that specifies an outlet out to turn on, turning off all other outlets.
- `in1(secondary: int)` — Turn on all outlets
  In middle inlet: If a 1 was last received in the right inlet, any number received in the middle outlet will send a 0 out all outlets. Otherwise, a number greater than 0 received in the middle inlet sends a 1 out all outlets. If 0 is received in the middle inlet, decode sends a 1 out the last outlet decoded by a number received in the left inlet, and 0 out all other outlets.
- `in2(primary: int)` — Turn off all outlets
  In right inlet: Any positive number other than 0 sends a 0 out all outlets. When decode receives a 0 in its right inlet, it outputs 0 or 1 out its outlets based on the values last received in the middle and left inlets.

## Help patcher examples

### basic

```
Example — [decode 4]
  fan-in:
    in0 ← [message "1"]    # An int in the left inlet gets decoded to one of outlets.
    in0 ← [message "2"]
    in0 ← [message "3"]
    in0 ← [message "4"]
    in0 ← [message "0"]
    in0 ← [message "-1"]
    in1 ← [toggle]    # A 1 at the middle inlet sets all outputs high regardless of the left inlet, unless the right outlet has received a 1.
    in2 ← [toggle]    # A 1 at the right inlet sets all outputs low regardless of the left or middle inlet.
  fan-out:
    out0 → [toggle]:in0
    out1 → [toggle]:in0
    out2 → [toggle]:in0
    out3 → [toggle]:in0
```

## See also

`bucket`, `gate`, `toggle`
