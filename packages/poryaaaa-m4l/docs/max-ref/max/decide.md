# decide

_max · Math_

> Choose randomly between 1 and 0

Outputs random 1 and 0 messages. The output sequence depends on the seed value to determine the sequence of values.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Make A Decision |
| in1 | Set Seed (0 = Use System Time) |
| out0 | Randomly Generated 0 or 1 |

## Arguments

- **seed** (`int`) _(optional)_ — Random seed value
  Sets a "seed" value to cause a specific (reproducible) sequence of pseudo-random 0 and 1 outputs to occur. If there is no argument, the time elapsed since system startup (an unpredictable value) is used as the seed, ensuring an unpredictable sequence for output.

## Messages

- `bang` — Output 1 or 0
  Causes a randomly chosen output of 1 or 0.
- `int(input: int)` — Output 1 or 0
  In left inlet: Same as bang.
- `in1(seed: int)` — Change the seed value
  In right inlet: A given "seed" number causes a specific (reproducible) sequence of pseudo-random 0 and 1 outputs to occur. The number 0 uses the time elapsed since system startup (an unpredictable value) as the seed, ensuring an unpredictable sequence of 0 and 1 outputs.

## Help patcher examples

### basic

```
Example — [decide]
  fan-in:
    in0 ← [metro 250] ← [toggle]    # Make some decisions.
    in1 ← [number]    # Set seed value.
  fan-out:
    out0 → [toggle]:in0
```

## See also

`drunk`, `random`, `toggle`, `urn`
