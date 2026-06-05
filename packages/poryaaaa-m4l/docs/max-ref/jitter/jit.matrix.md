# jit.matrix

_jit · Jitter Data_

> The Jitter Matrix!

The jit.matrix object is a named matrix which may be used for data storage and retrieval, resampling, and matrix type and planecount conversion operations.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | matrix | in |
| out0 | matrix | out |
| out1 | matrix | dumpout |

## Messages

- `bang` — Output the currently stored matrix
  Outputs the currently stored matrix.
- `int(values: list)` — Set all cells to a value and output the result
  Sets all cells to the value specified by value(s) and output the data. Position is specified of a list whose length is equal to the number of dimensions (dimcount).
- `float(values: list)` — Set all cells to a value and output the result
  Sets all cells to the value specified by value(s) and output the data. Value is specified as a list whose length is equal to the number of dimensions (dimcount).
- `list(values: list)` — Set all cells to a value and output the result
  Sets all cells to the value specified by value(s) and output the data. Position is specified of a list whose length is equal to the number of dimensions (dimcount).
- `clear` — Set all matrix values to zero
  Sets all matrix values to zero.
- `exportimage(filename: symbol, file-type: symbol)` — Export the current frame as an image file
  Export the current frame as an image file with the name specified by the first argument. The second argument sets the file type (default = png). Available file types are png, tiff, and jpeg.
  You can use the Max Preferences to specify a default image resolution for PNG images.
- `exportmovie([filename: symbol], FPS: float, codec: symbol, quality: symbol, timescale: int)` — Export a matrix as a movie
  Exports a matrix as a movie. The exportmovie message takes an optional argument to specify a file name. If no filename is specified, a file dialog will open to let you choose a file.
  See the jit.record object for information on the write arguments and their default values.
- `exprfill([plane: int], expression: symbol)` — Evaluate an expression to fill the matrix
  Evaluates expression to fill the matrix. If a plane argument is provided, the expression is applied to a single plane. Otherwise, it is applied to all planes in the matrix. See jit.expr for more information on expressions. Unlike the jit.expr object, there is no support for providing multiple expressions to fill multiple planes at once with different expressions. Call this method multiple times once for each plane you wish to fill.
- `fillplane(plane: int, value: int)` — Fill a plane with a specified value
  The word fillplane, followed by an integer that specifies a plane number and a value, will fill the specified plane with the single value.
- `getcell(position: list)` — Report cell values
  Sends the value(s) in the cell specified by position out the right outlet of the object as a list in the form cell cell-position0... cell-positionN val plane0-value... planeN-value.
- `importmovie([filename: symbol], time-offset: int)` — Import a movie into the matrix
  Imports a movie into the matrix. If no filename is specified, a file dialog will open to let you choose a file. The time-offset argument may be used to set a time offset for the movie being imported (the default is 0).
- `jit_gl_texture(texture-name: symbol)` — Copy a texture to the matrix
  Copies the texture specified by texture-name to the matrix.
- `op` — Perform jit.op operations on the matrix
  The word op, followed by the name of a jit.op object operator and a set of values, is equivalent to including a jit.op object with the specified operator set as an attribute and this jit.matrix object specified as the output matrix. The additional value arguments may either be a matrix name or a constant. If only one value argument is provided, this matrix is considered both the output and the left operand. For example, "op + foo bar" is equivalent to the operation thismatrix = foo + bar, and "op * 0.5" is equivalent to the operation thismatrix = thismatrix * 0.5.
- `read([filename: symbol])` — Read Jitter binary data files (.jxf)
  Reads Jitter binary data files (.jxf) into a matrix set. If no filename is specified, a file dialog will open to let you choose a file.
- `setall(values: list)` — Set all cells to a value
  Sets all cells to the value specified by value(s). Position is specified of a list whose length is equal to the number of dimensions (dimcount).
- `setcell(position: list, plane: literal, plane-number: int, val: literal, values: list)` — Set a cell to a specified value
  Sets the cell specified by position to the value specified by value. Position is specified of a list whose length is equal to the number of dimensions (dimcount). The optional arguments plane plane-number can be used to specify a plane. If a plane is specified, value should be a single number, otherwise it should be a list of numbers of size planecount - 1.
- `setcell1d` — Set a 1-dimensional cell to a specified value
  The word setcell1d, followed by a number specifying an x coordinate and a list of values, is similar to the setcell message but without the need to use a "val" token to separate the coordinates from the value since the dimension count (1) is fixed.
- `setcell2d` — Set a 2-dimensional cell to specified values
  The word setcell2d, followed by a pair of numbers specifying x and y coordinates and a list of values, is similar to the setcell message but without the need to use a "val" token to separate the coordinates from the value since the dimension count (2) is fixed.
