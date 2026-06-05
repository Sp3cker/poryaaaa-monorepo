# error

_max · System_

> Report Max errors

Listens for and reports Max errors as message output. This will allow for error management in cases where it is not appropriate to display the Max window.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | 1 Turns Error Listening On, 0 Turns It Off |
| out0 | Error Message Output |

## Arguments

- **on** (`int`) _(optional)_ — Turn on error reporting
  Turn on error reporting. If no argument is given, then error reporting will be off until an int message is received.

## Messages

- `int(enable: int)` — Set error management flag
  The error object allows you to catch errors and output them as Max messages. A non-zero number starts the error object "listening" for Max errors. The error object must be listening to produce any output. A 0 turns off listening.
- `float(enable: float)` — Set error management flag
  Converted to int.
- `error(message: list)` — Output an error message
  The word error followed by any message will send that message out the outlet.

## Help patcher examples

### basic

```
Example — [error]
  fan-in:
    in0 ← [toggle]    # turn on error reporting
  fan-out:
    out0 → [route error]:in0    # use route to strip off 'error' from output
```

## See also

`print`
