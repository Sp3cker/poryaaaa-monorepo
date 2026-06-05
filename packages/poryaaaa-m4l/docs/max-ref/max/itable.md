# itable

_max · U/I_

> Data table editor

Provides visual display of the table contents in your patcher window.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | int Outputs Value at Index, list Stores Value |
| in1 | Sets Y Value for Next int at Left Inlet |
| out0 | Outputs Requested Indices or Values |
| out1 | Outputs bang When Table Window Edited |

## Arguments

- **tablename** (`symbol`) — Name the table context
  The argument gives a name to the itable. Max looks for a file of the same name to load. If two or more table or itable objects share the same names, they also share the same values.

## Messages

- `bang` — Output a random quantile
  In left inlet: Same as a quantile message with a random number between 0 and 32,768 as an argument.
- `int(index: int)` — Function depends on inlet
  In left inlet: The number specifies an address in the itable. The value stored at that address is sent out the left outlet. However, if a value has been received in the right inlet, itable stores that value in the specified address, rather than sending out a number.
  In right inlet: The number specifies a value to be stored in itable. The next address number received in the left inlet causes the value to be stored at that address.
- `float(index: float)` — Function depends on inlet
  Converted to int.
- `list(index: int, value: int)` — Store a value at an index
  In left inlet: The second number is stored in itable, at the address (index) specified by the first number.
- `clear` — Set all values to 0
  In left inlet: Sets all values in the table to 0.
- `const(input: int)` — Fill the table with a number
  In left inlet: The word const, followed by a number, stores that number at all addresses in the table.
- `dump` — Output all numbers
  In left inlet: Sends all the numbers stored in the table out the left outlet in immediate succession, beginning with address 0.
- `fquantile(input: float)` — Return quantile address from float
  In left inlet: The word fquantile, followed by a number between zero and one, multiplies the number by the sum of all the numbers in the table. Then, table sends out the address at which the sum of the all values up to that address is greater than or equal to the result.
- `getbits(address: int, start: int, bits: int)` — Get bit values from an index
  Gets the value of one or more specific bits of a number stored in the table, and sends that value out the left outlet. The first argument is the address to query; the second argument is the starting bit location in the number stored at that address (the bit locations are numbered 0 to 31, from the least significant bit to the most significant bit); and the third argument specifies how many bits to the right of the starting bit location should be sent out. The specified bits are sent out the outlet as a single decimal integer.
- `goto(index: int)` — Set the pointer location
  In left inlet: The word goto, followed by a number, sets a pointer to the address specified by the number. The pointer is set at the beginning of the table initially.
- `handtool(flag: list)` — Select the hand tool
  The word handtool, followed by a zero or one, toggles setting the itable object to use the hand tool. It is equivalent to setting the tool attribute.
- `in1(input: int)` — Store the value
  In right inlet: The number specifies a value to be stored in itable. The next address number received in the left inlet causes the value to be stored at that address.
- `inv(value: int)` — Find the index of a value
  In left inlet: The word inv, followed by a number, finds the first value which is greater than or equal to that number, and sends the address of that value out the left outlet.
- `length` — Output the table size
- `linetool(flag: list)` — Select the line tool
  The word linetool, followed by a zero or one, toggles setting the itable object to use the line tool. It is equivalent to setting the tool attribute.
- `load` — Fill a table with a stream of data
  In left inlet: Puts the object in load mode. In load mode, every number received in the left inlet gets stored in the table, beginning at address 0 and continuing until the table is filled (or until the table is taken out of load mode by a normal message). If more numbers are received than will fit in the size of the table, excess numbers are ignored.
- `max` — Retrieve the maximum stored value
- `min` — Retrieve the minimum stored value
- `next` — Output value, then move the pointer
  In left inlet: Sends the value stored in the address pointed at by the goto pointer out the left outlet, then sets the pointer to the next address. If the pointer is currently at the last address in the itable object, it wraps around to the first address.
- `normal` — Exit load mode
  In left inlet: Undoes a prior load message; takes the itable object out of load mode and reverts it to normal operation.
- `penciltool(flag: list)` — Select the pencil tool
  The word penciltool, followed by a zero or one, toggles setting the itable object to use the pencil tool. It is equivalent to setting the tool attribute.
- `prev` — Output value, then move the pointer
  In left inlet: Causes the same output as the word next, but the pointer is then decremented rather than incremented. If the pointer is currently at the first address in the itable object, it wraps around to the last address.
