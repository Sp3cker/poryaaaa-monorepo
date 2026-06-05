# bondo

_max · Right-to-Left_

> Synchronize a group of messages

Synchronizes and outputs a set of inputs when any input is received. It can also be set with a time interval value (in milliseconds) to wait before sending its output.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Input 1 to be Synchronized |
| in1 | Input 2 to be Synchronized |
| out0 | Output of Inlet 1 |
| out1 | Output of Inlet 2 |

## Arguments

- **inlets-outlets** (`int`) _(optional)_ — The number of inlets and outlets
  Specifies the number of inlets and outlets. The default number of inlets and outlets is 2.
- **delay** (`int`) _(optional)_ — The delay time for messages
  Specifies the number of milliseconds to delay when a message is received before sending messages out the outlets.
- **list-flag** (`symbol`) _(optional)_ — Allow lists for input
  Using the symbol "n" as an argument, bondo is able to synchronize lists which arrive in different inlets.

## Messages

- `bang` — Send all stored messages
- `int(input: int)` — Store value and cause output
  In any inlet: The input is stored in the location corresponding to that inlet, and causes anything previously stored to be sent out its corresponding outlet. If no message has yet been received in a particular inlet, 0 is sent out of the corresponding outlet.
- `float(input: float)` — Store value and cause output
  In any inlet: The input is stored in the location corresponding to that inlet, and causes anything previously stored to be sent out its corresponding outlet. If no message has yet been received in a particular inlet, 0 is sent out of the corresponding outlet.
- `list(input: list)` — Store values and cause output
  In any inlet: The elements of the list are parsed among the inlets. The first element in the list is sent out the outlet which corresponds to the inlet which received the list and each subsequent element in the list is sent out each subsequent outlet.
  If the "n" argument was used, bondo will store and output lists for each outlet in addition to single numbers.
- `anything(input: list)` — Store values and cause output
  In any inlet: The input is stored in the location corresponding to that inlet, and causes anything previously stored to be sent out its corresponding outlet. If no message has yet been received in a particular inlet, 0 is sent out of the corresponding outlet.
- `set(input: list)` — Store value without output
  In any inlet: The word set, followed by any message, stores the input in the location corresponding to that inlet without triggering any output.

## Help patcher examples

### basic

```
Example — [bondo 4 20]
  fan-in:
    in0 ← [message "set 5 6 7 8"]    # set message without outputting
    in0 ← [number]
    in0 ← [button]    # bang to produce all the current values
    in1 ← [number]
    in2 ← [number]
    in3 ← [number]
  fan-out:
    out0 → [number]:in0
    out0 → [button]:in0
    out1 → [number]:in0
    out1 → [button]:in0
    out2 → [number]:in0
    out2 → [button]:in0
    out3 → [number]:in0
    out3 → [button]:in0
```

## See also

`buddy`, `join`, `onebang`, `pack`, `thresh`