- `setcell3d` — Set a 3-dimensional cell to specified values
  The word setcell3d, followed by three numbers specifying x, y, and z coordinates and a list of values, is similar to the setcell message but without the need to use a "val" token to separate the coordinates from the value since the dimension count (3) is fixed.
- `setplane1d` — Set a cell in a plane to a value (1d, no val token)
  The word setplane1d, followed by a number specifying an x coordinate, a number specifying a plane, and a value, is similar to the setcell message but without the need to use a "val" token to separate the coordinates from the value since the dimension count (1) is fixed, or use the "plane" token to specify which plane to set.
- `setplane2d` — Set a cell in a plane to a value (2d, no val token)
  The word setplane2d, followed by a pair of numbers specifying x and y coordinates, a number specifying a plane, and a value, is similar to the setcell message but without the need to use a "val" token to separate the coordinates from the value since the dimension count (2) is fixed, or use the "plane" token to specify which plane to set.
- `setplane3d` — Set a cell in a plane to a value (3d, no val token)
  The word setplane3d, followed by three numbers specifying x, y, and z coordinates, a number specifying a plane, and a value, is similar to the setcell message but without the need to use a "val" token to separate the coordinates from the value since the dimension count (1) is fixed, or use the "plane" token to specify which plane to set.
- `val(values: list)` — Set all cells to a value and output the result
  Sets all cells to the value specified by value(s). Position is specified of a list whose length is equal to the number of dimensions (dimcount) and outputs the data.
- `write([filename: symbol])` — Write matrix set as a Jitter binary data file (.jxf)
  Writes matrix set as a Jitter binary data file (.jxf). If no filename is specified, a file dialog will open to let you choose a file.

## GUI behaviors

- `(drag)` — Load a compatible media file
  Drag and drop a compatible media file onto the object to load it.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### filling

```
Example — [jit.matrix 4 char 5 5]
  fan-in:
    in0 ← [pak 255 0 0 0] ← [number]    # list
    in0 ← [t b l]
    in0 ← [t b l]
    in0 ← [message "clear, bang"]
    in0 ← [number]    # number
  fan-out:
    out0 → [jit.pwindow]:in0
    out0 → [t b l]:in0
```

### files

```
Example — [jit.matrix 4 char 320 240]
  fan-in:
    in0 ← [message "exportimage jpeg"]    # exportimage <name(optional)> <file type(def=png)> <1(optional = force settings dialog)> / available file types are tiff png and jpeg
    in0 ← [message "write"]
    in0 ← [message "read, bang"]    # read/write matrix in jit binary format
    in0 ← [message "importmovie, bang"]    # importmovie <name(optional)> <time offset(def=0)>
  fan-out:
    out0 → [jit.pwindow]:in0
    out1 → [print @popup 1]:in0
```

### conversions

```
Example — [jit.matrix 4 char 320 240]
  fan-in:
    in0 ← [attrui @type]
    in0 ← [attrui @usedstdim]
    in0 ← [attrui @dstdimstart]
    in0 ← [attrui @dstdimend]
    in0 ← [bpatcher]
    in0 ← [attrui @adapt]
    in0 ← [attrui @planemap]
    in0 ← [attrui @dim]
    in0 ← [attrui @planecount]
    in0 ← [attrui @interp]
  fan-out:
    out0 → [jit.pwindow]:in0
```

Attributes demonstrated: `@adapt`, `@dim`, `@dstdimend`, `@dstdimstart`, `@interp`, `@planecount`, `@planemap`, `@type`, `@usedstdim`

### basic

```
Example #1 — [jit.matrix this]
  fan-in:
    in0 ← [button]    # multiple copies of a named matrix access the same data
  fan-out:
    out0 → [jit.pwindow]:in0
```

```
Example #2 — [jit.matrix this 4 char 5 5]
  fan-in:
    in0 ← [pak getcell 0 0]
    in0 ← [t b l clear] ← [pak setcell 0 0 val 0 0 0 0 0]    # use setcell messages to change the cell values of the matrix
    in0 ← [t b l clear] ← [pak setcell 0 0 val 0 0 0 0 0]    # use setcell messages to change the cell values of the matrix
    in0 ← [t b l clear] ← [pak setcell 0 0 val 0 0 0 0 0]    # use setcell messages to change the cell values of the matrix
    in0 ← [attrui @dim]
    in0 ← [attrui @type]
    in0 ← [attrui @planecount]
  fan-out:
    out0 → [jit.pwindow]:in0
    out0 → [jit.cellblock]:in0
    out1 → [print @popup 1]:in0
```

Attributes demonstrated: `@dim`, `@planecount`, `@type`

## See also

`jit.coerce`, `jit.fill`, `jit.matrixset`, `jit.matrixinfo`, `jit.peek~`, `jit.poke~`, `jit.spill`, `jit.submatrix`
