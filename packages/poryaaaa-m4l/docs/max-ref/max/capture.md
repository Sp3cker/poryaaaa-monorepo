# capture

_max · Data_

> Store values to view or edit

Stores items in the order they are received for viewing, editing, and saving.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Data to be Captured |
| out0 | Sequential Output from dump Message |
| out1 | Item Count Outlet |

## Arguments

- **maximum** (`int`) _(optional)_ — Maximum number of items.
  The first argument sets a maximum number of items to store. If there is no argument, capture will store up to 512 items. Once the maximum has been exceeded, the earliest stored item is dropped as each new item is received.
- **display-format** (`symbol`) _(optional)_ — Formatting of data display
  If the second argument is a, all items will be displayed in ASCII form in the editing window. If the second argument is x, all numbers will be displayed in hexadecimal form in the editing window. If the second argument is m, numbers less than 128 are displayed in decimal, and numbers greater than 128 are in hexadecimal. If there is no argument, all items are displayed in decimal.

## Messages

- `int(input: int)` — Capture value and store
  Numbers or symbols are stored in the order in which they are received.
- `float(input: float)` — Capture value and store
  Numbers or symbols are stored in the order in which they are received.
- `list(input: list)` — Capture values and store
  All numbers and/or symbols in the list are stored in order from first to last.
- `anything(input: list)` — Capture values and store
  All numbers and/or symbols are stored in order from first to last.
- `clear` — Clear stored values
  Erases the contents of a capture object.
- `count(flag: int)` — Report item count
  Sends the number of items collected since the last count message out the right outlet of the capture object. Upon receipt of the count message, the object's internal count will be reset to 0 unless flag is set.
- `dump` — Output all stored values
  Outputs the contents of the capture object, one item at a time, out the left outlet.
- `open` — Open the viewing window
  Causes the window associated with the capture object to become visible. The window is also brought to the front. Double-clicking on the capture object in a locked patcher has the same effect.
- `wclose` — Close the viewing window
  Closes the window associated with the capture object.
- `write(filename: symbol)` — Write captured data to disk
  The word write, followed by a symbol, saves the contents of the capture object into a text file, using the symbol as the filename.

## GUI behaviors

- `(mouse)` — Display captured values
  Double-clicking on the object in a locked patcher will open a window which displays all values stored internally.

## Attributes

- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `sgoenn` — seen as: `sgoenn`

## Help patcher examples

### basic

> Any numbers collected by capture are not saved, however, you can copy the numbers into a table.
>
> report number of items received out right outlet since last count message received
>
> write data to file, optional argument specifies filename
>
> open and close text editor window

```
Example #1 — [capture 4000 m]  <-- double-click to open text editor
  fan-in:
    in0 ← [midiin]
    in0 ← [message "dump"]    # "dump" spews contents out one number at a time (could be used for dumping raw MIDI)
  fan-out:
    out0 → [print]:in0
```

```
Example #2 — [capture]
  fan-in:
    in0 ← [message "sgoenn"]
    in0 ← [message "3.4"]
    in0 ← [slider]    # Drag the slider back and forth, then double-click on the capture object below / capture captures floats and symbols too!
    in0 ← [attrui @size]    # specify max size
    in0 ← [message "write"]
    in0 ← [message "count"]
    in0 ← [attrui @listout]    # dump contents as a list
    in0 ← [message "wclose"]
    in0 ← [message "open"]
  fan-out:
    out1 → [number]:in0
```

Attributes demonstrated: `@listout`, `@size`

## See also

`itable`, `text`
