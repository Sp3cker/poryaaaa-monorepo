# jit.cellblock

_max · U/I_

> Edit rows and columns of data

Provides storage, viewing and editing of two-dimensional data. The format is similar to the "grid" display tools found in many other development environments. The current cell location, format, display and contents within jit.cellblock can be set with the mouse or by using Max messages.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | — | anything in, bang outputs value |
| in1 | list | sync input |
| out0 | list | column, row and value |
| out1 | list | edit values for cell |
| out2 | list | sync output |
| out3 | list | dumpout |

## Messages

- `bang` — Output the current cell contents
  Sends the contents out the object's left outlet, and sends a message through the third outlet in the form set value-list.
- `list(column: int, row: int)` — Select a cell
  Selects a cell within the cellblock. The message list col-number row-number is equivalent to the message select col-number row-number.
- `append(input: list)` — Append data to a cell
  The word append, followed by a data element or a list of elements, will add the specified valid Max data to the contents of the currently selected cell.
- `cell(column: int, row: int, setting: symbol, value: list)` — Control the appearance of a cell
  The cell message allows you to control the appearance of a single cell within the cellblock. Using the cell message will override changes for both row and col message.
- `clear([option: list])` — The clear message removes data from the cellblock.
  The clear message removes data from the cellblock. The message clear clears the contents of the specified cell. clear all will clear the entire contents of the cellblock. The message clear current will clear the contents of the currently selected cell(s). The clear message with no arguments is equivalent to the message clear current.
- `col(column: int, setting: symbol, value: list)` — Control the appearance of a column
  The col message allows you to control the appearance of a single column within the cellblock. Using the col message to override color settings will override any color changes made with the row message. Using the cell message will, however, override color changes for both col and row messages.
- `deref` — Remove reference to a coll
  Disconnects a jit.cellblock object from any attached coll objects.
- `dump` — Output all non-empty cell contents
  Sends a listing of the contents of all non-empty cells out the object's left outlet, one line per cell. Each output line takes the form col-number row-number cell-contents.
- `jit_deref` — Remove reference to a jit.matrix
  The message jit_deref will dereference the currently referenced jit.matrix object.
- `jit_matrix(name: symbol)` — Create reference to a jit.matrix
  The message jit_matrix will cause the jit.cellblock object's display/edit to directly reference the received jit.matrix rather than maintaining its own data structure.
- `mode(operational-mode: list)` — Set operational modes
- `plane(input: int)` — Select a jit.matrix plane to display
  The word plane followed by an integer that specifies a numbered plane in a jit.matrix object, will display that plane. The special message plane -1 will display all four planes of an RGBA jit.matrix object simultaneously.
- `prepend(input: list)` — Prepend data to a cell's contents
  Adds the specified valid Max data to the beginning of the currently selected cell contents.
- `read([filename: symbol])` — Read cellblock contents from a file
  Opens and reads the contents of a cellblock file from disk if a filename is specified. No attempts are made to verify the contents. If no filename is specified, a file dialog box will be displayed to allow selection of a saved cellblock file.
- `refer(name: symbol)` — Create a reference to a coll object
  The word refer, followed by the name of a coll, object, displays the contents of the named coll object's internal list. Changes to the data in the jit.cellblock will change the contents of the attached coll object.
- `refresh` — Force a redraw
  Causes the jit.cellblock object to be redrawn.
- `row(row: int, setting: symbol, value: list)` — Control the appearance of a row
  The row message allows you to control the appearance of a single row within the cellblock.
- `rowblend(foreground: int, background: int)` — Set row/column color blending
  The word rowblend, followed by two numbers in the range 0-100 specifying foreground and background blend percentages. blends the foreground and background colors for the cellblock object.
- `select(column-number: int, row-number: int)` — Select a cell
  The select message, followed by a column number and row number, will select the requested cell using the current selmode. The leftmost outlet of jit.cellblock outputs a message in the format of [column, row, cell contents]. The third outlet outputs a message in the format of sync select [column, row], which can be connected to another jit.cellblock for synchronization between two cellblocks.
- `send(name: symbol, option: list)` — Send data to a receive object
  The word send, followed by the name of a receive object, will transmit cellblock values without using connected patch cords; it is the equivalent of sending the output through a send object.
  send receive-object col-number row-number will send the data in the specified cell to the specified received object. send receive-object all sends all non-empty cell contents to the specified receive object as a series of lists in the form cell-data-type value.
- `set(input: list)` — Replace cell data
  Replaces a cell's data with the data specified. Two forms are supported:
  set current value replaces the currently selected cell's contents.
  set col-number row-number values replaces the specified cell's contents.
- `setwithoutdirty(message: list)` — Send message without dirtying patcher
  The word setwithoutdirty followed by any message to jit.cellblock, executes that message without dirtying the patcher. Example: setwithoutdirty col 0 just 1 sets the text justification of column 0 without dirtying the patcher, which would normally occur when sending this message. You can also disable patcher dirtying permanently by enabling the neverdirty attribute. Ironically, doing so will itself dirty the patcher.
- `signal` — Display signal input
  Displays signal input based on the signalmode attribute setting.
- `sync(messages: list)` — Allow synchronization between jit.cellblock objects
  Receives input in the jit.cellblock object's right inlet from another jit.cellblock object, so that two cellblocks can maintain location and selection synchronization. Synchronization allows for multiple jit.cellblock objects to react as if connected. One use of synchronization is used to force external cellblock objects to act as header rows and columns for a main jit.cellblock. For more information on setting synchronization, see the mode message.
