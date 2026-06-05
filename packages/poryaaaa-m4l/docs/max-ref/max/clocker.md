# clocker

_max · Timing_

> Report elapsed time, at regular intervals

The clocker object is a metronome that reports the time elapsed since it was started. This object uses the Max time format syntax, so the interval that the clocker object uses can be either fixed or tempo-relative. Its output can be quantized using tempo-relative syntax, and if the autostarttime attribute is set, the object can also start at a tempo-relative point.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Start/Stop Metronome |
| in1 | Set Metronome Time Interval |
| out0 | Output Ticks of Metronome |

## Arguments

- **time-interval** (`int, float, symbol`) — The first argument sets an initial value for the time interval at which clocker sends out its output. This time interval can be either a number which specifies time in milliseconds (e.g. clocker 200) or a notevalue (e.g. clocker 4nd).

 Note: While the clocker object lets you specify time in any of Max's standard time formats, the Interval attribute argument should be used when specifying time in any other time unit besides milliseconds or notevalues.

 If there is no argument, the initial time interval is set to 5 milliseconds. If notevalue, ticks, or bars.beats.units are specified for the delay interval, the clocker object will not operate unless the transport is running.
  The first argument sets an initial value for the time interval at which clocker sends out its output. This time interval can be either a number which specifies time in milliseconds (e.g. clocker 200) or a notevalue (e.g. clocker 4nd).

 Note: While the clocker object lets you specify time in any of Max's standard time formats, the Interval attribute argument should be used when specifying time in any other time unit besides milliseconds or notevalues.

 If there is no argument, the initial time interval is set to 5 milliseconds. If notevalue, ticks, or bars.beats.units are specified for the delay interval, the clocker object will not operate unless the transport is running.

## Messages

- `bang` — In left inlet: Starts the clocker object.
  In left inlet: Starts the clocker object. If the clocker object is not running, a bang message will start the count. If the clocker object is running, a bang message will reset the count.
- `int(non-zero-to-start: int)` — Start/stop.
  In left inlet: Any non-zero number starts the clocker object. The time elapsed since clocker was started is sent out the outlet at regular intervals. 0 stops the clocker object.
- `float(non-zero-to-start: float)` — Start/stop.
  Same as int.
- `list(input: list)` — ITM-time-list
  In right inlet: A list may be used to specify time in one of the Max time formats.
- `anything(interval: list)` — Same as list.
- `clock(setclock object name: symbol)` — The word clock, followed by the name of an existing setclock object, sets the clocker object to be controlled by that setclock object rather than by Max’s internal millisecond clock.
  The word clock, followed by the name of an existing setclock object, sets the clocker object to be controlled by that setclock object rather than by Max’s internal millisecond clock. The word clock by itself sets the clocker object back to using Max’s regular millisecond clock.
- `reset` — In left inlet: Resets the elapsed time to 0 without stopping or restarting the clock; clocker continues to report the new elapsed time at the same regular interval.
  In left inlet: Resets the elapsed time to 0 without stopping or restarting the clock; clocker continues to report the new elapsed time at the same regular interval. This message is meaningless when the clocker is not running, since it always resets to 0 anyway when stopped.
- `stop` — In left inlet: Stops the clocker object.

## Attributes

- `@inlet` (int)

## Help patcher examples

### basic

```
Example #1 — [clocker 4n]
  fan-in:
    in0 ← [message "reset"]
    in0 ← [toggle]
    in1 ← [message "4n"]
    in1 ← [message "1.0.0"]
  fan-out:
    out0 → [number]:in0
```

```
Example #2 — [clocker 103.785004]
  fan-in:
    in0 ← [toggle]
  fan-out:
    out0 → [flonum]:in0    # float value in box causes input to be added as floats.
```

```
Example #3 — [clocker 1000]
  fan-in:
    in0 ← [toggle]    # non/zero starts, zero stops
    in0 ← [message "stop"]
    in0 ← [button]    # click to start timer
    in0 ← [message "reset"]    # reset counter only
    in1 ← [number]    # set interval
  fan-out:
    out0 → [number]:in0    # output (ms)
```

## See also

`counter`, `cpuclock`, `delay`, `setclock`, `tempo`, `transport`, `uzi`
