# active

_max · System_

> Send 1 when patcher window is active, 0 when inactive

active will output a 1 when the Patcher window becomes active (i.e., it is the front-most window and its title bar is highlighted), and output a 0 when the Patcher window becomes inactive. You can use this to change the user interface of your window. For example, messages from the key object can be turned off when the window is made inactive.

## Messages

- `int(internal-messaging: int)` — There are no inlets. Output is triggered automatically when the patcher window is activated or deactivated.

## Help patcher examples

### basic

```
Example — [active]
  fan-out:
    out0 → [gate]:in0
    out0 → [select 1 0]:in0
```

## See also

`closebang`, `loadbang`, `loadmess`, `savebang`
