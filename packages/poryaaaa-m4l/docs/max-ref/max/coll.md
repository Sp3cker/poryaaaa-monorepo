# coll

_max · Data_

> Store and edit a collection of data

Allows for the storage, organization, editing, and retrieval of different messages.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Single Item Outputs Data, Multiple Items Store Data |
| out0 | Data Contents |
| out1 | Number or Symbol Associated With Data |
| out2 | bang When Finished Reading Data File |
| out3 | bang When Finished With Dump Output |

## Arguments

- **name** (`symbol`) _(optional)_ — Context and file name
  Determines the named context of the coll object. All coll objects that share the same name share their contents. When a patch containing a named coll is loaded, Max will search for a file that matches the name; if found, the file is automatically loaded.
- **no-search** (`any`) _(optional)_ — Suppress file load
  An optional nonzero value as a second argument will prevent the coll object from searching for a file with the named symbol.

## Messages

- `bang` — Retrieve the next data set
  See the next listing.
- `int(index: int)` — Retrieve data by index
  The number refers to the address of a message stored in coll. If a message is stored at that address, the stored message is output. If the stored message is a single symbol, it is always prepended with the word "symbol" when output.
- `float(index: float)` — Retrieve data by index
  The number refers to the address of a message stored in coll. If a message is stored at that address, the stored message is output. If the stored message is a single symbol, it is always prepended with the word "symbol" when output.
- `list(index: int, data: list)` — Store index and data
  The first value is used as the address (the storage location within coll) at which to store the remaining items in the list. The address will always be stored as an int.
- `anything(index: any)` — Retrieve data by symbol
  See the symbol listing.
- `append(data: list)` — Add item associated with an index
  The append message creates a new item associated with an index that is one larger than the highest current index. For example, if the coll is empty, append xyz will add an item xyz associated with the index 0. append xyz a second time will add another item xyz associated with the index 1.
- `assoc(address name: symbol, data index: int)` — Associate a name with an index
  Associates a symbol with the numeric address, provided that the number address already exists. After association, any reference to that symbol will be interpreted as a reference to the number address. Each number address can have only one symbol associated with it.
- `clear` — Clear all data
- `deassoc(address name: symbol, data index: int)` — De-associate a name with an index
  Removes the association between a symbol and the number address. The symbol will no longer have any meaning to coll.
- `delete(index: any)` — Remove data and renumber
  Removes the data at the address provided. If the specified address is numeric, all higher numbered addresses are decremented by 1.
- `dump` — Output all data
  Sends all of the stored addresses out the 2nd outlet and all of the stored messages out the 1st outlet, in the order in which they are stored. A bang is sent out the 4th outlet when the dump is completed.
- `end` — Move to last address
  Sets the pointer (as used by the goto, next, and prev messages) to the last address.
- `filetype(filetype: symbol)` — Set the recognized file types
  Sets the file types which can be read and written into the coll object. The message filetype with no arguments restores the default file behavior.
- `flags(save-setting: int, unused: int)` — Set the file-save flag
  Sets the flags used to save its contents within the patch that contains it. The message flags 1 0 notifies the object to save its contents as part of the patcher file. The message flags 0 0 causes the contents not to be saved.
- `goto(index: list)` — Move to an index
  Sets the pointer (as used by the goto, next, and prev messages) at a specific address, but does not trigger output. If the specified address does not exist, the pointer is set at the beginning of the collection. Data will be output in response to a subsequent bang, next, or prev message.
- `insert(index: int, data: list)` — Insert data at a specific address
  Inserts the message at the address specified by the number, incrementing all equal or greater addresses by 1 if necessary.
- `insert2(index: int, data: list)` — Insert data at a specific address
  See the insert listing.
- `length` — Retrieve the number of entries
  Counts the number of entries contained in the coll and sends the number out the 1st outlet.
- `max([element: int])` — Return the highest numeric value
  Gets the highest value in any entry. An optional integer argument (defaults to '1') specifies an element position to use.
- `merge(index: int, data: list)` — Merge data at an existing address
  Appends data at the end of the data found at the specified index. If the address does not yet exist, it is created.
- `min([element: int])` — Return the lowest numeric value
  Gets the lowest value in any entry. An optional integer argument (defaults to '1') specifies an element position to use.
- `next` — Move to the next address
  Sends the address and data stored at the current address, then sets the pointer to the next address. If the pointer is currently at the last address in the collection, it wraps around to the first address. If the address is a symbol rather than a number, 0 is sent out the second outlet.
- `nstore(index: int, association: symbol, data: list)` — Store data with both number and symbol index
  Stores the message at the specified number address, with the specified symbol associated. This has the same effect as storing the message at an int address, then using the assoc message to associate a symbol with that number.
