# function

_max · U/I_

> Breakpoint function editor

Draw or store a set of x, y points as floating-point numbers. The output the entire function is useful as an input for line~. You can also get an interpolated y value for any x value.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | bang Outputs All, float Outputs Y at X, list Edits |
| out0 | Interpolated Y (float) for Input X Interpolated Y (float) for Input X |
| out1 | All Points in line Format |
| out2 | dump Message Output (list) |
| out3 | bang When Changed With Mouse |

## Messages

- `bang` — Output a list of breakpoints
  Triggers a list output of the current breakpoints from the middle-left outlet formatted for use by the line~ object. As an example, if the function contained breakpoints at X = 1, Y = 0; X = 10, Y = 1; and X = 20, Y = 0, the output would be 0, 1 9 0 10. If the optional output mode is enabled, the output would be 0 0 1 9 0 10.
  If there are any sustain points in the function, bang outputs a list of all the points up to the sustain point. Additional points in the function, up to a subsequent sustain point or the end point, whichever applies, can be output by sending the next message. See the description of the next and sustain messages for additional information.
- `int(x-value: int)` — Output an interpolated y-axis value
  The value is taken as an X value and outputs a corresponding Y value out the left outlet. The Y value is produced by linear floating-point interpolation of the function. If the X value lies outside the first or last breakpoint, the Y value is 0.
- `float(input: float)` — Output an interpolated y-axis value
  The value is taken as an X value and outputs a corresponding Y value out the left outlet. The Y value is produced by linear floating-point interpolation of the function. If the X value lies outside the first or last breakpoint, the Y value is 0.
- `list(x-value: number, y-value: number, [curve: number])` — Add or modify a breakpoint
  If the list contains two values, a new point is added to the function. The first value is X, the second is Y.
  If the list contains three values, an existing point in the function is modified. The first value is the index (starting at 0) of a breakpoint to modify, the second is the new X value for the breakpoint, and the third is the new Y value for the breakpoint. (If the index number in the list refers to a breakpoint that does not exist, the message is ignored.)
  If the list contains four values and the mode attribute is set to 1 (curve), the third value is the curve amount, with values greater than 0 resulting an exponential curve, and values less than 0 resulting in a logarithmic curve.
- `clear(indices: list)` — Clear breakpoints
  The word clear by itself erases all existing breakpoints. The word clear can also be followed by one or more breakpoint indices (starting at 0) to clear selected breakpoints.
- `clearfix` — Clear all fixed-value states
  The word clearfix clears all fix states (sets them to 0).
- `clearsustain` — Clear all sustain states
  The word clearsustain clears all sustain states (sets them to 0).
- `color(color index: int)` — Set color with index (deprecated)
  The color message sets a color with an index from 0 to 15 for breakpoints and lines against a light background. It is no longer supported.
- `copy` — Copy the points from a function
  The copy message copies all of the current function points to the clipboard so that they can be pasted into another function object.
- `dump(receive-name: symbol)` — Output all breakpoints
  Outputs a series of multiple element lists describing each break point out the function object's third outlet. Each list contains the breakpoints X and Y values, followed by the curve value, if present. An optional symbol argument can be used to specify a receive objects as a destination.
- `fix(index: number, flag: int)` — Toggle breakpoint editing
  The word fix, followed by a number specifying the index of a point and 0 or 1, prevents the user from changing the point if the second number is 1, and allows the user to change the point if the second number is 0. By default, points are moveable unless clickmove 0 has been sent to disable moving of all points.
- `getfix(point-indices: list)` — Report all fixed-value points
  The word getfix. with no arguments, will cause the function object to send a list all fix points out the object's third outlet. If an index is provided as an argument, the fix state for that point will be output.
- `getsustain(point-indices: list)` — Report all sustain points
  The word getsustain. with no arguments, will cause the function object to send a list all sustain points out the object's third outlet. If an index is provided as an argument, the sustain state for that point will be output.
- `lineout(ARG_NAME_0: int)` — Output a list of breakpoints
  The word lineout followed by either 0, 1, or no argument is equivalent to the bang message.
- `listdump(receive-name: symbol)` — Output all breakpoints as a single list
  Outputs a single list which contains all X and Y values for each of the breakpoints out the function object's third outlet. An optional symbol argument can be used to specify a receive object as a destination.
- `next` — Continue from a sustain point
  The next message sends a list out the second outlet that continues from the sustain point where the output of the last bang or next message ended. For instance, if the function contained breakpoints at (a) X = 1, Y = 0; (b) X = 10, Y = 1; and (c) X = 20, Y = 0, and point b was a sustain point, a bang message would output 0, 1 9 and a subsequent next message would output 1, 0 10. After a next message reaches the end point, a subsequent next message is equivalent to a bang message. next is also equivalent to a bang when no bang has been sent that reached a sustain point, or when a function contains no sustain points.
