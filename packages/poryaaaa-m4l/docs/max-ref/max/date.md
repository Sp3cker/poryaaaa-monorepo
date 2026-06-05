# date

_max · System_

> Report current date and time

Reports the current date, time, or the number of 1/60th-second ticks since startup.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | date, time, or ticks Message |
| out0 | Date (Month/Day/Year) |
| out1 | Time (Hour/Minute/Second) |
| out2 | Ticks (1/60th Second) |

## Messages

- `date` — Output the current date
  Outputs the current date as a list (month/day/year) out the left outlet.
- `ticks` — Output the tick count
  Outputs the current value of Ticks (the number of 1/60ths of a second since system startup) out the right outlet.
- `time` — Output the current time
  Outputs the current time as a list (military hours/minutes/seconds) out the middle outlet.

## Help patcher examples

### basic

```
Example — [date]  click on 'date' and 'ticks' to output
  fan-in:
    in0 ← [message "date"]
    in0 ← [message "time"]
    in0 ← [message "ticks"]
  fan-out:
    out0 → [unpack 0 0 0]:in0
    out1 → [unpack 0 0 0]:in0
    out2 → [number]:in0    # Ticks
```

## See also

`clocker`, `timer`
