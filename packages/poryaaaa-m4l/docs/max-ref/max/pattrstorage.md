# pattrstorage

_max · Data_

> Save and recall pattr presets

View and modify client object data, and store or recall presets.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | int, float, messages |
| out0 | dumpout |

## Arguments

- **name** (`symbol`) _(optional)_ — Scripting name
  A symbol argument may be optionally used to set the pattrstorage object's scripting name. In the absence of an argument, the pattrstorage object is given an arbitrary, semi-random name, such as u197000004.

## Messages

- `int(index: int)` — Recall a preset
  Recalls the data from the preset specified by int.
- `float(index: float)` — Interpolate between presets
  Recalls the data from the preset specified by float. If the number falls between two whole numbers (e.g. 1.5), the pattrstorage object will interpolate between the data stored in the preset corresponding to the integer portion of the float and the data stored at the preset numbered one higher (e.g. 1.5 will cause pattrstorage to interpolate 50% between presets 1 and 2). See the interp message for more information about interpolation modes.
- `anything(arguments: list)` — Direct pattr access
  Incoming messages to the pattrstorage object are analyzed. If the first element of the message matches the path name or alias of an object maintained by the pattrstorage object (visible in the object's client list), the subsequent arguments in the message set that object's value. Otherwise, the message is ignored.
- `active(client: list)` — Set client active status
  The word active, followed by a symbol that specifies the path name or alias of a client object and a 1 or 0, sets that object's active status. When a client object is active (default), its data will be recalled when presets are recalled--otherwise, the object is ignored during recall. Setting the active state of a parent object (such as a patcher-any client object containing other client objects), automatically sets the active state of the child objects of the parent to the same value.
- `alias([client: symbol], [alias: symbol])` — Create an alias for a client
  The word alias, followed by two symbols, generates an alias for the client object whose path name is given in the first argument. The alias permits the object to be referred to by a name given in the second argument.
  For example, alias a_patcher::a_pattr the_pattr would alias the object at the location a_patcher::a_pattr to the name the_pattr.
  Aliases can be used interchangeably with path names within the pattrstorage object, and are useful for referring to long paths by simpler, shorter names.
  get alias The word get alias, followed by a symbol that specifies the path name of a client object, returns that object's alias (if any) from the pattrstorage object's outlet, preceded by the symbol alias.
- `clear` — Delete all presets
  The word clear removes all presets from the pattrstorage object's internal list.
- `client_close` — Close the client list window
  Closes the client list window.
- `clientwindow(name: list)` — Open a pattrstorage client window
  Opens the pattrstorage object's client list window (the title bar reads clientwindow (name), where (name) is the patcher name of the pattrstorage object which created the window).
- `copy([pattrstorage: symbol], from-index: int, t0-index: int)` — Copy a preset
  The word copy, followed by 2 or 3 arguments, copies the stored values from one numbered preset to another. Followed by 2 numbers, the stored values from the preset slot specified by the first number will be copied to the preset slot specified by the second number. If that slot doesn't yet exist, it will be automatically created. Followed by a symbol and 2 numbers, the stored values from a preset slot, as specified by the first number, of the pattrstorage object referred to by the symbol will be copied to a preset slot, as specified by the second number, of the object receiving the copy message. For example, the message copy parent::psto_parent 3 1 would cause preset 3 of the pattrstorage object called psto_parent, located in the parent patch of the pattrstorage object receiving the copy message, to be copied to preset 1 of the pattrstorage object receiving the message. In order for this to function reliably, client path names must match exactly. If they do not, the data for that client is ignored.
- `delete([index: list])` — Delete one or all presets
  The word delete, followed by a number, clears any data in the preset whose index is specified by that number and removes the preset from the pattrstorage object's internal list. If delete is not followed by an argument, all presets are cleared and removed. See the getslotlist message for further information on viewing the object's list of presets.
- `dump` — Output all current values
  The word dump reports the current value of all client objects from the pattrstorage object's outlet as a series of messages, each in the form [object pathname] [data ...]. The output of dump is finished when the message dump done is output.
- `fade(list: list)` — Recall preset data
  Identical to the recall message. Deprecated.
- `fillempty` — Fill all empty presets
  The word fillempty sets the value of any empty preset slots for all client objects to the current value of the respective object. For client pattr objects with an initial value, the initial value will be used instead. No data is sent to the client objects themselves.
- `getactive(client: list)` — Report client active state
  The word getactive, followed by a symbol that specifies the path name or alias of a client object, reports the active status of the client object from the pattrstorage object's outlet, preceded by the symbol active.
- `getalias(client: list)` — Report a client's alias
  The word getalias, followed by a symbol that specifies the path name or alias of a client object, will cause pattrstorage to return that client-object's alias from its outlet.
- `getclientlist` — Output a list of pattr clients
  The word getclientlist reports the path names of any client objects from the pattrstorage object's outlet as a series of messages, each preceded by the symbol clientlist. The output of getclientlist is finished when the message clientlist done is output.
- `getcurrent` — Report the current preset
  The word getcurrent reports the currently active preset from the pattrstorage object's outlet, preceded by the symbol current.
- `getedited` — Report the preset edit state
  The word getedited reports the edit state of the currently active preset from the pattrstorage object's outlet, preceded by the symbol edited. If the data in the currently active preset has been edited, the state is reported as 1. Otherwise, the edit state is reported as 0.
- `getinterp(client: list)` — Report client interpolation mode
  The word getinterp, followed by a symbol that specifies the path name or alias of a client object, reports that object's interpolation mode from the pattrstorage object's outlet, preceded by the symbol interp.
- `getlockedslots` — Output a list of locked presets
  The word getlockedslots reports the indices of any locked slots from the pattrstorage object's outlet as a list, preceded by the symbol lockedslots.
- `getpriority(client: list)` — Report client priority
  The word getpriority, followed by a symbol that specifies the path name or alias of a client object, reports that object's priority from the pattrstorage object's outlet, preceded by the symbol priority.
- `getslotlist` — Output a list of available presets
  The word getslotlist reports the numbers of any valid presets from the pattrstorage object's outlet, preceded by the symbol slotlist.
- `getslotname(index: int)` — Retrieve a preset name
  The word getslotname, followed by a number, causes the name of the preset slot specified by the number to be output from the pattrstorage object's outlet, preceded by the symbol slotname.
- `getslotnamelist([range: int])` — Output a list of preset names
  The word getslotnamelist, followed by an optional number, reports the slot names of all used slots to be sent from the pattrstorage object's outlet as a series of messages, each preceded by the symbol slotlist. The output of getslotlist is finished when the message slotname done is output.
  Without an argument, or with an argument of 0, the getslotnamelist message will cause all slots from 0 to the largest stored slot number to be output, regardless of whether the slot has been defined or not. The facilitates the use of the getslotlist message with objects such as umenu. To filter undefined slots (even if they have names), send the getslotlist message with a non-0 argument.
  For more information, see the pattrstorage object's help file.
- `getstoredvalue(client: list)` — Retrieve a client value
  The word getstoredvalue, followed by a symbol that specifies the path name or alias of a client object and a number which specifies a preset, reports that object's value, as stored in that preset slot, from the pattrstorage object's outlet, in the form [object pathname or alias] [data ...].
- `getsubscriptionlist` — Output a list of subscribed items
  The word getsubscriptionlist causes the names of all subscribed objects to be sent from the pattrstorage object's outlet as a series of messages, each preceded by the symbol subscriptionlist. The output of getsubscriptionlist is finished when the message subscriptionlist done is output. See the subscribemode attribute for more information.
- `grab` — Access current pattr values
  The word grab causes the current value of all client objects to be reacquired by the pattrstorage object. This is particularly useful when the pattrstorage object is managing client objects whose data changes internally, without sending notifications to the pattr system.
- `insert(index: int)` — Insert a new preset
  The word insert, followed by a number, stores the data for every object maintained by pattrstorage in a numbered preset. The number argument specifies the index of the preset to be stored. Any presets numbers at the specified index or higher are automatically incremented to make room for the inserted preset.
- `interp(arguments: list)` — Set client interpolation mode
  The word interp, followed by at least 1 and up to 3 arguments (symbol, symbol, float/symbol), sets the interpolation status and mode for a specific client object. The first symbol specifies the path name or alias of a client object. The second symbol argument determines the mode, and can be one of the following values:
  off- No interpolation. Same as no additional argument.
  linear- Linear interpolation. Presets recalled using float or fade messages will be interpolated using a standard linear algorithm.
  thresh- Threshold. Takes optional 3rd argument, which sets the threshold. Presets recalled using float or fade messages will recall data from the first preset specified when the fade amount is below the threshold, and will recall data from the second preset specified when the fade amount is greater than or equal to the threshold.
  ithresh- Inverse threshold. Takes optional 3rd argument (float), which sets the threshold. Presets recalled using float or fade messages will recall data from the first preset specified when the fade amount is greater than or equal to the threshold, and will recall data from the second preset specified when the fade amount is less than the threshold.
  pow- Power curve. Takes optional 3rd argument (float), which sets the exponent to which the fade amount will be raised. Presets recalled using float or fade messages will recall data between the two specified presets, along the curve described. Power curves can be used to create faster or slower "attacks" and "decays" for the fade envelope.
  table- Table-specified curve. Takes optional 3rd argument (symbol), which specifies the name of a table to use for curve lookup. Presets recalled using float or fade messages will recall data between the two specified presets, along the curve described in the table. Tables are assumed to contain values between 0 and 100, representing the new fade amount * 100 (this is clipped internal to the pattrstorage object, but is not normalized). The length of the table is stretched to match the expected fade values (between 0 and 1), so any number of table entries can be used. If the lookup fade amount does not fall exactly onto a table-specified value, linear interpolation is used to determine the new fade amount. Please see the pattrstorage help file for examples of table-specified interpolation.
- `locate(client: symbol)` — Open the patcher containing an object
  The word locate, followed by a symbol corresponding to the path name or alias of an object, will cause the containing patcher of that object to be opened.
- `lock(index: int, status: int)` — Set the lock status of a preset
  The word lock, followed by 2 numbers, sets the lock status for a particular preset number. The first argument specifies the preset number to be locked or unlocked. The second argument specifies the lock state, and should be either 0 (unlocked) or 1 (locked). Locked presets cannot be deleted (using the delete or remove messages) or overwritten (using the store message). Locked presets can be moved (as a result of insert, remove or renumber messages, if performed on other presets). Locks are saved in the preset data file.
- `lockall(status: int)` — Lock all presets
  The word lockall, followed by a number, sets the lock status for all presets at once. The argument specifies the lock state, and should be either 0 (unlocked) or 1 (locked). Locked presets cannot be deleted (using the delete or remove messages) or overwritten (using the store message). Locked presets can be moved (as a result of insert, remove or renumber messages, if performed on other, unlocked presets). Locks are saved in the preset data file.
- `priority(client: list)` — Set client priority
  The word priority, followed by a symbol that specifies the path name or alias of a client object and a number, sets the recall and display priority for that object. When presets are recalled, the data for client objects will be restored in the order established by priority. Lower priorities are executed first. Negative priorities are permitted. Priority is only respected within a single level of the patcher hierarchy. Data in parent patchers will always be restored before data in nested patchers.
- `purge` — Rebuild the internal client list
  The word purge rebuilds the internal client list of the pattrstorage object, removing entries for client objects which have been deleted or moved. Typically, the pattrstorage object retains a reference to such objects, so that their settings (priority, active, interp, etc.) can be restored if the objects reappear.
- `read([filename: list])` — Read preset data from disk
  The word read, followed by an optional symbol that specifies a filename, reads an JSON or XML file representing preset data from disk into the pattrstorage object. If the argument is given, and represents a valid file path, the file will be read from that location--otherwise, a standard File Dialog will be presented for the user to manually choose the file to be read.
- `readagain([filename: list])` — Reload preset data from disk
  The word readagain re-reads an JSON or XML file previously specified by the read or write messages. If no file has been previously specified, a standard File Dialog will be presented for the user to manually choose the file to be read.
- `recall(client-name, preset-indices, and interpolation-value: list)` — Recall preset data
  The word recall, followed by 1 to 4 arguments, recalls data from a preset. If recall is followed by a number or a floating-point number, the data for every object whose value is stored in the specified preset (or in the interpolated preset represented by a floating-point number - see the float message) will be recalled. If recall is followed by 2 arguments--a symbol and a number--and the symbol argument matches the path name or alias of a client object, only the data for the specified object will be recalled. The number argument always specifies the index (or interpolated index) of the preset to recall.
  Followed by 3 or 4 arguments, the recall message recalls interpolated data from 2 presets at a specified weight between the two. If the word recall is followed by two numbers that specify the indices of two presets and a a floating point number between 0 and 1.0 that specifies an interpolation value, the data for every object whose value is stored in the specified presets will be recalled.
  If recall is followed by a symbol that specifies the path name or alias of a client object followed by two numbers that specify the indices of two presets and a floating point interpolation value, only the data for the specified object will be recalled.
  In these latter cases, the floating point argument specifies the weight of the interpolation, and should be between 0. and 1. A floating point argument of 0. would simply recall the data for the preset matching the first index, and 1. would recall the data for the preset matching the second index. See the interp message for more information about interpolation modes.
- `recallmulti(weighted-pairs: list)` — The word recallmulti, followed by at least 2 numeric arguments, permits weighted recall of multiple presets.
  The word recallmulti, followed by at least 2 numeric arguments, permits weighted recall of multiple presets. Each argument determines the (normalized) weight of a particular preset in the final output. If the argument is an integer, the weight is 100%. If the argument is a floating point number, the integer part of the number determines the preset number, and the floating point part of the number determines the weight. For instance, recallmulti 1.3 2.3 5.4 would weight preset 1 at 30%, preset 2 at 30% and preset 5 at 40%. Since weights are normalized, the total weight can be higher than 100%; for instance, recallmulti 1.5 3.5 6.8 would calculate correct weights (27.77%, 27.77% and 44.44% respectively), and recallmulti 1 2 3, recallmulti 1.5 2.5 3.5 and recallmulti 1.99 2.99 3.99 all result in the same output (33.33% for each member).
- `remove(index: int)` — Delete a preset and renumber
  The word remove, followed by a number, deletes the data for every object maintained by pattrstorage in a numbered preset. The number argument specifies the index of the preset to be removed. Any presets numbers higher than the specified index are automatically decremented.
- `renumber` — Renumber presets consecutively
  The word renumber renumbers stored presets into consecutive preset slots, beginning with slot 1.
- `resolvealias(client: list)` — Resolve a client alias
  The word resolvealias, followed by a symbol that specifies the alias of a client object, returns that object's full path name (if any) from the pattrstorage object's outlet, preceded by the symbol resolvealias.
- `setall(client: list)` — Set a value for all presets
  The word setall, followed by a symbol that specifies the path name or alias of a client object and a variable number of additional arguments corresponding to the value of the client object, sets the value of the specified client object for all preset slots to that value. No data is sent to the client object itself.
- `setstoredvalue(client: list)` — Directly set client values
  The word setstoredvalue, followed by a symbol that specifies the path name or alias of a client object, a number which specifies a preset and a variable number of additional arguments corresponding to the data expected by the client object, sets the value of the specified client object within the specified preset slot to the specified data.
- `slotname(index: int, [slotname: symbol])` — Provide a name for a preset
  The word slotname, followed by a number and an optional symbol, sets the name of the preset slot specified by the number. If the symbol argument is not present, the name of the slot is removed. Undefined slots can be given names for labeling purposes. If a preset object is linked to the pattrstorage object, it will display the slot name of the corresponding pattrstorage slot.
- `storage_close` — Close the stored data window.
  Closes the stored data window.
- `storagewindow(name: list)` — Open a pattrstorage data window
  Opens the pattrstorage object's stored data window (the title bar reads storagewindow (name), where (name) is the patcher name of the pattrstorage object which created the window).
- `store(client-and-index: list)` — Store a preset
  The word store, followed by 1 or 2 arguments, stores data in a numbered preset. If the word store is followed by a number, the data for every object maintained by pattrstorage will be stored. If store is followed by 2 arguments--a symbol and a number--and the symbol argument matches the path name or alias of a client object, only the data for the specified object will be stored. The number argument always specifies the index of the preset to be stored. If the preset index specified by the number argument is already in use, the existing data will be overwritten without a warning.
  A storage slot of '0' is allowed, but *IS NOT SAVED* in a file. It can be used as a temporary slot for interpolation activities or other non-permanent experiments.
- `storeagain(client: list)` — Store a preset in the last-used slot
  The word storeagain simply executes a store operation, using the most recently-use preset slot. If there is no previously-used preset slot (if the store message has never been sent to the object), the message is ignored.
- `storenext(client: list)` — Store a present in an empty slot
  The word storenext executes a store operation, using the next empty preset slot, counting up from preset 1. For instance, if preset slots 1, 2 and 4 have data stored in them, and the pattrstorage object receives the storenext message, the current state of the client objects would be stored to preset slot 3. A second storenext message would cause the data to be stored to preset slot 5.
- `subscribe(target: list)` — Add clients to the subscription list
  The word subscribe, followed by one or more symbols, each corresponding to the path name or alias of an object, will add the specified object to the pattrstorage object's subscription list. See the subscribemode attribute for more information.
- `unsubscribe(target: list)` — Remove clients from the subscription list
  The word unsubscribe, followed by one or more symbols, each corresponding to the path name or alias of an object, will remove the specified object from the pattrstorage object's subscription list. See the subscribemode attribute for more information.
- `write([filename: list])` — Write preset data to disk
  The word write, followed by an optional symbol that specifies a filename, writes any preset data to a JSON file on disk. If the argument is given, and represents a valid file path, the file will be saved at that location - otherwise, a standard File Dialog will be presented for the user to manually choose a name and location for the file to be saved.
- `writeagain([filename: list])` — Rewrite preset data to disk
  The word writeagain writes any preset data to a JSON file on disk previously specified by the read or write messages. If no file has been previously specified, a standard File Dialog will be presented for the user to manually choose a name and location for the file to be saved.
- `writejson([filename: symbol])` — Write presets as a JSON file
  The word writejson, followed by an optional symbol that specifies a filename, writes any preset data to a file on disk, in JSON format. If the argument is given, and represents a valid file path, the file will be saved at that location--otherwise, a standard File Dialog will be presented for the user to manually choose a name and location for the file to be saved.
- `writexml([filename: symbol])` — Write presets as an XML file
  The word writexml, followed by an optional symbol that specifies a filename, writes any preset data to a file on disk, in XML format. If the argument is given, and represents a valid file path, the file will be saved at that location--otherwise, a standard File Dialog will be presented for the user to manually choose a name and location for the file to be saved.

## GUI behaviors

- `(mouse)` — Open the client window
  Double-clicking on a pattrstorage object opens the object's client list window, as if the object had received the clientwindow message.

## Attributes

- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `changemode` — seen as: `changemode $1`
- `client_rect` — seen as: `client_rect $1 $1 $2 $2`
- `flat` — seen as: `flat $1`
- `greedy` — seen as: `greedy $1`
- `savemode` — seen as: `savemode $1`
- `storage_rect` — seen as: `storage_rect $1 $1 $2 $2`

## Help patcher examples

### interface

> The preset object's pattrstorage attribute is set to the name of the pattrstorage it controls.
>
> Open Slot Storage window and delete all presets.
>
> Store two presets.

```
Example — [pattrstorage interface @savemode 0]
  fan-in:
    in0 ← [umenu] ← [p menu-helper]    # Recall presets using menu.
    in0 ← [message "getslotnamelist"]    # Fills a menu object with slotnames so that the menu's Item Number can be used to recall presets. / Output list of slotnames for all presets, with "undefined" slotnames for unused preset numbers, if any, between presets…
    in0 ← [message "getslotnamelist 1"]    # …or output list of slotnames for stored presets (same message, but with non-zero argument).
    in0 ← [message "slotname 1 First, slotname 3 Third"]    # Give each preset a slotname.
    in0 ← [message "store 1, store 3"]
    in0 ← [message "storagewindow, clear"]
  fan-out:
    out0 → [p menu-helper]:in0
```

### slots

```
Example — [pattrstorage slots @savemode 0]
  fan-in:
    in0 ← [message "delete 2"]    # Delete preset 2 and do not renumber.
    in0 ← [message "remove 1"]    # Delete preset 1 and decrement higher-numbered presets by 1.
    in0 ← [message "clear"]    # Delete all presets.
    in0 ← [message "store 1"]    # Store keyboard chord in preset 1
    in0 ← [message "copy 2 74"]    # Copy preset 1 to preset 74
    in0 ← [message "insert 1"]    # Move chord stored in preset 1 to preset 2, then store new chord in preset 1.
    in0 ← [message "getslotlist"]    # Output a list of all presets (slots)
    in0 ← [message "store 74"]    # Store keyboard chord in preset 74.
    in0 ← [message "storagewindow, storage_rect 766 44 1220 302, clear"]    # Open Storage Slots window with all presets cleared. / Presets can be rearranged and copied after their data is stored.
    in0 ← [message "renumber"]    # Renumber presets to be 1, 2, 3.
  fan-out:
    out0 → [message "read slots.json 0"]:in1
```

### scope

> Individual pattr objects and/or subpatchers can be stored/recalled/enabled/disabled.

```
Example — [pattrstorage scope @savemode 0]
  fan-in:
    in0 ← [message "active ui $1"]    # enable/disable preset recall for all UI-object inside patcher.
    in0 ← [message "greedy $1"]
    in0 ← [message "setstoredvalue ui::soap $1 999"]    # recall a single parameter from this preset, sending the name and value out pattrstorage's outlet. / store a single parameter to this preset, if the preset already exists.
    in0 ← [message "store ui::soap $1"]    # recall a single parameter from this preset. / store a single parameter to this preset, creating new presets if necessary.
    in0 ← [message "recall ui::soap $1"]
    in0 ← [message "active ui::soap $1"]    # enable/disable preset recall for UI-object inside patcher.
    in0 ← [message "storagewindow"]
    in0 ← [message "getstoredvalue ui::soap $1"]
  fan-out:
    out0 → [message "read scope.json 0"]:in1
```

### save

> If pattrstorage's name (its scripting name) is not explicitly given (as an argument or attribute), Max will generate a unique name that looks something like u000000000. Look in the Inspector, at Scripting Name.

```
Example — [pattrstorage storage @savemode 0]
  fan-in:
    in0 ← [message "savemode $1"]
    in0 ← [message "readagain"]
    in0 ← [message "write"]    # write <filename>: write pattrstorage data to disk
    in0 ← [message "read"]    # read <filename>: write pattrstorage data from disk
    in0 ← [message "writeagain"]    # read and write, using the same file that was last used.
```

### interpolation

```
Example — [pattrstorage interp @savemode 0]
  fan-in:
    in0 ← [message "changemode $1"]    # Interpolated data can result in unnecessary processing. When changemode = 1, only changed data output.
    in0 ← [message "store 1"]    # Store function values in preset 1
    in0 ← [message "store 3"]    # Move the points (step 1), then store new values in preset 3.
    in0 ← [prepend interp dis_function]
    in0 ← [message "recall 1 3 $1"]
```

### priority

```
Example — [pattrstorage priority @savemode 0]
  fan-in:
    in0 ← [message "clientwindow"]    # Since 0 is less than 1, tortoise's pattr values are still recalled before hare's. / Edit priority in the client window, making tortoise priority 0.
    in0 ← [message "client_close"]    # Close the window.
    in0 ← [message "priority tortoise::hubris -10, priority hare::hubris -10,"]    # Tortoise's pattr values are still recalled before hare's, but hubris is restored first within each subpatcher. / Set hubris to be restored before speed in both hare and tortoise.
    in0 ← [message "clientwindow"]    # The tortoise patcher has priority -1, which means its pattr values are recalled before the hare's, because the hare has priority 1. / Open a Client Objects window.
```

### Max for Live

> In Max for Live, if you set the parameter_enable attribute (Parameter Mode Enable = 1) and Initial Enable of a pattrstorage object, it will visible to the Live set as a Blob parameter.

```
Example — [pattrstorage MFL] (MFL)
  fan-in:
    in0 ← [number]
    in0 ← [message "store $1"]
```

### windows

```
Example — [pattrstorage]
  fan-in:
    in0 ← [message "storage_rect $1 $1 $2 $2"]
    in0 ← [message "storagewindow"]    # Open a storage window.
    in0 ← [message "client_close"]    # Close the window.
    in0 ← [message "storage_close"]    # Close the window.
    in0 ← [message "flat $1"]    # Enable/disable patcher hierarchical display.
    in0 ← [message "client_rect $1 $1 $2 $2"]
    in0 ← [message "clientwindow"]    # Open a client window.
```

### basic

```
Example — [pattrstorage basic @savemode 0]
  fan-in:
    in0 ← [message "storagewindow"]
    in0 ← [message "store 1"]
    in0 ← [message "store 2"]
    in0 ← [message "1"]
    in0 ← [message "2"]    # Recall preset 2
    in0 ← [flonum]
```

## See also

`autopattr`, `pattrforward`, `pattrhub`, `pattrmarker`
