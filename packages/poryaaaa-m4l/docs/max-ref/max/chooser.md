# chooser

_Max · U/I_

> Display a scrolling list of selectable items

The chooser object is similar to the umenu object, but it displays a scrolling list of selectable items rather than a pop-up menu.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Messages |
| out0 | Index (Message or Single-Click) |
| out1 | Item (Message or Single-Click) |
| out2 | Index (Double-Click) |
| out3 | Item (Double-Click) |
| out4 | Preview Control |
| out5 | Dump Outlet |

### Port details

**`out0` (Index (Message or Single-Click)):** The index of the list item received as a message or the item chosen by a single mouse click.

**`out1` (Item (Message or Single-Click)):** The text of the item received as a message or the item chosen by a single mouse click.

**`out2` (Index (Double-Click)):** The index of the item chosen by a double mouse click.

**`out3` (Item (Double-Click)):** The text of the item chosen by a double mouse click.

**`out4` (Preview Control):** Connect this inlet to the left inlet of an sfplay~, imovie, movie, jit.movie, or jit.movie~ object to control these objects to play files when preview is enabled.

## Arguments

- **OBJARG_NAME** (`OBJARG_TYPE`) — None

## Messages

- `bang` — Report current item
  Sends the contents of the currently selected item out the second outlet and the index of the currently selected out the left outlet.
- `int(index: int)` — Sets current item by index
  Sets the currently selected item to the specified index, then sends that item's contents out the second outlet and its index out the left outlet.
- `float(index: float)` — Sets current item by index
  Sets the currently selected item to the specified index, then sends that item's contents out the second outlet and its index out the left outlet.
- `anything(contents: list)` — Sets current item by contents
  If the chooser object contains an item whose contents matches the message, that item will be selected. Then the item's contents will be sent out the second outlet and its index will be sent out the left outlet. If there is no match, the currently selected item is not changed and no output occurs.
- `append(message: list)` — Adds a chooser item
  The word append, followed by any message, appends that message to the end of the chooser object's item list.
- `clear` — Removes all items
- `count` — Reports the number of items
  Sends the number of items currently in the item list out the right outlet, preceded by the word count.
- `delete(indices: list)` — Removes one or more items
  The word delete, followed by one or more item numbers, removes the numbered items from the item list.
- `deselect` — Deselect currently selected item
  The word deselect removes the visual reprepesentation of the selected item from the chooser object. It does not change the chooser object's current value, so a bang will still output the last selected item.
- `dictionary(dict-name: symbol)` — Set the items for chooser with a dictionary
  The word dictionary, followed by a dictionary name, will set the items for chooser. You can also attach the first outlet of a dict object to the first inlet of chooser. In order for this to work, the dictionary needs to include an "items" entry. For example, the following dictionary entry will populate chooser with the items "hank, carol, andreas, roland":
  "items" : [ "hank", "carol", "andreas", "roland" ]
- `insert(index: int, message: list)` — Inserts an item
  The word insert, followed by an index number and a message, inserts a new item to the list at the position specified by the index.
- `next` — Selects the next item
  The next message selects the next item in the list, then sends the selected item's contents out the second outlet and its index out the left outlet.
- `play(index: int)` — Previews a media file
  The word play, followed by an index number, sends a command to an object (such as sfplay~ or jit.movie) connected to the fifth outlet to open and play the specified media file. The index argument selects the file to play.
- `prev` — Selects the previous item
  The prev message selects the previous item in the list, then sends the selected item's contents out the second outlet and its index out the left outlet.
- `progress(position: float)` — Sets the relative preview location
  When the preview attribute is on, the word progress, followed by a float between 0.0 and 1.0, will update the display of the current preview progress circle.
- `set(index: int)` — Selects an item without output
  The word set, followed by a number, sets the currently selected item to the specified index, but does not produce any output.
- `setnext` — Selects the next item without output
  Selects the item below the currently selected item in the list, but does not produce any output.
- `setprev` — Selects the previous item without output
  Selects the item above the currently selected item in the list, but does not produce any output.
