# live.grid

_m4l · Live UI Objects_

> A UI grid of steps, with constraints and directions

live.grid is a user interfaces object designed for use with the chucker~ object. It provides a display grid of steps, constraints for transposition, and playback direction for use in controlling the chucker~ object.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | — | Message in |
| in1 | — | Sync Messages |
| out0 | list | Current Step Value (y value, direction) |
| out1 | list | Steps Values |
| out2 | list | Directions Values |
| out3 | list | Mouse Over Information |
| out4 | list | dumpout |
| out5 | anything | Synchronisation Messages (if Linked) |

## Messages

- `int(step-index: int)` — Set, display, and output the current step
  Sets, displays and outputs the current step. The list sent out the left outlet of the live.grid object takes the form step(s) direction (optional, depending on the visibility of the directions panel). When the live.grid object is in Matrix Mode and there is no value selected in the column, a 0 is sent as the step value.
- `list(input-list: list)` — Function depends on the direction or matrixmode attributes
  When the direction attribute is set to 0, a list of 2 values sets the step index and value.
  When the direction attribute is set to 1, a list of 3 values sets the step index, value and direction.
  When the matrixmode attribute is set to 1, a list of 3 values sets the object's behavior: x, y, and state (0/1). An optional fourth value can be used to set direction (-1/0/1).
- `clear` — Deselect all currently set rows and columns
  The clear message is only used when the matrixmode attribute is enabled, and will deselect all currently set rows and columns.
- `constraint(constraint of the step: list)` — Set the constraint for a step
  Sets the constraint for a specific step. For instance, the message constraint 3 0 0 1 1 sets the constraints of the third step (column numbering starts at 1) to 0 0 1 1.
- `directions(directions: list)` — Set a step's playback direction
  Sets the grid directions, used to indicate the direction of playback of a step:
  -1: Backward
  0: Stop
  1: Forward
- `down` — Decrease all step values
  Decreases (lowers) the current values of all the steps by one. New step values are sent out the second outlet.
  Note: The current constraints, if any, are also taken into account when altering current step values; the resulting shift might be "rounded" to the closest step.
- `freeze(step-index: int)` — Display/hide a frozen step
  Set the index to 0 to hide the overlay used to indicate frozen values. Any index greater than zero sets and displays the frozen step.
- `getcell(x-index: int, y-index: int)` — Report the value of a specified cell
  The word cell, followed by a pair of numbers that specifies the x and y indices of a cell in the live.grid display, will send a list consisting of the word cell, a number pair that specifies the x and y indices, and the value of the cell (column numbering starts at 1).
- `getcolumn(column: int)` — Report the values for a specified column
  The word getcolumn, followed by a number that specifies a column in the live.grid display, will send a list consisting of the word column, a number that specifies the column index, and a list of the current column values out the fourth (dumpout) outlet (column numbering starts at 1).
- `getconstraint(column: int)` — Report the constraint values for a specified column
  The word getconstraint, followed by a number that specifies a column in the live.grid display, will send a list consisting of the column number followed by a list indicating the constrain state of each cell in the column, from bottom to top (column numbering starts at 1). Constraints are indicated as follows:
  0: Constraint
  1: No constraint
- `getdirections(column: int)` — Report the direction for a specified column
  The word getdirections, followed by a number that specifies a column in the live.grid display, will send a list consisting of the column number followed by the direction out the fourth (dumpout) outlet (column numbering starts at 1). The direction is indicated as follows:
  -1: Backward
  0: Stop
  1: Forward
- `getrow(row: int)` — Report the values for a specified row
  The word getrow, followed by a number that specifies a row in the live.grid display, will send a list consisting of the word row, a number that specifies the row index, and a list of the current row values out the fourth (dumpout) outlet (row numbering starts at 1).
- `init` — Restore and output the initial values
  Restores and outputs the initial values.
- `left` — Left-shift values of all steps
  Rotate the values of all steps to the left. New steps values are sent out the second outlet.
  Note: The current constraints, if any, are also taken into account when altering current step values; the resulting shift might be "rounded" to the closest step.
- `linkdump` — Synchronize a live.grid object to another
  When the link attribute is set to 1, the linkdump message sends all messages required to synchronize one live.grid object to another live.grid object out the object's right outlet.
- `random(type (optional): list)` — Generate new step values or step constraints
  The word random generates new step values or step constraints, depending on the current mode. An optional second argument can be used to select a portion of the UI object to randomize:
  steps: Randomize the steps values
  constraint: Randomize constraints and make sure that the steps values are correct with respect to the new constraints grid
  directions: Randomize directions if the direction attribute is set to 1.
- `reset(type (optional): list)` — Set the default values or clear constraints
  The word reset sets the default values or clears the constraints, depending on the current mode. An optional second argument can be used to select a portion of the UI object to reset:
  steps: Reset the steps values (Note: since the reset may interact with the current constraints, a given step value may not be completely reset)
  constraint: Clear all constraints
  directions: Set all the directions to forward if the direction attribute is set to 1.
