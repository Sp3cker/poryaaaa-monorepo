# funbuff

_max · Data_

> Store pairs of numbers

Stores, manages, and recalls pairs of numbers.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | list Inserts Element, int Retrieves Element |
| in1 | Sets Stored Element Value for Insertion |
| out0 | Stored Element Output |
| out1 | Index Output for next Message |
| out2 | bang Output When next Message Reaches End |

## Arguments

- **filename** (`symbol`) _(optional)_ — Filename of saved funbuff
  The argument specifies the name of a file to be read into funbuff when the patch is loaded. Changes to the contents of one funbuff will not affect the contents of another funbuff object with the same name.

 A file for funbuff can also be created using a text editor window, beginning the text with the word funbuff, followed by a list of space-separated numbers which specify alternating x and y values. A funbuff that has been saved as a file can be viewed and edited as text by choosing Open as Text... from the File menu. Numbers in the form of text can be pasted in from other sources such as the editing window of a capture object, or even from another program such as a word processor.

## Messages

- `bang` — Print information to the Max Console
  In left inlet: Prints information in the Max Console concerning the current status of the funbuff object's contents: how many elements it contains, the minimum and maximum x and y values it contains, and its domain and range (the maximum minus the minimum, for the x and y axes respectively).
- `int(x-value: int)` — Store or output a saved pair
  In left inlet: The number is the x value of an x,y pair. If a y value has been received in the right inlet, the two numbers are stored together in funbuff. Otherwise, the x value causes the corresponding y value stored in funbuff to be sent out the left outlet.
  If there is no stored x value which matches the number received, funbuff uses the closest x value which is less than the number received, and sends out the corresponding y value.
- `float(x or y value: float)` — Store or output a saved pair
  In either inlet: Converted to int.
- `clear` — Clear all contents
  Erases the contents of funbuff.
- `copy` — Copy data to clipboard
  Copies the current selection (made by using the select message) into the global funbuff clipboard. The data stored on this clipboard can then be pasted into another funbuff object using the paste message.
- `cut` — Cut data to clipboard
  Copies the current selection (made by using the select message) into the global funbuff clipboard and deletes it from the funbuff object. The data stored on this clipboard can then be pasted into another funbuff object using the paste message.
- `delete(x-value: int, y-value: int)` — Delete one value pair
  In left inlet: The word delete, followed by two numbers, looks for such an x,y pair in funbuff, and deletes it if it exists. If delete is followed by only one number, only the x value is sought, and deleted if it is present.
- `dump` — Output all value pairs
  In left inlet: Sends all the stored pairs out the middle and left outlets in immediate succession. The y values are sent out the middle outlet, and the x values are sent out the left outlet, in alternation. The pairs are sent out in ascending order based on the x value.
- `embed(flag: int)` — Set patcher embedding flag
  The word embed, followed by a non-zero number, causes the funbuff data to be stored inside the patcher. The default setting is not to store the funbuff data inside the patcher.
- `find(y-value: int)` — Output matching values
  The word find, followed by a number, will output (out the left outlet) all x values (indexes) whose y value is equal to the number indicated.
- `goto(index: int)` — Move the pointer to a specific value
  The word goto, followed by a number, sets a pointer to the x value (index) specified by the number. A subsequent next message will return the y value at the specified x.
- `in1(y-value: int)` — Provide Y value for storage
  In right inlet: The number is a y value which will be paired with the next x value received in the left inlet, and stored in funbuff.
- `interp(x-value: int)` — Calculate an intermediate value
  In left inlet: The word interp, followed by a number, uses that number as an x value, measures its position between its two neighboring x values in the funbuff, and then sends--out the left outlet--the y value that holds a corresponding position between the two neighboring y values. If the received number is already the x value in a stored x,y pair, the corresponding y value is sent out. If the received number exceeds the minimum or maximum x values stored in funbuff, the y value that's associated with the minimum or maximum x value is sent out. If the funbuff is empty, 0 is sent out.
