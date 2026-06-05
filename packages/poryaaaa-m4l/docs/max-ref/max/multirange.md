# multirange

_MSP · MC_

> Graphical function breakpoint editor

multirange is designed to work with mc.evolve~ and mc.gradient to set their breakpoints

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | bang Outputs All, float Outputs Y at X, list Edits |
| out0 | Interpolated Y1, Y2, Phase for Input X |
| out1 | All Points in line Format |
| out2 | dump Message Output (list) |
| out3 | bang When Changed With Mouse |

## Messages

- `bang` — Reports current state out of 2nd inlet from the left
  Outputs a series of multi-element lists out the middle-left outlet. The first three lists are the inital breakpoint's phase, y2 and y1 values. The next three contain the phase, y2 and y1 values for all subsequent breakpoints.
- `int(lookup: int)` — Function lookup
  Sending a value within the multirange object's
  domain
  returns a list with the y1, y2 and phase at that x position.
- `float(lookup: float)` — Function lookup
  Sending a value within the multirange object's
  domain
  returns a list with the y1, y2 and phase at that x position.
- `list(x-value: number, y2-value: number, y1-value: number, [phase: number])` — Add a new breakpoint
  Creates a new breakpoint at the specified x-position. If the list contains only three values, the phase is set to the average of the y1 and y2 values. An optional 4th value can be used to speficy the phase value.
  A list with 5 values modifies the breakpoint at the index specified by the first list element (beginning with 0).
- `clear` — Clear all breakpoints
  The word clear by itself erases all existing breakpoints. The word clear can also be followed by one or more breakpoint indices (starting at 0) to clear selected breakpoints.
- `dump(receive-name: symbol)` — Output all breakpoints
  Outputs a series of multiple element lists describing each break point out the multirange object's middle-right outlet. Each list contains the breakpoints X, Y1, Y2 and phase values. An optional symbol argument can be used to specify a receive objects as a destination.
- `listdump(receive-name: symbol)` — Output all breakpoints as a single list
  Outputs a single list which contains all X, Y and phase values for each of the breakpoints out the multirange object's middle-right outlet. An optional symbol argument can be used to specify a receive objects as a destination.

## GUI behaviors

- `(mouse)` — Manually add or edit breakpoints
  You can use the mouse to add breakpoints to the multirange function; the finished function can then be sent to a mc.evolve~ or mc.gradient~ object for use as a control signal in MSP. The X, Y1, Y2 and phase values of the breakpoint are displayed in the upper part of the object’s box.
  Clicking on empty space in the function adds a breakpoint at the current X position, which you can begin to move immediately by dragging.
  Clicking on the bar connecting the Y1 and Y2 nodes and dragging allows you to change the X position of the breakpoint.
  Clicking on either y point of a breakpoint allows you to adjust the value by dragging.
  Shift-clicking on a breakpoint deletes that point from the function. Command-clicking on Macintosh or Control-clicking on Windows on a breakpoint toggles the sustain property of the point. Sustain points are outlined in white. Whenever an editing operation with the mouse is completed, a bang is sent out the right outlet.
  Points with a Y value of 0 are outlined circles; other points are solid. This allows you to see at a glance whether a function starts or ends at Y = 0.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)
- `@save` (int)

## Help patcher examples

### mc.evolve~

```
Example — [multirange]
  fan-in:
    in0 ← [message "dump"]
    in0 ← [message "clear"]
    in0 ← [flonum]
  fan-out:
    out2 → [mc.evolve~ 12]:in0
    out3 → [button]:in0
```

### basic

> Phase is ignored by objects such as mc.evolve~ but could be used to determine where to start a sequence of steps within a range (see mc.gradient~)

```
Example — [multirange]  click to add ranges / drag up - down on a point to change range / drag left - right on a bar to change X / use shift key to delete ranges / use option key to edit phase
  fan-in:
    in0 ← [message "dump"]    # dump state in format compatible with mc.evolve~ and mc.gradient~
    in0 ← [button]    # report state to labeled outlet
    in0 ← [message "clear"]    # clear the function
    in0 ← [flonum]    # function lookup
  fan-out:
    out0 → [message "0.373512 0.473214 0.379922"]:in1    # Y1 , Y2 and phase for float (X) input
    out1 → [print multirange-labeled]:in0
    out1 → [button]:in0
    out2 → [print multirange-dump]:in0
    out3 → [button]:in0
```

## See also

`mc.evolve~`, `mc.function`, `mc.gradient~`, `mc.range~`
