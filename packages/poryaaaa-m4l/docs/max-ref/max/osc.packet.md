# osc.packet

__

> Store and recall an OSC packet

The osc.packet object stores and recalls OSC packets, and can convert them into different formats.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | OSC packet to store and output immediately |
| in1 | OSC packet to store |
| out0 | stored OSC packet |

## Messages

- `bang` — TEXT_HERE
- `FullPacket(ARG_NAME_0: int, ARG_NAME_1: int)` — TEXT_HERE
- `clear` — TEXT_HERE
- `osc_packet(ARG_NAME_0: symbol)` — TEXT_HERE

## Attributes

- `@outputformat` (symbol) — Output Format
  Set the output format an OSC packet will be converted to.
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### basic

```
Example — [osc.packet]
  fan-in:
    in0 ← [button]
    in1 ← [param.osc @auto 1]
  fan-out:
    out0 → [osc.codebox]:in0
```

## See also

`param.osc`, `osc.codebox`, `udpsend`, `udpreceive`