- `interptab(value: int, tablename: symbol)` — Interpolate values using a table
  In left inlet: The word interptab, followed by a number and the name of a named table object functions similarly to the interp message (mentioned above), except that it uses the data in the table as an interpolating function. This allows you to easily perform non-linear interpolation between consecutive values in a funbuff.
- `max` — Output the maximum value
  Sends the maximum y value currently stored in the funbuff out the left outlet.
- `min` — Output the minimum value
  Sends the minimum y value currently stored in the funbuff out the left outlet.
- `next` — Move the pointer and output value
  Finds the x value pointed to by the pointer (or, if the pointer points to a number not yet stored as an x value, to the next greater x value), and sends the corresponding y value out the left outlet. Also, funbuff calculates the difference between that x value and the value previously pointed to by the pointer, sends the difference out the middle outlet, and resets the goto pointer to the next greater x value.
- `paste` — Paste data from clipboard
  The word paste will copy the contents of the global funbuff clipboard into a funbuff object. The contents of the clipboard are set using the select, copy and cut messages. These messages provide a handy way of copying data between different funbuff objects in any open patchers.
- `print` — Print diagnostics to the Max Console
  Prints diagnostic information regarding funbuff's current state in the Max Console.
- `read([filename: symbol])` — Load a stored funbuff
  Calls up the Open Document dialog box so that a file of x,y values can be read into funbuff. If the word read is followed by a symbol, Max looks for a file with that name (in the file search path) to load directly into the funbuff. The funbuff file format is described on the next page.
- `select(starting-index and range: list)` — Select a group of values
  In left inlet: The word select, followed by an two integers representing a starting index and a range will select a region of the funbuff which can be edited using the cut, copy and paste messages. For example select 2 3 will select the part of a funbuff from index 2 through index 5.
- `set(value-pair: list)` — In left inlet: The word set, followed by one or more space-separated pairs of numbers, stores each pair as x,y pair.
- `undo` — Undo previous cut or paste
  The undo message is used to undo the results of the previous cut or paste message.
- `write([filename: symbol])` — Save stored data to disk
  Calls up the standard Save As dialog box, so that the contents of funbuff can be saved as a separate file. If the word write is followed by a symbol, the contents of the funbuff are saved immediately in a file, using the symbol as the filename.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in1` — Sets Stored Element Value for Insertion

### interpolation

> "interptab" is just like "interp" but takes an additional argument which names the table in which an interpolation curve is defined.

```
Example — [funbuff]
  fan-in:
    in0 ← [message "0 0, 127 $1"]
    in0 ← [message "interptab $1 shupsh"]
  fan-out:
    out0 → [slider]:in0
```

### basic

> list of two numbers in left (or int in right followed by int in left) inserts or replaces a list element

```
Example #1 — [funbuff]
  fan-in:
    in0 ← [message "clear"]    # Clear all data
    in0 ← [message "write"]    # File I/0
    in0 ← [button]    # Information
    in0 ← [message "goto 2"]    # "goto n" can be used to specify the "next" event
    in0 ← [message "embed 1"]    # save funbuff in patcher: no (default) -- yes --
    in0 ← [message "select 3 5"]    # selects the region of the funbuff from x1 to x1+x2
    in0 ← [message "copy"]
    in0 ← [message "embed 0"]
    in0 ← [number]
    in0 ← [message "next"]
    in0 ← [message "read"]
    in0 ← [message "1 2"]
    in0 ← [message "3 4"]
    in0 ← [message "5 6"]
    in0 ← [message "5 3"]
    in0 ← [message "cut"]
    in0 ← [message "find 4"]
    in0 ← [message "dump"]    # dump all
    in0 ← [message "interp 4"]    # "find n" outputs all x values at which n is stored / interpolate
  fan-out:
    out0 → [number]:in0    # value output
    out1 → [number]:in0    # element count (next message)
    out2 → [button]:in0    # "next" reached end (send 1 to reset)
```

```
Example #2 — [funbuff]
  fan-in:
    in0 ← [message "undo"]    # one may paste among funbuffs
    in0 ← [message "paste"]
    in0 ← [button]
```

## See also

`bline`, `coll`, `funbuff`, `itable`, `line`, `table`