- `right` — Right-shift all step values
  Rotate the values of all the steps to the right. New steps values are sent out the second outlet.
  Note: The current constraints, if any, are also taken into account when altering current step values; the resulting shift might be "rounded" to the closest step.
- `set(step-index: int)` — Set and display an output step without causing output
  Sets and displays the current output step without causing output.
- `setcell(x-index: int, y-index: int, value: int)` — Set the value of the specified cell
  The word setcell, followed by a pair of numbers that specifies the x and y indices of a cell in the live.grid display and a number value, will set the value of the specified cell (column numbering starts at 1).
- `steps(step values: list)` — Set the values of all the steps at once
  Sets the values of all the steps at once. Value numbering starts at 1.
- `up` — Increase all step values
  Increases (raises) the values of all steps. New steps values are sent out the second outlet.
  Note: The current constraints, if any, are also taken into account when altering current step values; the resulting shift might be "rounded" to the closest step.

## Attributes

- `@attr_attr_save` (int)
- `@category` (atom)
- `@defaultname` (float, size 4)
- `@dynamiccolor_default` (symbol)
- `@label` (symbol)
- `@paint` (int)
- `@save` (int)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `link` — seen as: `link $1`

## Help patcher examples

### appearance

```
Example — [live.grid] (live.text[5])
  fan-in:
    in0 ← [attrui @directioncolor]
    in0 ← [attrui @freezecolor]
    in0 ← [attrui @hbgcolor]
    in0 ← [attrui @bgstepcolor2]
    in0 ← [attrui @bordercolor2]
    in0 ← [attrui @stepcolor]
    in0 ← [attrui @amountcolor]
    in0 ← [attrui @bgstepcolor]
    in0 ← [attrui @bordercolor]
```

Attributes demonstrated: `@amountcolor`, `@bgstepcolor`, `@bgstepcolor2`, `@bordercolor`, `@bordercolor2`, `@directioncolor`, `@freezecolor`, `@hbgcolor`, `@stepcolor`

### data

```
Example — [live.grid]
  fan-in:
    in0 ← [message "getcolumn $1"]
    in0 ← [message "getrow $1"]
    in0 ← [message "getdirections $1"]
    in0 ← [message "setcell 5 7 1"]
    in0 ← [message "getconstraint $1"]
    in0 ← [message "setcell 7 4 1"]
    in0 ← [message "setcell 7 6 1"]
    in0 ← [message "getcell 3 4"]
    in0 ← [message "getcell 7 4"]
  fan-out:
    out4 → [route cell column row constraint directions]:in0
```

### matrix

```
Example #1 — [live.grid]
  fan-in:
    in0 ← [attrui @marker_horizontal]
    in0 ← [attrui @marker_vertical]
  fan-out:
    out1 → [p live.grid2matrix @rows 4]:in0
```

```
Example #2 — [live.grid]
  fan-in:
    in0 ← [message "clear"]
    in0 ← [message "random"]
  fan-out:
    out1 → [p live.grid2matrix @rows 16]:in0
```

```
Example #3 — [live.grid]
  fan-in:
    in0 ← [number] ← [counter 1 16] ← [metro 250]
  fan-out:
    out0 → [zl scramble]:in0
    out0 → [message "3"]:in1
```

```
Example #4 — [live.grid]
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [number]:in0
    out3 → [message "9 1 0"]:in1    # x y state
```

Attributes demonstrated: `@marker_horizontal`, `@marker_vertical`

### linking

```
Example #1 — [live.grid]
  fan-in:
    in0 ← [message "link $1"]
    in0 ← [r live.grid_linking_step]
    in1 ← [live.grid]
  fan-out:
    out1 → [prepend steps 2]:in0
    out1 → [button]:in0
    out2 → [button]:in0
    out5 → [button]:in0
    out5 → [live.grid]:in1
```

```
Example #2 — [live.grid]
  fan-in:
    in0 ← [message "link $1"]
    in0 ← [message "linkdump"]
    in0 ← [r live.grid_linking_step]
    in1 ← [live.grid]
  fan-out:
    out1 → [button]:in0
    out1 → [prepend steps 1]:in0
    out2 → [button]:in0
    out5 → [button]:in0
    out5 → [live.grid]:in1
```

### basic

```
Example — [live.grid]
  fan-in:
    in0 ← [number]
    in0 ← [message "set $1"]
    in0 ← [pak 1 1]
    in0 ← [pak 1 1 1]
    in0 ← [r cmd_to_live.grid]
  fan-out:
    out0 → [message "1 1"]:in1    # current step value(s) + direction (optional)
    out1 → [message "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16"]:in1    # steps values
    out2 → [message "1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1"]:in1    # steps directions
    out3 → [message "16 14 0 1"]:in1    # mouse over information
```

## See also

`live.step`, `multislider`, `itable`, `matrixctrl`, `chucker~`
