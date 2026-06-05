# umenu

_max · U/I_

> Pop-up menu

Displays text as a pop-up menu. Selections can be made manually, or set incoming numbers. Outputs both selection number and selection text.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Messages in |
| out0 | Item Number Chosen |
| out1 | Menu Item Text Evaluated as a Message |
| out2 | Dumpout |

## Messages

- `bang` — Output current selection
  Sends out the currently displayed menu item.
- `int(index: int)` — Select a menu item programmatically
  The number specifies a menu item to be sent out, and causes umenu to display that item. The items are numbered starting at 0. A menu item can also be chosen from a umenu with the mouse, as with any pop-up menu.
- `float(index: float)` — Select a menu item programmatically
  Converted to int.
- `append(message: list)` — Add a menu item
  The word append, followed by any message, appends that message as the new last item in the menu.
- `checkitem(index: int, checked: int)` — Set a menu item check mark
  The word checkitem, followed by an item number and 1 or 0, places (1) or removes (0) a check mark next to the item number.
- `clear` — Remove menu items
  Removes all items from the umenu.
- `clearchecks` — Remove all menu check marks
  The word clearchecks removes check marks for all items.
- `count` — Report the number of menu items
  Sends the number of items in the umenu out the right outlet, preceded by the word count.
- `delete(indices: list)` — Remove select menu items
  The word delete, followed by one or more numbers that correspond to items in the list, deletes the item or items from the umenu.
- `dictionary(dict-name: symbol)` — Set the items for umenu with a dictionary
  The word dictionary, followed by a dictionary name, will set the menu items for umenu. You can also attach the first outlet of a dict object to the first inlet of umenu. In order for this to work, the dictionary needs to include an "items" entry. For example, the following dictionary entry will populate umenu with the items "hank, carol, andreas, roland":
  "items" : [ "hank", "carol", "andreas", "roland" ]
- `dump` — Dump contents of umenu out rightmost outlet.
  Output is the word 'dump', the item's entry number, and the contents. ex: 'dump 1 one'.
- `enableitem(index: int, enable: int)` — Enable or disable menu items
  The word enableitem, followed by a number that specifies a menu item and a 1 or 0, enables or disables the specified item number. Disabled menu items cannot be selected, but their text and item number are sent from the rightmost outlet if the mouse is released while above them, prefixed by the symbols disabled_eval and disabled_item, respectively.
- `gettoggle` — Report toggle state
  Reports the current value of the umenu object's toggle state (1 or 0, for on or off), from the right outlet, preceded by the word toggle.
- `insert(index: int, message: list)` — Insert a menu item
  The word insert, followed by a number and a message, inserts the message at the address specified by the number, incrementing all equal or greater addresses by 1 if necessary.
- `mode(display: int)` — Set menu appearance and behavior
  Legacy message. Please use the menumode attribute. The word mode, followed by a number in the range 1-4, sets the appearance and behavior of the umenu object. The normal pop-up menu style is 1 (the default). Scrolling mode (2) lets you scroll through the individual menu items by dragging the mouse up or down, displaying one item at a time. Label mode (3) shows the text of the selected menu item with no border around it, and does not respond to the mouse. Toggle mode (4) sets a button style. Clicking on the object in mode 3 causes it to alternate between an active and inactive state. When changing from inactive to active, the object sends the message toggle 1 from its rightmost outlet. When changing from active to inactive, the object sends the message toggle 0 from its rightmost outlet, and changes to the color set using the bgcolor attribute. Whether activating or deactivating, the object also sends its current message from the middle outlet and its current item number from the left outlet.
- `next` — Select the next menu item
  Selects the next menu item and causes the umenu object to display that item. This message will ignore disabled or separator menu items.
- `populate` — Populate the menu with file names
  If the umenu object has a valid folder path as its prefix, the populate message will cause the menu to re-populate its items list, based on the current contents of the specified path (and filtered by the types list). After population is complete, the number of items added to the umenu object will be output from the right outlet, preceded by the word populate.
- `prev` — Select the previous menu item
  Selects the previous menu item and causes the umenu object to display that item. This message will ignore disabled or separator menu items.
- `set(item: list)` — Select a menu item with no output
  The word set, followed by a number or symbol, specifies a menu item to be displayed by umenu, but does not send it out the outlet. If the set argument is a symbol, set searches for a menu item which begins with the symbol.
- `setcheck(character: int)` — Set check mark character
  (Macintosh only) The word setcheck, followed by a number that specifies the decimal representation of a UTF-8 character (e.g. 8226 for a bullet or 62 for a greater than symbol), sets the character used to be the check mark. The word setcheck with no argument specifies the default square root checkbox.
- `setitem(index: int, message: list)` — Set menu item text
  The word setitem, followed by an item number and any message, sets the specified menu item to that message.