- `quantile(number: int)` — Return quantile address
  In left inlet: The word quantile, followed by a number, multiplies the number by the sum of all the numbers in the itable object. This result is then divided by 2^15 (32,768). Then, table sends out the address at which the sum of all values up to that address is greater than or equal to the result.
- `read(filename: symbol)` — Read a data file from disk
  In left inlet: The word read, followed by a name, opens and reads data values from a file in Text or Max binary format. Without an argument, read opens a standard Open Document dialog for choosing a file to read values from. If the file contains valid data, the entire contents of the existing table are replaced with the data.
- `refer(name: symbol)` — Change table data context
  In left inlet: The word refer, followed by the name of another table, sets the receiving itable object to read its data values from a named table object.
- `selecttool(flag: list)` — Choose the select tool
  The word selecttool, followed by a zero or one, toggles setting the itable object to use the select tool. It is equivalent to setting the tool attribute.
- `send(receive-name: symbol, address: int)` — Send a value to a receive object
  The word send, followed by the name of a receive object, followed by an address number, sends the value stored at that address to all receive objects with that name, without sending the value out the itable object’s outlet.
- `set(start: int, values: list)` — Store a list of values
  In left inlet: The word set, followed by a list of numbers, stores values in certain addresses. The first number after the word set specifies an address. The next number is the value to be stored in that address, and each number after that is stored in a successive address.
- `setbits(address: int, start: int, count: int, value: int)` — Change the bit values of an address
  In left inlet: Changes the value of one or more specific bits of a number stored in the itable object. The word setbits is followed by four number arguments. The first argument is the address being referred to; the second argument is the starting bit location in the number stored at that address (the bit locations are numbered 0 to 31, from the least significant bit to the most significant bit); the third argument specifies how many bits to the right of the starting bit location should be modified, and the fourth argument is the value (stated in decimal or hexadecimal form) to which those bits should be set.
  For example, the message setbits 47 5 3 6 will look at address 47 in the itable object, start at bit location 5 (the sixth bit from the right), and replace the 3 bits starting at that location with the bits 110 (the binary equivalent of the decimal integer 6). Suppose that address 47 of the itable object stores the number 87. The binary form of 87 is 1 010 111, so replacing the 3 bits starting at bit location 5 with 110 would change the number to 1 110 111, which is the binary form of the decimal integer 119. The new number stored at address 47 in the itable object will therefore be 119.
- `sum` — Output the sum of all values
  In left inlet: Sends the sum of all the values in the itable object out the left outlet.
- `write` — Write data to disk
  In left inlet: Opens a standard save file dialog for choosing a name to write data values from the itable object. The file can be saved in Text or Max binary format.

## GUI behaviors

- `(mouse)` — Edit stored values
  The values stored in table can be entered and edited graphically with the mouse.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)
- `@save` (int)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in1` — Sets Y Value for Next int at Left Inlet

### advanced

```
Example — [itable]
  fan-in:
    in0 ← [message "quantile $1"]
    in0 ← [message "inv $1"]
    in0 ← [attrui @tool]    # choose a drawing tool
    in0 ← [message "next"]
  fan-out:
    out0 → [number]:in0
```

Attributes demonstrated: `@tool`

### appearance

```
Example — [itable]  Hover to see the textcolor
  fan-in:
    in0 ← [attrui @style]    # Create and apply styles using the format palette or type a style name here
    in0 ← [attrui @tool]
    in0 ← [attrui @linecolor]
    in0 ← [attrui @bgcolor]
    in0 ← [attrui @selectioncolor]
    in0 ← [attrui @pointcolor]
    in0 ← [attrui @textcolor]
```

Attributes demonstrated: `@bgcolor`, `@linecolor`, `@pointcolor`, `@selectioncolor`, `@style`, `@textcolor`, `@tool`

### basic

```
Example — [itable]  Click and draw to quickly change the values in the table
  fan-in:
    in0 ← [message "$1 $1"]
    in0 ← [button]    # select a random value
    in0 ← [slider]    # read a single value
    in0 ← [message "sum"]    # output the sum of all values
    in0 ← [message "length"]    # output the table length
    in0 ← [prepend legend] ← [toggle]    # turn the numeric legends on or off
  fan-out:
    out0 → [slider]:in0
    out0 → [number]:in0
    out1 → [button]:in0    # The right outlet sends a bang when the table has been changed by editing with the mouse
```

## See also

`capture`, `coll`, `funbuff`, `histo`, `multislider`, `table`, `text`
