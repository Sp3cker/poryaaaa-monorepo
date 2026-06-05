# menubar

_max · System_

> Put up a custom menu bar

The menubar object provides control over the menu bar. It allows your patch to put up its own menus, and add items to standard File and Edit menus. When a menu item is chosen, the item number is sent out the outlet corresponding to the menu containing the item.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | 1 Enables Menus, 0 Disables Menus |
| out0 | Application Menu Item Index Chosen |
| out1 | File Menu Item Index Chosen |
| out2 | Edit Menu Item Index Chosen |
| out3 | Window Menu Item Index Chosen |

## Arguments

- **display and behavior** (`int`) _(optional)_ — Menu count
  The argument sets the number of menus in the object's menu bar. If present, it must be at least 5 (one additional menu). The four default menus, which are always present, are File, Edit, Windows., and Help. On Macintosh, the Standard System Menu with the Apple icon and the Max application menu will appear to the left of the other menus.

## Messages

- `int(show-menus-flag (0 or non-zero): int)` — A non-zero number displays the menubar object's menus, 0 restores the previous contents of the menu bar (either the Max menus or the menus of another menubar object).
- `about(text of first menu item: list)` — This is a script message (see above) used to modify standard menus, and is the first item to appear in a script. It takes the following arguments:

 • Text of the first menu item (i.e. About My Program…).

 On the Macintosh the About item appears as the first item in the application menu (Max menu). On Windows, it appears as the first item in the Help menu. The message apple may be used optionally for compatibility with older Macintosh versions of Max.
- `append(menu number, item number, text: list)` — Add an item to a menu temporarily.
  The append message adds an item to a menu previously defined in the menubar object's script with the menutitle message. The arguments are as follows:
  • Menu number
  • Item number
  • Text of item
  • metacharacters (optional, see item message for details)
  If the menus for the object are currently showing, the item will be usable in the menu immediately. If it is not showing, the item will be included in the specified menu when the menus are showing next. After returing to the regular Max menus, any items added to the menubar object's menus with the append message will not be shown again (unless you send another append message), and are not saved in the object's script.
- `appendpermanent(menu number, item number, text: list)` — Add an item to a menu.
  The appendpermanent message adds an item to a menu previously defined in the menubar object's script with the menutitle message. The arguments are as follows:
  • Menu number
  • Item number
  • Text of item
  • metacharacters (optional, see item message for details)
  If the menus for the object are currently showing, the item will be usable in the menu immediately. If it is not showing, the item will be included in the specified menu when the menus are showing next. Items will be shown for the lifetime of the menubar object are added to the object's script.
- `apple(text of first menu item: list)` — This is a script message (see above) used to modify standard menus, and, when used, is the first item to appear in a script. It takes the following arguments:

 • Text of the first menu item (i.e. About My Program…).

 On the Macintosh the About item appears as the first item in the application menu (Max menu). On Windows, it appears as the first item in the Help menu. This message may be used optionally for compatibility with older Macintosh versions of Max.
- `checkitem(checkitem: list)` — Followed by a menu number, an item number, and a code 0 or 1, checkitem puts a check before the specified item if the code is 1, otherwise it removes the check.
- `closeitem` — This is a script message (see above) used to modify standard menus. The closeitem message takes an optional argument of 0 or 1. If "closeitem", "closeitem 1", or no message at all is entered in the script, the Close item will appear in the File menu for closing the active window. If "closeitem 0" is entered in the script, the Close item will be disabled in the File menu.
- `edit(item number, text: list)` — This is a script message (see above) used to modify standard menus. It takes the following arguments:

 • Item number to output

 • Text of item to add to edit menu

 The edit message inserts items into the standard Edit menu after the Clear item and before the Overdrive and Resume items (which are moved into the Edit menu when menubar is activated). A blank line separates the custom inserted items from the default items. Each item has a number associated with it which is sent out the third outlet of menubar when the item is chosen. The order in which your additional items appear in the Edit menu is determined by their order in the script, not by the (arbitrary) number associated with each item.
- `enableitem(menu-number, item-number, and enable/disable-flag (0 or 1): list)` — Followed by a menu number, an item number, and a code 0 or 1, enableitem enables the specified item if the code is 1, otherwise it disables (and grays out) the item.
- `end` — This is a script message (see above) used to complete a script definition. This builds the menus and reports any errors encountered.
- `file(item number, text: list)` — This is a script message (see above) used to modify standard menus. It takes the following arguments:

 • Item number to output

 • Text of item to add to file menu

 The file message inserts items at the top of the standard File menu (before the Midi Setup... menu item). Each item has a number associated with it which is sent out the when the item is chosen. The order in which your additional items appear in the File menu is determined by their order in the script, not by the (arbitrary) number associated with each item.
