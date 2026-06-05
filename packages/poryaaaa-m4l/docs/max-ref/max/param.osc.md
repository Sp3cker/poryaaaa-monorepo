# param.osc

__

> Control and report info about parameters using OSC.

Controls and reports info about parameters using OpenSoundControl (OSC).

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | OSC in |
| out0 |  |

## Messages

- `bang` — Generate OSC output
  Then bang message will cause param.osc to output OSC according to the outputmode.
- `anything(ARG_NAME_0: list)` — OSC-like Max messages
  param.osc accepts normal Max messages that represent OSC, for example: /param/live.gain~ -18.
- `FullPacket(ARG_NAME_0: int, ARG_NAME_1: int)` — OSC packet
  An OSC packet containing messages to be dispatched to parameters in the patch.
- `allparams` — Output the values of all parameters in a patch as an OSC bundle
  Generate an OSC bundle containing the values of every parameter in a patch.
- `info(ARG_NAME_0: list)` — Extended info about all parameters in a patch as an OSC bundle
  Generate an OSC bundle containing comprehensive info about every parameter in the patch, for example the long name, short name, minimum, maximum, etc.
- `osc_packet(ARG_NAME_0: symbol)` — TEXT_HERE
- `param(ARG_NAME_0: list)` — Output the value(s) of the most recently changed parameter as an OSC bundle
  Generate an OSC bundle containing the value of the most recently changed parameter.
- `set(ARG_NAME_0: list)` — Dispatch OSC to parameters without triggering output.
  Dispatches OSC to parameters without triggering output from param.osc or UDP if enabled.

## Attributes

- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `/param.osc.maxhelp/param/live.gain~%5B1%5D` — seen as: `/param.osc.maxhelp/param/live.gain~%5B1%5D -6`
- `/param.osc.maxhelp/param/live.gain~` — seen as: `/param.osc.maxhelp/param/live.gain~ -6`
- `/param.osc.maxhelp/param/live.menu` — seen as: `/param.osc.maxhelp/param/live.menu 1`

## Help patcher examples

### basic

```
Example — [param.osc @auto 1]
  fan-in:
    in0 ← [message "allparams"]
    in0 ← [message "info"]
    in0 ← [message "/param.osc.maxhelp/param/live.menu 1"]
    in0 ← [message "/param.osc.maxhelp/param/live.gain~%5B1%5D -6"]
    in0 ← [button]
    in0 ← [message "/param.osc.maxhelp/param/live.gain~ -6"]
    in0 ← [attrui @auto]
    in0 ← [attrui @outputmode]
  fan-out:
    out0 → [osc.codebox]:in0
```

Attributes demonstrated: `@auto`, `@outputmode`

## See also

`osc.codebox`