- `setrgb(fore-red: int, fore-green: int, fore-blue: int, back-red: int, back-green: int, back-blue: int)` — Set menu colors
  The word setrgb, followed by six numbers between 0 and 255 that specify RGB values, uses the first three numbers to set the foreground (text) color and the second three numbers to set the background (fill) color.
- `setsymbol(menu item: symbol)` — Select menu item with no output
  The word setsymbol, followed by a message, specifies a menu item to be displayed by name without triggering any output.
- `settoggle(state: int)` — Set toggle state, cause output
  The word settoggle, followed by a one or zero, sets the umenu object to the specified state if it is in toggle mode and performs output as if the object were clicked on (the symbol toggle, followed by a zero or one, indicating the toggle state). Without an argument, the message simply toggles the object's state and triggers output.
- `showchecked` — Show checked menu item
  This message operates as follows. If the currently displayed item is checked, do nothing. Otherwise, starting at the first item in the menu, find one that is checked and set the menu to display that item. If there isn't one, do nothing.
- `symbol(item: symbol)` — Select a menu item by name
  Identical to the set message with a symbol argument, except that the found item number is sent out (and the text of the item is sent out the right outlet, if the Evaluate Item Text feature is enabled).
- `toggle(state: int)` — Set toggle state, cause output
  The word toggle, followed by a one or zero, sets the umenu object to the specified state if it is in toggle mode and performs output as if the object were clicked on (the symbol toggle, followed by a zero or one, indicating the toggle state). Without an argument, the message simply toggles the object's state and triggers output.

## GUI behaviors

- `(drag)` — Load the menu from a folder
  When a file folder is dragged from the Max File Browser to a umenu object, the folder's contents will be loaded into as menu choices.
  When a file folder is dragged from the Max File Browser to a blank space in an unlocked patcher window, a umenu object containing the folder's contents loaded will be created.
- `(mouse)` — Select a menu item
  Clicking with the mouse lets you select a menu item to be sent out, and causes umenu to display that item.

## Attributes

- `@introduced` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `autopopulate` — seen as: `autopopulate 1`
- `depth` — seen as: `depth $1`
- `prefix` — seen as: `prefix C74:/media/msp/`, `prefix french, prefix_mode 1`, `prefix per, prefix_mode 0`

## Help patcher examples

### appearance

> Use the format palette to create gradient panels

```
Example — [umenu]
  fan-in:
    in0 ← [attrui @style]    # Create and apply styles using the format palette or type a style name here
    in0 ← [attrui @color]    # Toggle mode attributes
    in0 ← [attrui @bgfillcolor]
    in0 ← [attrui @elementcolor]
    in0 ← [attrui @textjustification]
    in0 ← [attrui @fontface]
    in0 ← [attrui @fontsize]
    in0 ← [attrui @fontname]    # Text attributes
    in0 ← [attrui @textcolor]
    in0 ← [p menumode] ← [toggle]    # Click to see the color (toggle on) and elementcolor (toggle off) attributes / p menumode emits: "menumode 3, toggle 1" | "toggle 0, menumode 0"
```

Attributes demonstrated: `@bgfillcolor`, `@color`, `@elementcolor`, `@fontface`, `@fontname`, `@fontsize`, `@style`, `@textcolor`, `@textjustification`

### more about menus

> Menus have two additional modes which you set in the Inspector - "Label" and "Scrolling." Label mode is useful when you want window text to change under program control. Scrolling mode can be used for gigantic lists, or things that contain text you want to change as Numericals.

> set normal (menu) mode

```
Example #1 — [umenu]
  fan-in:
    in0 ← [counter 0 7] ← [button]    # Click to see next verse of the tune / Label Mode Example:
```

```
Example #2 — [umenu]
  fan-in:
    in0 ← [message "mode 3"]    # set label mode / set scrolling mode
    in0 ← [message "mode 1"]    # Scrolling Mode Example:
    in0 ← [message "mode 2"]
  fan-out:
    out0 → [number]:in0
```

### enable/disable

```
Example — [umenu]
  fan-in:
    in0 ← [message "append (i love you)"]    # Items in an umenu can be disabled in one of two ways. To disable an item permanently, it needs to be surrounded by parentheses.
    in0 ← [message "append <(not disabled)>"]    # Disabled items and their evaluated contents are output from umenu's rightmost outlet if the mouse if released over them. The item text is preceded by the symbol 'disabled_eval', and the item number by the symbol 'disabled_item'.
    in0 ← [message "append <separator>"]    # And, to add a separator to an umenu, use the symbol <separator>. You can also add an item wrapped in parentheses by surrounding it with <>.
    in0 ← [message "enableitem 3 $1"]    # To temporarily enable or disable a menu item (it's state will not be saved with the patch), use the 'enableitem' message. 'enableitem <item #> 1/0'.
  fan-out:
    out0 → [print item @popup 1]:in0
    out1 → [print eval @popup 1]:in0
    out2 → [print dumpout @popup 1]:in0
```