- `nth(index: int)` — Output a breakpoint value
  The word nth, followed by a number, uses the number as the index (starting at 0) of a breakpoint, and outputs the Y value of the breakpoint out the left outlet. If no breakpoint with the specified index exists, no output occurs.
- `paste` — Paste the points of a copied function
  The paste message pastes all of the points of a previously copied function into a function object.
- `quantize_x` — Quantize points on the grid's horizontal axis
  This message will cause all of the points to automatically snap to the horizontal grid as defined by the gridstep_x attribute.
- `quantize_y` — Quantize points on the grid's vertical axis
  This message will cause all of the points to automatically snap to the vertical grid as defined by the gridstep_y attribute.
- `set(x-y-coordinate-pairs: list)` — Set breakpoint values
  Given the number of points already defined within function's graphic editor, a corresponding list of x-y-coordinate pairs will set the position of each point.
- `setcurve(index: int, curve-factor: float)` — Draw a curve between two points
  The word setcurve, followed by an integer that specifies the index of a function point (numbered from 1) and a floating point value that specifies a curve, will create a curved line segment between the specified point and the next point.
  Curve factor values from 0 to 1.0 produce an "exponential" curve when increasing in value and values from -1.0 to 0 produce a "logarithmic" curve. The closer to 0 the curve parameter is, the closer the curve is to a straight line, and the farther away the parameter is from 0, the steeper the curve. The mode attribute must be set to 1 (curve mode) for this message to be effective.
- `setdomain(maximum: float)` — Set the maximum X-axis value
  The word setdomain, followed by a float or int value, sets the maximum displayed X value, then modifies the X values of all breakpoints so that they remain in the same place given the new domain.
- `setrange(minimum: number, maximum: number)` — Set minimum and maximum Y-axis values
  The word setrange, followed by two float or int values, sets the minimum and maximum display range for Y values, then modifies the Y values of all breakpoints so that they remain in the same place given the new range.
- `sustain(index: int, flag: int)` — Set the sustain state for a breakpoint
  The word sustain, followed by number specifying the index of a point and zero or one, turns that point into a sustain point if the second number is 1, or into a regular point if the second number is 0. By default, points are regular (non-sustain). The behavior of sustain points is discussed in the description of the bang message above. Command-clicking on Macintosh or Control-clicking on Windows also toggle the sustain property of a point.
- `xyc(x-value: number, y-value: number, curve-factor: float)` — Add a point with curve information
  The word xyc, followed by an two numbers that specifies X and Y values and a floating point number that specifies a curve factor, will add a new point with curve information to the function.
  Curve factor values from 0 to 1.0 produce an "exponential" curve when increasing in value and values from -1.0 to 0 produce a "logarithmic" curve. The closer to 0 the curve parameter is, the closer the curve is to a straight line, and the farther away the parameter is from 0, the steeper the curve. The mode attribute must be set to 1 (curve mode) for this message to be effective.

## GUI behaviors

- `(mouse)` — Manually add or edit breakpoints
  You can use the mouse to add or edit breakpoints.
  Clicking on empty space in the function adds a breakpoint. You can begin to move it immediately by dragging. Adding breakpoints can be disabled with the clickadd attribute.
  Dragging on an existing breakpoint moves the breakpoint. Modifying breakpoints can be disabled with the clickmove attribute.
  Shift-clicking on a breakpoint deletes it.
  Command- (Mac OS) or control-clicking (Windows) on a breakpoint toggles the sustain property of the point. Sustain point click behavior can be set with the clicksustain attribute.
  If the mode attribute has been set to Curve, option- (alt- on Windows) dragging on a line segment modifies the curvature of that segment.

## Attributes

- `@introduced` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `clickadd` — seen as: `clickadd $1`
- `clickmove` — seen as: `clickmove $1`
- `clicksustain` — seen as: `clicksustain $1`
- `domain` — seen as: `domain 100`, `domain 1000`
- `getdomain` — seen as: `getdomain, getrange`
- `outputmode` — seen as: `outputmode $1`
- `range` — seen as: `range -1 1`, `range 0 1`, `range 0 2`

## Help patcher examples

### appearance

```
Example — [function]
  fan-in:
    in0 ← [attrui @gridstep_x]
    in0 ← [attrui @gridstep_y]
    in0 ← [attrui @bgcolor]
    in0 ← [attrui @gridcolor]
    in0 ← [attrui @linecolor]
    in0 ← [attrui @style]    # Create and apply styles using the format palette or type a style name here
    in0 ← [attrui @textcolor]
```

Attributes demonstrated: `@bgcolor`, `@gridcolor`, `@gridstep_x`, `@gridstep_y`, `@linecolor`, `@style`, `@textcolor`

### mc

Attributes demonstrated: `@candycane`, `@chans`

### curve

> When function's mode attribute is set to curve, function outputs all points in curve syntax, suitable for the curve~ object.

