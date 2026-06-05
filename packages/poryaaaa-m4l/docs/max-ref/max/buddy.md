# buddy

_max · Right-to-Left_

> Synchronize arriving data

Outputs incoming data after something has been received in all inlets.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Input to be Synchronized |
| in1 | Input to be Synchronized |
| out0 | Synchronized Output of Inlet 1 |
| out1 | Synchronized Output of Inlet 2 |

## Arguments

- **inlets** (`int`) _(optional)_ — The number of inlets (and outlets).
  Sets the number of inlets and outlets. If there is no argument, there are two inlets and two outlets.

## Messages

- `bang` — Simulate input of 0
  In any inlet: Same as sending the number 0.
- `int(input: int)` — Input data into one outlet
  In any inlet: When data has been received in all its inlets, buddy sends the received messages out their corresponding outlets, then waits until data has arrived again in all inlets.
- `float(input: float)` — Input data into one outlet
  In any inlet: When data has been received in all its inlets, buddy sends the received messages out their corresponding outlets, then waits until data has arrived again in all inlets.
- `list(input: list)` — Input data into one outlet
  In any inlet: When data has been received in all its inlets, buddy sends the received messages out their corresponding outlets, then waits until data has arrived again in all inlets.
- `anything(input: list)` — Input data into one outlet
  In any inlet: When data has been received in all its inlets, buddy sends the received messages out their corresponding outlets, then waits until data has arrived again in all inlets.
- `clear` — Clear all stored values
  In left inlet: Deletes all values stored in the inlets.

## Help patcher examples

### basic

> When messages go through a send, their order to receive objects is unknown. With buddy, you can ensure that the two values come out in a specified order.

```
Example #1 — [buddy]
  fan-in:
    in0 ← [message "22"]
    in1 ← [message "35.6"]
  fan-out:
    out0 → [print second]:in0
    out1 → [print first]:in0
```

```
Example #2 — [buddy 4]  buddy only sends its input when all inputs have been received. note that the button doesn't fire until you click on all the numbers. Then all inputs are cleared.
  fan-in:
    in0 ← [message "1"]
    in1 ← [message "2"]
    in2 ← [message "3"]
    in3 ← [message "4"]
  fan-out:
    out0 → [button]:in0
```

```
Example #3 — [buddy]  the list starts with the controller value, then the number
  fan-in:
    in0 ← [ctlin]
    in1 ← [ctlin]
  fan-out:
    out0 → [pack]:in0
    out1 → [pack]:in1
```

## See also

`bondo`, `onebang`, `join`, `pack`, `swap`, `thresh`, `unjoin`, `unpack`