- `nsub(index: int, position: int, data: any)` — Replace a single data element
  Replaces a data element with a new value. As an example, nsub 2 4 7 replaces the fourth element of address 2 with the value 7. Number values and symbols can both be substituted in this manner.
- `nth(index: int, position: int)` — Return a single data element
  Returns the data element found at a specific position in the stored list and send it out the first outlet. As an example, nth 75 2 will output the second item in the list stored at address 75.
- `open` — Open a data editing window
  Opens a data editing window for the current data and bring it into focus.
- `prev` — Move to the previous address
  Sends the address and data stored at the current address, then sets the pointer to the previous address. If the pointer is currently at the first address in the collection, it wraps around to the last address. If the address is a symbol rather than a number, 0 is sent out the second outlet.
- `read(filename: symbol)` — Choose a file to load
  With no arguments, read puts up a standard Open Document dialog box to choose a file to load. If an argument is provided, the named file is loaded.
- `readagain` — Reload a file
  Loads the contents of the most recently read file. If no prior file load has occurred, the request is treated like a read message.
- `refer(object name: symbol)` — Change data reference
  Changes the reference to the data in another named coll object. Changes to the data stored in any referenced coll will be shared by all other objects with the same name.
- `remove(index: any)` — Remove an entry
  Removes that address and its contents from the collection.
- `renumber(data index: int)` — Renumber entries
  Renumbers data entries as consecutive and in increasing order. The optional argument specifies the starting number address for the data.
- `renumber2(data index: int)` — Increment indices by one
- `separate(data index: int)` — Creates an open entry index
  Increments the numerical indices for all data whose index is greater than the provided. This creates an open 'slot' for a subsequent add.
- `sort(sort order (-1 or 1): int, entry (-1, 0, or 1): int)` — Sort the data
  Sorts the data into a specified order. If the first argument is -1, the items are sorted in ascending order. If the first argument is 1, the items are sorted in descending order.
  The second argument specifies what data is used to sort the contents. If the second argument is -1, the index (either number or symbol) associated with the data is used. If the second argument is not present or is 0, the first item in the data is used. If the second argument is 1 or greater, that data elements is used for the sorting order.
- `start` — Move to the first entry
  Sets the pointer (used by the goto, next, and prev messages) to the first address in the coll.
- `store(index: symbol, data: list)` — Store data at a symbolic index
  Stores the message at an address named by the provided symbol. As an example, store triad 0 4 7 will store 0 4 7 at an address named triad.
- `sub(index: int, position: int, data: list)` — Replace a data element, output data
  Same as nsub, except that the message stored at the specified address is sent out after the item has been substituted.
- `subsym(new name: symbol, old name: symbol)` — Changes an index symbol
  Changes the symbol associated with data. The first argument is the new symbol to use, the second argument is the symbol associator to replace.
- `swap(index: int, index: int)` — Swap two indices
  Exchanges the indices associated with two addresses. The data is unchanged, but the indexes that they use are swapped.
- `symbol(index: symbol)` — Retrieve data by symbol
  Retrieves a message stored at the address named by the symbol. If no address is associated with the symbol, no output is produced. If the stored message is a single symbol, it is always prepended with the word "symbol" when output.
- `wclose` — Close the data editing window
- `write(filename: symbol)` — Write the data to a disk file
  With no arguments, write puts up a standard Open Document dialog box to choose a filename to write. If an argument is provided, the name is used as a filename for storage.
- `writeagain` — Rewrite a file
  Saves the contents to the most recently written file. If no prior file write has occurred, the request is treated like a write message.

## GUI behaviors

- `(mouse)` — Open a data editing window
  Double-click on the coll object to display the contents as text in an editing window. The data can be manually edited within this editor.

## Attributes

- `@basic` (int)
- `@label` (symbol)
- `@save` (int)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `embed` — seen as: `embed 0`, `embed 1`

## Help patcher examples

### files & references

> A coll whose content is backed by a file can additionally be edited using Max's external text editor support.

```
Example #1 — [coll]  data
  fan-in:
    in0 ← [message "refer phoo"]    # Data Reference
    in0 ← [number]    # entry recall
    in0 ← [message "refer spoo"]
    in0 ← [message "refer bloo"]    # change the currently referenced coll
  fan-out:
    out0 → [number]:in0
```

```
Example #2 — [coll spoo 1]
  (no patch cords)
```

```
Example #3 — [coll bloo 1]
  (no patch cords)
```

```
Example #4 — [coll phoo 1]  named colls saved with the patcher.
  (no patch cords)
```