### autopopulate

> Because the autopulate attribute is set to 1, the prefix message will cause the menu to re-populate its items list (sound files).

```
Example — [umenu]
  fan-in:
    in0 ← [message "prefix C74:/media/msp/"]
    in0 ← [message "clear"]
  fan-out:
    out1 → [prepend open]:in0
```

### checkitem

> The checked/unchecked state of an item is not saved in a patcher

> The setcheck message lets you choose the character used for the check style. The argument is the decimal representation of the UTF-8 character:

```
Example #1 — [umenu]  A menu where only one item is checked at a time
  fan-in:
    in0 ← [message "clearchecks, checkitem $1 1"]
  fan-out:
    out0 → [message "clearchecks, checkitem $1 1"]:in0
```

```
Example #2 — [umenu]
  fan-in:
    in0 ← [message "setcheck 8226"]    # Bullet
    in0 ← [message "setcheck 62"]    # Greater Than
    in0 ← [message "setcheck"]    # A menu where all items are toggled on and off / default square-root checkbox
    in0 ← [message "checkitem $1 -1"]
  fan-out:
    out0 → [message "checkitem $1 -1"]:in0
```

### prefix settings

> The umenu object accepts the following additional commands:
>
> 'prefix' accepts a single argument, which can be concatenated or prepended to the menu item text before output.
>
> 'populate': if 'prefix' happens to be a valid folder path, the populate message will cause the umenu to clear, and fill itself with the contents of the folder referred to. If successful, umenu will report the number of items added via its right outlet, preceded by the symbol 'populate'
>
> 'autopopulate': if autopopulate is set to on, entering a new prefix will automatically populate the umenu (if the prefix is a valid path).
>
> 'types': up to 64 file types may be entered, to be used as filters to the populate message. By default, no file types are filtered. Common file types are TEXT (text file), maxb (max binary), maxt (max text) and MooV (QT movie)
>
> 'prefix_mode': determines how the prefix will be used; 0 = concatenate; 1 = prepend; 2 = ignore (useful if you want to populate an umenu, but don't want the prefix on output).
>
> 'depth' sets a depth for iterating through nested folders. 0 (no recursive iteration) is the default.
>
> prefix, prefix_mode, types, autopopulate flag and depth are all saved for each umenu

```
Example #1 — [umenu]
  fan-in:
    in0 ← [message "autopopulate 1"]
    in0 ← [message "depth $1"]
    in0 ← [prepend prefix] ← [bpatcher]    # drag and drop a folder here
  fan-out:
    out1 → [print populated]:in0
    out2 → [route populate]:in0
```

```
Example #2 — [umenu]
  fan-in:
    in0 ← [message "prefix per, prefix_mode 0"]
  fan-out:
    out1 → [print concat_mode @popup 1]:in0
```

```
Example #3 — [umenu]
  fan-in:
    in0 ← [message "prefix french, prefix_mode 1"]
  fan-out:
    out1 → [print prepend_mode @popup 1]:in0
```

### toggle

> In toggle mode, umenu can behave like a toggle button, sending 'toggle 1' or 'toggle 0' from its right outlet, to indicate the toggle state. To change the selection, hold the Control key down while clicking on umenu (with the patch locked).

> the 'toggle' and 'settoggle' messages will adjust the umenu's toggle state without using the mouse.

```
Example — [umenu]
  fan-in:
    in0 ← [message "settoggle $1"]
    in0 ← [message "toggle $1"]
  fan-out:
    out0 → [print tog @popup 1]:in0
    out1 → [print tog @popup 1]:in0
    out2 → [route toggle]:in0
```

### basic

```
Example #1 — [umenu]
  fan-in:
    in0 ← [message "insert 2 How about this?"]    # insert an item
    in0 ← [message "dump"]    # dump contents of umenu out rightmost outlet
    in0 ← [message "clear"]    # get rid of all items
    in0 ← [message "setitem 0 Holy Cow"]    # set text of existing item (does not append if the item doesn't exist)
    in0 ← [message "delete 0"]    # delete an item from the menu
    in0 ← [message "append This, append That, append The Other Thing"]    # add items to the end / Menus can be built and modified with messages:
  fan-out:
    out1 → [message ""]:in1    # umenu's middle outlet outputs the text in the selected entry
    out2 → [print dump @popup 1]:in0
```

```
Example #2 — [umenu]
  fan-in:
    in0 ← [message "set 0"]
    in0 ← [message "setsymbol Cuatro"]
    in0 ← [message "symbol Cuatro"]
    in0 ← [message "set Tres"]
    in0 ← [number] ← [dial]    # Move the dial / The menu can be used to display text associated with incoming numbers. Items start at 0.
  fan-out:
    out0 → [number]:in0
```

## See also

`coll`, `fontlist`