```
Example — [function]  each segment: (level time curve factor) / Use the option key to edit curves graphically
  fan-in:
    in0 ← [message "setcurve 1 $1"]
    in0 ← [button]
  fan-out:
    out1 → [curve~]:in0
```

### getting-attribute-values

> You can query a function object's attributes using "get" messages - just add the name of the attribute (with no space between "get" and the attribute name).

```
Example — [function]  < This function object's Scipting name is set to myfunction using the Inspector
  fan-in:
    in0 ← [message "getdomain, getrange"]
    in0 ← [getattr domain]
  fan-out:
    out2 → [route domain range]:in0
```

### zoom

```
Example #1 — [function]
  fan-in:
    in0 ← [button]    # click to produce an outstanding sound!
    in0 ← [prepend zoom_x] ← [rslider] ← [loadmess 0 1]
    in0 ← [prepend zoom_y] ← [rslider] ← [loadmess 0 1]
  fan-out:
    out1 → [line~]:in0
```

```
Example #2 — [function]
  fan-in:
    in0 ← [prepend zoom_y] ← [rslider] ← [loadmess 0 1]
    in0 ← [prepend zoom_x] ← [rslider] ← [loadmess 0 1]
    in0 ← [button]    # click to produce an outstanding sound!
  fan-out:
    out1 → [route list]:in0
```

### sustain

> function allows any points other than the first and last to be sustain points when using the line~-appropriate output. A bang received sends only the points up to the first sustain point. Subsequent "next" messages move from there to the next sustain point, or to the end if there are no more sustain points.

```
Example — [function]  Sustain points are outlined in white. It's easier to see them when you change the color of the dots by using the Inspector. / Command-double-click on any point to turn sustain on or off, or send the sustain message (shown above).
  fan-in:
    in0 ← [message "sustain 3 $1"]    # turn point 3's sustain property on/off
    in0 ← [button]    # • start at the beginning
    in0 ← [message "next"]    # • go to next sustain point or end
  fan-out:
    out1 → [line~]:in0
```

### shadow

```
Example #1 — [function]  signed range (-1 - 1)
  fan-in:
    in0 ← [attrui @shadowproportion]
    in0 ← [attrui @shadowalpha]
    in0 ← [attrui @shadowsigned]
    in0 ← [attrui @shadowreflectionpoint]
    in0 ← [attrui @shadowblend]
```

```
Example #2 — [function]  unsigned range (0 - 1)
  fan-in:
    in0 ← [attrui @shadowproportion]
    in0 ← [attrui @shadowalpha]
    in0 ← [attrui @shadowblend]
```

Attributes demonstrated: `@shadowalpha`, `@shadowblend`, `@shadowproportion`, `@shadowreflectionpoint`, `@shadowsigned`

### basic

> Erase all
>
> • Click and release the mouse on a point to see where it is. Drag on the point to move it.
>
> • Click in empty space to add a new point (unless "clickadd 0" has been sent).
>
> • Shift-click on a point to remove it (unless "clickadd 0" has been sent).
>
> • Cmd-click on a point to toggle sustain
>
> bang when changed by mouse action

```
Example — [function]
  fan-in:
    in0 ← [message "outputmode $1"]
    in0 ← [message "fix 0 0"]    # prevent/allow moving a single point with the mouse (the first one in this case)
    in0 ← [message "fix 0 1"]
    in0 ← [message "setrange 0 1"]
    in0 ← [message "range 0.5 1.5"]
    in0 ← [message "clear"]
    in0 ← [message "setrange 0.5 1.5"]    # change display and function so it retains shape
    in0 ← [message "setdomain 100"]
    in0 ← [message "setdomain 1000"]
    in0 ← [message "dump"]
    in0 ← [message "clickmove $1"]    # allow/disallow moving all existing points by dragging
    in0 ← [message "clickadd $1"]
    in0 ← [message "range 0 1"]    # changed displayed y min and max
    in0 ← [message "range 0 2"]
    in0 ← [message "range -1 1"]
    in0 ← [message "domain 100"]    # change displayed x maximum
    in0 ← [message "domain 1000"]
    in0 ← [flonum]    # produce interpolated y for this x at left outlet
    in0 ← [message "0 25.5 0.7"]    # change x, y value of the first point (function resorts the list, so this point may not be at the same index afterward)
    in0 ← [message "50 0.5"]    # add a new point
    in0 ← [button]    # Output all breakpoints in line format (initial value followed by a list of deltatime-value pairs)
    in0 ← [message "listdump"]    # send all points out dump outlet as one list, optional argument names receive object
    in0 ← [message "clicksustain $1"]    # allow/disallow changing sustain state with the mouse / send all points out dump outlet, optional argument names receive object
  fan-out:
    out0 → [flonum]:in0    # interpolated y value
    out1 → [print line]:in0    # line format output
    out2 → [print dump]:in0    # dump output
    out3 → [button]:in0
```

## See also

`line`