```
Example #5 — [coll helpColl]
  fan-in:
    in0 ← [message "read"]    # read from a file
    in0 ← [message "writeagain"]    # save contents over last file
    in0 ← [message "readagain"]    # revert to saved file
    in0 ← [message "write"]    # write to a file
    in0 ← [message "write test"]    # write file to where the patcher is stored
    in0 ← [message "read test"]    # read a named file in Max's search path
  fan-out:
    out2 → [button]:in0    # bangs when done reading
```

### symbols

```
Example #1 — [coll symbols]
  fan-in:
    in0 ← [message "symbol one"]    # retrieve data from newly named indexes
    in0 ← [message "symbol two"]
    in0 ← [message "store dos 2.222, subsym two dos"]
    in0 ← [message "store uno 1.111, subsym one uno"]    # Substitute associated symbol / Index substitution and association
    in0 ← [message "deassoc six 6"]    # deassociate symbol with index
    in0 ← [message "symbol six"]    # recall data using symbol
    in0 ← [message "6 foo, assoc six 6"]    # associate symbol with index
  fan-out:
    out0 → [message ""]:in1
    out1 → [message ""]:in1
```

```
Example #2 — [coll symbols]
  fan-in:
    in0 ← [message "store tres ei"]
    in0 ← [message "store uno ce"]    # store
    in0 ← [message "store dos ome"]
    in0 ← [message "symbol uno"]    # retrieve
    in0 ← [message "symbol dos"]
    in0 ← [message "symbol tres"]
  fan-out:
    out0 → [message ""]:in1
    out1 → [message ""]:in1
```

### advanced storage

```
Example #1 — [coll helpfoo 1]
  fan-in:
    in0 ← [message "renumber 4"]    # renumber from a specific value
    in0 ← [message "renumber"]    # renumber numerical indices
    in0 ← [message "swap 1 2"]    # swap data at indices
    in0 ← [message "sort 1"]    # sort entries based on first element
    in0 ← [message "sort -1 -1"]    # sort entries based on index number (ascending) / Sort and Swap
    in0 ← [message "open"]
```

```
Example #2 — [coll helpColl]
  fan-in:
    in0 ← [message "embed 1"]    # save the data with the patcher
    in0 ← [message "nsub 2 3 elephants"]
    in0 ← [message "merge 2 6.124 "more text""]    # set element at index / append data to index / insert an entry
    in0 ← [message "insert 2 345 0.99 "my text""]
    in0 ← [message "embed 0"]    # coll defaults to embed 0
    in0 ← [message "sub 2 3 rhinos"]    # set element then output
  fan-out:
    out0 → [print]:in0
    out1 → [message ""]:in1    # index number or symbol
```

### queries

```
Example #1 — [coll helpfoo 1]
  fan-in:
    in0 ← [message "min"]    # get lowest value of any slot/entry
    in0 ← [message "length"]    # get number of entries (left outlet) / Dataset Queries
    in0 ← [message "min 1"]    # get the lowest value in the first slot of any entry
    in0 ← [message "max 1"]    # get the highest
    in0 ← [message "max"]    # get the highest
  fan-out:
    out0 → [number]:in0    # data
```

```
Example #2 — [coll helpfoo 1]
  fan-in:
    in0 ← [message "prev"]    # output previous entry
    in0 ← [message "end"]    # goto end - no output
    in0 ← [message "next"]    # output next entry
    in0 ← [message "goto 1"]    # goto an entry - no output
    in0 ← [button]    # bang to output current entry / Stepped Movement
  fan-out:
    out0 → [number]:in0    # data
    out1 → [number]:in0    # index
```

### basic

```
Example #1 — [coll helpColl]  coll objects with the same name share data
  fan-in:
    in0 ← [message "delete 1"]    # delete entry and decrement higher indices / Removal
    in0 ← [message "remove 2"]    # delete entry with no renumbering
    in0 ← [message "clear"]    # erase everything
```

```
Example #2 — [coll helpColl]
  fan-in:
    in0 ← [number]    # int to retrive an index and its data
    in0 ← [message "nth 1 2"]    # nth returns an element from an index
    in0 ← [message "dump"]    # output all stored data
  fan-out:
    out0 → [print]:in0
    out1 → [number]:in0
    out3 → [button]:in0
```

```
Example #3 — [coll helpColl]
  fan-in:
    in0 ← [message "2 567 8.002 "more text""]
    in0 ← [message "1 123 4.001 "some text""]
    in0 ← [message "3 910 11.003 "and more""]    # store data indexed by the first list element
    in0 ← [message "open"]    # open the text editing window
```

## See also

`external_text_editor`, `bag`, `itable`, `jit.cellblock`, `table`, `funbuff`, `coll.codebox`
