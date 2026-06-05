# metro

_max · Timing_

> Output a bang message at regular intervals

Acts as a metronome which outputs bang s at a regular, specified interval. This object uses the Max time format syntax, so the interval that the metro object uses can be either fixed or tempo-relative.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Start/Stop Metronome |
| in1 | Set Metronome Time Interval |
| out0 | Output Ticks of Metronome |

### Port details

**`in1` (Set Metronome Time Interval):** Sets the delay time for the object. Delay time can be specified in any of the time formats used in Max.

## Arguments

- **interval** (`number`) _(optional)_ — Output interval
  The optional first argument sets an initial value for the time interval at which metro sends its output. This time interval can be either a number which specifies time in milliseconds (e.g. metro 100) or notevalue (e.g. metro 4n).

 Note: While the metro object lets you specify time in any of Max's standard time formats, the interval attribute argument should be used when specifying time in any other time unit besides milliseconds or notevalues (e.g. metro @interval 11025 samples).

 If there is no argument, the initial time interval is 5 milliseconds.

## Messages

- `bang` — Start the clock
  In left inlet: starts the metro object.
- `int(input: int)` — Turn metro on/off or set the time interval
  In left inlet: Any number other than 0 starts the metro object. At regular intervals, metro sends a bang out the outlet. 0 stops metro.
  In right inlet: The number is the time interval, in milliseconds, at which metro sends out a bang. A new number in the right inlet does not take effect until the next output is sent.
- `float(input: float)` — Set the time interval
  Performs the same function as int.
- `list(input: list)` — ITM-time-list
  In right inlet: A list may be used to specify time in one of the Max time formats.
- `anything(interval: list)` — ITM-time-list
  Same as list.
- `clock(name: symbol)` — Select a clock source
  The word clock, followed by the name of an existing setclock object, sets the metro object to be controlled by that setclock object rather than by Max’s internal millisecond clock. The word clock by itself sets the metro object back to using Max’s regular millisecond clock.
- `stop` — Stop the clock
  In left inlet: Stops metro.

## Attributes

- `@inlet` (int)

## Help patcher examples

### basic

> @defer 1 forces metro to use a low-priority queue. The 'qmetro' object is really just a metro object with the @defer attribute set to 1.

```
Example #1 — [metro 4n @active 1]
  fan-out:
    out0 → [button]:in0    # This metro will start as soon as you start the transport above. It counts in quarter notes.
```

```
Example #2 — [metro 500]
  fan-in:
    in0 ← [toggle]    # start/stop
    in1 ← [number]    # set time in milliseconds
  fan-out:
    out0 → [button]:in0
```

```
Example #3 — [metro @defer 1]  =
  (no patch cords)
```

## See also

`clocker`, `counter`, `cpuclock`, `delay`, `setclock`, `tempo`, `transport`, `uzi`
