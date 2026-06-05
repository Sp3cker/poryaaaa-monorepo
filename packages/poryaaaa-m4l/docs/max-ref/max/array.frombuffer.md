# array.frombuffer

_max · Array_

> Read audio buffer values into an array object

Determine the properties of a buffer that has been read into an array object.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array, messages in |
| in1 | array, messages in |
| out0 |  |
| out1 | channel count out |

## Arguments

- **buffername** (`symbol`) _(optional)_ — Buffer Name
  The name of the buffer to reference

## Messages

- `bang` — Trigger Output
  Reprocess previously received buffer values and trigger output.
- `list(ARG_NAME_0: list)` — Receive a list of messages

## Attributes

- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `flatsinglechannel` — seen as: `flatsinglechannel $1, channelstart 1, bang`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in1` — array, messages in

### basic

```
Example — [array.frombuffer arrayfrombuf @framelength 50]  array out / channel count
  fan-in:
    in0 ← [button]    # bang to trigger array out
    in0 ← [message "flatsinglechannel $1, channelstart 1, bang"]
  fan-out:
    out0 → [print frombuffer]:in0
    out1 → [number]:in0
```

## See also

`array`, `array.tobuffer`, `peek~`, `poke~`
