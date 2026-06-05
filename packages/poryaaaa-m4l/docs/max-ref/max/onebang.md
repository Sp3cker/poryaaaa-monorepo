# onebang

_max · Control_

> Gate bangs using a bang

Allows a bang in the left inlet to pass through ONLY if a bang has been received in the right inlet. After that, a bang in the left inlet will not get through again until a bang has been received again in the right inlet.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | bang/anything | (bang/anything) Only first bang gets through |
| in1 | bang/anything | (bang/anything) Set to let one bang through |
| out0 | bang | (bang) First bang |
| out1 | bang | (bang) All other bangs |

## Arguments

- **initialization** (`int`) _(optional)_ — First-time pass-through
  A non-zero argument sets onebang to permit a bang to be sent out the left outlet the first time a bang is received in the left inlet.

## Messages

- `bang` — Function depends on inlet
  In left inlet: Causes a bang to be sent out the left inlet only if a bang has been received in the right inlet since the last bang was sent out.
  In right inlet: Resets onebang to permit a bang to be sent out the next time a bang is received in the left inlet.
- `int(input: int)` — Function depends on inlet
  In either inlet: Same as a bang.
- `float(input: float)` — Function depends on inlet
  In either inlet: Same as a bang.
- `list(input: list)` — Function depends on inlet
  In either inlet: Same as a bang.
- `anything(input: list)` — Function depends on inlet
  In either inlet: Converted to bang.
- `stop` — Ignore previous right inlet input
  In left inlet: Undoes the effect of a bang in the right inlet.

## Help patcher examples

### basic

> Example: Detect the first time that middle C is played, but ignore subsequent occurrences until reset occurs.

```
Example #1 — [onebang 1]
  fan-in:
    in0 ← [select 60] ← [stripnote]
    in1 ← [button]    # reset
  fan-out:
    out0 → [button]:in0
```

```
Example #2 — [onebang]
  fan-in:
    in0 ← [button]    # a bang in this inlet...
    in1 ← [button]    # ...will only get through after a bang in this inlet.
  fan-out:
    out0 → [button]:in0    # only one left bang gets through for each right bang.
    out1 → [button]:in0    # if no right bang was received the incoming bang is routed to the right outlet
```

## See also

`gate`, `gswitch2`
