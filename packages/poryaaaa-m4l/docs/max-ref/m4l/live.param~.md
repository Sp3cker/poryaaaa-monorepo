# live.param~

_m4l · Live MSP Objects_

> Generate an MSP signal from a parameter value.

live.param~ produces a signal with the current value of a parameter.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| out0 | signal | Parameter Value |

## Arguments

- **parameter-name** (`parameter-name`) — TEXT_HERE
  The live.param~ object takes as an argument the name of the parameter that you wish to monitor.

## Messages

- `signal` — The current value of the parameter is output as a value at signal rate

## Help patcher examples

### basic

```
Example — [live.param~ Toto]  bind to the Toto parameter, and output the value as a continuous signal
  fan-out:
    out0 → [scope~]:in0
    out0 → [number~]:in0
```

## See also

`live.observer`, `live.remote~`
