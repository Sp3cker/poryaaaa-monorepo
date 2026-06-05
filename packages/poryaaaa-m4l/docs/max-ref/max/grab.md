# grab

_max · Control_

> Intercept the output of another object

grab can send a message and extract the result from the receiving object.

 Note: The grab object cannot be used to communicate from a send to a receive between Max for Live devices.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Message to Send to Object |
| out0 | Result of Remote Message |
| out1 | Connect to Object That Will Receive Message |

## Arguments

- **number-of-outlets** (`int`) _(optional)_ — TEXT_HERE
  The first argument sets the number of outlets, in addition to the right outlet. If there is no argument, grab has 1 additional outlet.
- **receive-name** (`symbol`) _(optional)_ — TEXT_HERE
  If a symbol is present as a second argument, the message received in the inlet is sent to all receive objects named by the symbol, instead of being sent out the right outlet. In this case the rightmost outlet, which would normally send out the incoming message if no second argument were present, will not exist.

## Messages

- `bang` — Performs the same function as anything.
- `int(input: int)` — Performs the same function as anything.
- `float(input: float)` — Performs the same function as anything.
- `list(input: list)` — Performs the same function as anything.
- `anything(input: list)` — The message is sent out the right outlet, or if a second argument is present the message is sent to receive objects named by the second argument.
- `set(input: symbol)` — If a second argument has been typed into grab specifying the name of a receive object, then the word set, followed by a symbol, specifies the name of a (different)
  If a second argument has been typed into grab specifying the name of a receive object, then the word set, followed by a symbol, specifies the name of a (different) receive object via which grab can grab messages from remote objects.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `length` — seen as: `length`

## Help patcher examples

### example

> grab is used here to get the length of the table to set the limits of the counter

```
Example — [grab]
  fan-in:
    in0 ← [message "length"]
  fan-out:
    out0 → [- 1]:in0
    out1 → [table]:in0
```

### basic

> When grab has a second symbol argument, the message is sent to the receive object with the specified name, and the output of objects immediately connected to the receive object is sent out grab's outlet. The second outlet of grab disappears in this mode.

```
Example #1 — [grab 1 blue]
  fan-in:
    in0 ← [flonum]
  fan-out:
    out0 → [flonum]:in0
```

```
Example #2 — [grab]
  fan-in:
    in0 ← [button]    # click to output result
  fan-out:
    out0 → [number]:in0
    out1 → [int 55]:in0
```

```
Example #3 — [grab 2]
  fan-in:
    in0 ← [message "20 30"]    # click here and watch results in the Max window
  fan-out:
    out0 → [print a @popup 1]:in0
    out1 → [print b @popup 1]:in0
    out2 → [unpack]:in0
```

## See also

`preset`, `table`