- `text(input: list)` — Replace cell contents with text
  Replaces the value of the currently selected cell with the incoming text values. This is provided as a convenient way of receiving the output of a textedit object.
- `write([filename: symbol])` — Write all data to a disk file
  Opens a file and writes the contents of a cellblock file to disk. If a filename is specified, that file is created and written. If no filename is specified, a file dialog box will be displayed to allow selection of a pathname and file.
- `writeagain` — Repeat the previous file write
  If a file has been written, the writeagain message will allow it to be rewritten without further user interaction.

## Attributes

- `@category` (atom)
- `@default` (int)
- `@label` (symbol)
- `@save` (int)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out3` — dumpout

### coll

> Editing of existing contents is supported, but you cannot add cells, rows or columns - this must be done with direct changes to the coll itself.

```
Example — [jit.cellblock]
  fan-in:
    in0 ← [message "deref"]    # disable a coll connection
    in0 ← [message "refer theColl"]    # enable a coll connection
```

### dim messages

```
Example — [jit.cellblock]
  fan-in:
    in0 ← [r dim_command]
```

### cell messages

```
Example — [jit.cellblock]
  fan-in:
    in0 ← [message "cell 1 1 frgb, cell 1 1 brgb"]    # remove cell-specific colors
    in0 ← [message "cell 1 1 brgb $1 $2 $3"]
    in0 ← [message "cell 1 1 frgb $1 $2 $3"]
    in0 ← [r cb-cell]
```

### remote send

> use a loopback from the middle-right inlet to send the value on every selection change.

```
Example — [jit.cellblock]
  fan-in:
    in0 ← [p loader] ← [loadbang]    # p loader emits: "set $1 $2 ( $1 : $2 )"
    in0 ← [message "send myVals 2 2"]    # click on a send message to send one or all cell data to a named receive object
    in0 ← [message "send myVals all"]
    in0 ← [message "send myVals"]
  fan-out:
    out2 → [button]:in0
```

### sync

> The syncronization command from the third outlet, and sent to the right-most inlet of a similar jit.cellblock, will cause two (or more) jit.cellblocks to syncronize their movements, selections and viewports. This is a handy way of viewing multiple planes or dimensions, and for coordinating the selection from multiple sets of data.

```
Example #1 — [jit.cellblock]
  fan-in:
    in1 ← [jit.cellblock]
  fan-out:
    out2 → [jit.cellblock]:in1
```

```
Example #2 — [jit.cellblock]
  fan-in:
    in1 ← [jit.cellblock]
  fan-out:
    out2 → [jit.cellblock]:in1
```

### editing

> The middle-left outlet provides a "set" command suitable for piping into a textedit object for simple editing. By connecting a "select" message to this outlet, you can cause the focus to stay on the textedit control for ease of entry.

```
Example — [jit.cellblock]
  fan-in:
    in0 ← [textedit]    # enter text to be placed in the current cell
  fan-out:
    out0 → [print @popup 1]:in0
    out1 → [t select l]:in0
```

### selection

> in-place edit mode allows you to select a cell then immediately edit it directly within the grid display

```
Example — [jit.cellblock]
  fan-in:
    in0 ← [message "2 2"]
    in0 ← [attrui @selmode]    # choose a selection mode
    in0 ← [attrui @outmode]    # choose an output mode
    in0 ← [message "select 3 3"]
  fan-out:
    out0 → [print @popup 1]:in0
```

Attributes demonstrated: `@outmode`, `@selmode`

### setting values

```
Example — [jit.cellblock]
  fan-in:
    in0 ← [message "set 1 2 Some more 1 2 3's and text"]    # use set to place a new value in any of the cells
    in0 ← [message "set 1 0 2.25"]
    in0 ← [message "set 0 1 152"]
    in0 ← [prepend set current]
    in0 ← [r cb-setting]
```

### structure

```
Example — [jit.cellblock]
  fan-in:
    in0 ← [attrui @readonly]
    in0 ← [attrui @cols]
    in0 ← [attrui @rows]
    in0 ← [attrui @colwidth]
    in0 ← [attrui @rowheight]
    in0 ← [attrui @precision]
```

Attributes demonstrated: `@cols`, `@colwidth`, `@precision`, `@readonly`, `@rowheight`, `@rows`

### appearance

```
Example — [jit.cellblock]
  fan-in:
    in0 ← [attrui @just]
    in0 ← [attrui @hcellcolor]
    in0 ← [attrui @headercolor]
    in0 ← [attrui @fontface]
    in0 ← [attrui @fontsize]
    in0 ← [attrui @fontname]    # Text attributes
    in0 ← [attrui @grid]
    in0 ← [attrui @textcolor]
    in0 ← [attrui @border]
    in0 ← [attrui @vscroll]
    in0 ← [attrui @hscroll]
    in0 ← [attrui @rowhead]
    in0 ← [attrui @colhead]
    in0 ← [attrui @fgcolor]
    in0 ← [attrui @bordercolor]
    in0 ← [attrui @gridlinecolor]
    in0 ← [attrui @bgcolor]
```

Attributes demonstrated: `@bgcolor`, `@border`, `@bordercolor`, `@colhead`, `@fgcolor`, `@fontface`, `@fontname`, `@fontsize`, `@grid`, `@gridlinecolor`, `@hcellcolor`, `@headercolor`, `@hscroll`, `@just`, `@rowhead`, `@textcolor`, `@vscroll`

### basic

```
Example — [jit.cellblock]  click a cell to send its location and data from the left outlet
  fan-out:
    out0 → [print @popup 1]:in0
```

## See also

`coll`, `maximum`, `minimum`