- `sort(direction: int)` — Sorts items by contents
  The word sort followed by a positive number, sorts the chooser object's items alphetically in ascending order (A-Z). sort followed by a negative number sorts the list of items alphabetically in descending order (Z - A).
- `stop` — Stops preview playback
  The word stop causes the current preview playback of any item to stop.

## GUI behaviors

- `(mouse)` — Selects an item
  A single-click on an item selects the item, causing its contents to be sent out the second outlet and its index to be sent out the left outlet. A double-click on an item sends its contents out the fourth outlet and its item index out the third outlet.

## Attributes

- `@category` (symbol)
- `@default` (int)
- `@label` (symbol)
- `@save` (int)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `autopopulate` — seen as: `autopopulate $1`
- `collection` — seen as: `collection "Vizzie Generate"`
- `dentist` — seen as: `dentist`
- `enabledrag` — seen as: `enabledrag $1`
- `factorycontent` — seen as: `factorycontent $1`
- `filekind` — seen as: `filekind audiofile`, `filekind audiofile, autopopulate 1`, `filekind moviefile`
- `preview` — seen as: `preview $1`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out5` — Dump Outlet

### selection

> When selectedclick is enabled, clicking on an already selected item reports that item. Otherwise items are only reported when the selection changes.

```
Example #1 — [chooser]  When multiselect is enabled, chooser outputs the messages selectedindices and selecteditems with one or more currently selected items. Use shift to select multiple items.
  fan-in:
    in0 ← [attrui @multiselect]
  fan-out:
    out0 → [route selectedindices]:in0
    out1 → [route selecteditems]:in0
```

```
Example #2 — [chooser]  click on an item, then click on it again and watch whether the large button below lights up
  fan-in:
    in0 ← [attrui @selectedclick]    # compare output behavior when selectedclick is enabled and disabled
  fan-out:
    out0 → [number]:in0
```

Attributes demonstrated: `@multiselect`, `@selectedclick`

### preview

> when enabling preview, preview will be dimmed if there is no connected sfplay~ or jit.movie

```
Example #1 — [chooser]
  fan-in:
    in0 ← [message "filekind moviefile, factorycontent 1, autopopulate 1"]
    in0 ← [message "preview $1"]
  fan-out:
    out4 → [jit.movie]:in0
```

```
Example #2 — [chooser]
  fan-in:
    in0 ← [message "preview $1"]
    in0 ← [message "filekind audiofile, autopopulate 1"]
  fan-out:
    out4 → [sfplay~]:in0    # connect to fifth outlet of chooser
```

### files

```
Example — [chooser]
  fan-in:
    in0 ← [message "filekind moviefile"]    # use database query types to populate the list
    in0 ← [message "autopopulate $1"]
    in0 ← [message "factorycontent $1"]
    in0 ← [message "filekind, collection, prefix C74:/media/msp"]    # If the filekind or collection are set, they must be cleared before the prefix attribute will show a folder's contents; the above message is a way to clear them / show a folder of files
    in0 ← [message "clear"]
    in0 ← [message "enabledrag $1"]    # enable drag 'n' drop from the file list
    in0 ← [message "filekind audiofile"]
    in0 ← [message "collection "Vizzie Generate""]    # show a collection
  fan-out:
    out0 → [number]:in0    # single click
    out2 → [number]:in0    # double click
```

### basic

> item text

> item text

```
Example — [chooser]
  fan-in:
    in0 ← [message "append doctor, append dentist, append nurse"]
    in0 ← [number]    # select item by index
    in0 ← [message "set $1"]
    in0 ← [message "dentist"]    # select by contents
    in0 ← [message "deselect"]    # remove the visual highlight on the selected entry (does not change output value)
    in0 ← [message "clear"]    # clear all items
  fan-out:
    out0 → [number]:in0
    out1 → [message "nurse"]:in1
    out2 → [number]:in0
    out3 → [message "doctor"]:in1
```

## See also

`umenu`, `sfplay~`, `folder`, `jit.playlist`, `playlist~`
