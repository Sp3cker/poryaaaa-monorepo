# delay

_max · Timing_

> Delay a bang

Holds a bang for a specified amount of time before sending it to the next object. This object uses the Max time format syntax, so the delay time (which is normally specified in milliseconds) can also be set to other fixed or tempo-relative values.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | bang Gets Delayed, stop Cancels |
| in1 | Set Delay Time in Milliseconds |
| out0 | Delayed bang |

## Arguments

- **time** (`any`) — Initial delay time
  Sets an initial amount of time to delay a bang received in the left inlet. This time interval can be either a number which specifies time in milliseconds (e.g. delay 200) or a notevalue (e.g. delay 4nd).

 Note: While the delay object lets you specify time in any of Max's standard time formats, the delaytime attribute argument should be used when specifying time in any other time unit besides milliseconds or notevalues (e.g. delay @delaytime 11025 samples).

 If there is no argument, the initial time interval is 5 milliseconds. If notevalue, ticks, or bars.beats.units are specified for the delay interval, the object will not operate unless the transport is running.

## Messages

- `bang` — Delay, then output
  In left inlet: A bang is delayed a certain number of milliseconds before being sent out the outlet.
- `int(time: int)` — Set delay time
  In right inlet: The number is stored as the number of milliseconds to delay a bang received in the left inlet. A number received in the right inlet changes the delay time of the next bang received -- it does not modify the time of a bang currently being delayed. In left inlet: The number is stored as the number of milliseconds to delay a bang received in the left inlet. It then automatically sends a bang message to itself to start the delay.
- `float(time: float)` — Set delay time
  In right inlet: The number is stored as the number of milliseconds to delay a bang received in the left inlet. A number received in the right inlet changes the delay time of the next bang received -- it does not modify the time of a bang currently being delayed. In left inlet: The number is stored as the number of milliseconds to delay a bang received in the left inlet. It then automatically sends a bang message to itself to start the delay.
- `list(input: list)` — Set time with ITM value
  In right inlet: A list may be used to specify time in one of the Max time formats.
- `anything(time: list)` — Set time with ITM value
  Same as list.
- `clock(setclock-object-name: symbol)` — Choose a clock source
  The word clock, followed by the name of an existing setclock object, sets the delay object to be controlled by that setclock object rather than by Max’s internal millisecond clock. The word clock by itself sets the delay object back to using Max’s regular millisecond clock.
- `stop` — Stop any pending output
  In left inlet: Stops delay from outputting the bang it is currently delaying.

## Attributes

- `@category` (symbol)
- `@default` (atom, size 2)
- `@label` (symbol)
- `@style` (symbol)
- `@units` (atom, size 7)

## Help patcher examples

### more

> global or other named transport must be running when a tempo-relative Max time format is used.

```
Example — [delay 1n]
  fan-in:
    in0 ← [attrui @delaytime]
    in0 ← [attrui @quantize]
    in0 ← [button]    # bang to start delay
    in0 ← [attrui @transport]
  fan-out:
    out0 → [button]:in0
```

Attributes demonstrated: `@delaytime`, `@quantize`, `@tempo`, `@transport`

### basic

```
Example — [delay 500]
  fan-in:
    in0 ← [button]    # bang to start delay
    in0 ← [message "stop"]    # stop delay
    in1 ← [number] ← [dial]    # set delay time in milliseconds
  fan-out:
    out0 → [button]:in0
```

## See also

`deferlow`, `pipe`, `setclock`, `transport`