- `item(menu number, item number, text: list)` — This is a script message (see above) used to create new menus and menu items. It takes the following arguments:

 • Menu number

 • Item number

 • Text of item

 • metacharacters (optional)

 The item message adds an item to an additional menu previously defined with a menutitle message. The order in which your items appear in the menu is determined by their order in the script, not by the (arbitrary) number associated with each item. The item number argument only specifies the number which is sent out the menubar object’s outlet when the user chooses this item. It’s a good idea to start your item numbers at 1 and list the items in the order you want them to appear in a menu.

 Two characters can be used to modify the appearance of a menu item:

 /: followed by a character, assigns that character as a Command-key equivalent

 (: disables the menu item

 These special characters cannot appear as part of the actual item text. For example, the text On/Off will appear as Onff_O, not as On/Off.
- `markitem(menu-number, item-number, and ASCII-code: list)` — This message is no longer supported.
- `menutitle(item number, name of menu: list)` — This is a script message (see above) used to create new menus and menu items. It takes the following arguments:

 • Menu number (must be at least 5 and must not exceed the number of outlets specified in the argument to the menubar object.

 • Name of menu

 The menutitle message adds a new menu before the Window menu. The first additional menu is number 5. The menu number determines both the order of the additional menu in the menu bar and the outlet it uses when the user chooses its items. A menutitle message must appear in the script before any item messages that refer to its menu number.
- `newitem(menu item number: int)` — This is a script message (see above) used to modify standard menus. It takes the following arguments:

 • Item number to output.

 The newitem message followed by a non-zero number directs Max to send the specified number out the menubar object’s File menu outlet when the user chooses the New command from the File menu, instead of opening a new patcher window. The message newitem 0 (or the absence of any newitem message) causes the New command to behave normally.
- `open(menu item number: int)` — This is a script message (see above) used to modify standard menus. It takes the following arguments:

 • Item number to output.

 The open message followed by a non-zero number directs Max to send the specified number out the menubar object’s File menu outlet when the user chooses the Open... command from the File menu, instead of displaying the Open Document dialog box. The message open 0 (or the absence of any open message) causes the Open... command to behave normally.
- `saveas(menu item number: int)` — This is a script message (see above) used to modify standard menus. It takes the following arguments:

 • Item number to output.

 The saveas message followed by a non-zero number directs Max to send the specified number out the menubar object’s File menu outlet when the user chooses Save or Save As... from the File menu, instead of performing the standard Save actions. The number sent out the outlet when Save is chosen will be 1 less than the number sent when Save As... is chosen. The message saveas 0 (or the absence of any saveas message) causes the Save and Save As... commands to behave normally.

## GUI behaviors

- `(mouse)` — Double-clicking on the menubar object in a locked patcher will open a text editor window where you can insert a script to configure the menubar's appearance and behavior.

## Help patcher examples

### appending

```
Example — [menubar 5]
  fan-in:
    in0 ← [toggle]    # Turn on to enable menus (observe the menu bar), turn off to disable
    in0 ← [message "append 5 2 Show Me Once"]    # This item will only be shown until the menus are turned off (if the message is received while the menus are off, it will be shown the first time the menus are turned on only). And it will not be saved in the script.
    in0 ← [message "appendpermanent 5 3 Show Me Always"]    # This item will added to the menu each time the menus are shown and saved in the script.
```

### basic

```
Example — [menubar 5]  double-click to edit menu specification
  fan-in:
    in0 ← [message "checkitem 5 1 $1"]
    in0 ← [toggle]    # Turn on to enable patchers menus (observe the menu bar), turn off to disable
    in0 ← [message "enableitem 5 1 $1"]
  fan-out:
    out0 → [number]:in0    # 1 if the About... item is chosen
    out1 → [number]:in0    # File menu items (except Close, Midi Setup…, Max Menus, and Quit)
    out2 → [number]:in0    # Any items added to Edit menu
    out3 → [number]:in0    # Window menu items
    out4 → [number]:in0    # User-defined menu items (outlets for each defined menu)
```

## See also

`umenu`, `Menus (Fundamentals)`
