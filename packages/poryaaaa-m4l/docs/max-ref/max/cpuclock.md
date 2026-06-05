# cpuclock

_max · Timing_

> Retrieve the CPU time

Accesses a precise value from the system timer. This allows for timing calculations with very high resolution.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Bang to get the current system time. |
| out0 | Current system time. |

## Messages

- `bang` — Output a time value
  bang causes the current time to be output. The time value is calculated from when Max is launched (starting from 0.0).
- `reset` — Reset timer value
  Resets the system timer value to 0.0.

## Help patcher examples

### uzi test

```
Example — [cpuclock]
  fan-in:
    in0 ← [uzi] ← [message "10000"]    # How fast is your uzi?
  fan-out:
    out0 → [t f f]:in0
```

### scheduling test

```
Example — [cpuclock]
  fan-in:
    in0 ← [metro 75]
  fan-out:
    out0 → [t f f]:in0
```

### midi_latency

```
Example #1 — [cpuclock]
  fan-in:
    in0 ← [select 50] ← [notein]
  fan-out:
    out0 → [- 0.]:in0
```

```
Example #2 — [cpuclock]
  fan-in:
    in0 ← [t i b] ← [message "50"]    # Send a noteout and start the timer.
  fan-out:
    out0 → [- 0.]:in1
```

### basic

```
Example — [cpuclock]
  fan-in:
    in0 ← [metro 1000] ← [toggle]    # Start the metro.
  fan-out:
    out0 → [flonum]:in0    # System time as a float
```

## See also

`metro`, `translate`, `timepoint`, `transport`, `when`
