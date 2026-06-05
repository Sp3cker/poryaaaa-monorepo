# osc.codebox

_Max · Devices_

> Display OSC packets as Dictionaries

The osc.codebox object is a UI object for the display of OSC packets as dictionaries.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | OSC packet to display in dictionary format |

## Messages

- `FullPacket(ARG_NAME_0: int, ARG_NAME_1: int)` — TEXT_HERE
- `clear` — TEXT_HERE
- `osc_packet(ARG_NAME_0: symbol)` — TEXT_HERE

## Attributes

- `@attr_attr_save` (int)
- `@category` (symbol)
- `@dynamiccolor_default` (symbol)
- `@label` (symbol)
- `@legacydefault` (float, size 4)
- `@paint` (int)
- `@preview` (symbol)
- `@save` (int)
- `@set` (pointer)
- `@style` (symbol)

## Help patcher examples

### basic

```
Example — [osc.codebox]
  fan-in:
    in0 ← [param.osc @auto 1]
    in0 ← [message "clear"]
```

Attributes demonstrated: `@auto`, `@outputmode`

## See also

`param.osc`, `dict.codebox`, `udpsend`, `udpreceive`
