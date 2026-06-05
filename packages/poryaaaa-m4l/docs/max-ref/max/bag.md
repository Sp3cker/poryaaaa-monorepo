# bag

_max · Data_

> Store a collection of numbers

Stores and manages a collection of numbers. You can add to or delete an integer from a bag as well as report its contents. bag with any argument maintains multiple entries with the same item; otherwise it holds only one of each.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | list Adds Items, bang Dumps Items |
| in1 | Item Value in List |
| out0 | Output for Items When bang is Received |

## Arguments

- **duplicate-flag** (`symbol`) _(optional)_ — Symbol to allow duplicate storage
  The presence of any symbol argument causes the bag to store duplicate values. If there is no argument, bag will store only one of each number at a time.

## Messages

- `bang` — Output all entries
  In left inlet: Causes bag to send all its collected numbers out the outlet.
- `int(input: int)` — Store or remove a number
  In left inlet: The number is either added to or deleted from the collection of numbers stored in the bag object, depending on the number in the right inlet.
  In right inlet: The number is stored as an indicator of whether to include or delete the next number received in the left inlet. If non-zero, the number received in the left inlet is added to the bag. If 0, the number is deleted from the collection.
  No output is triggered by a number received in either inlet.
- `float(input: float)` — Store or remove a number
  Converted to int.
- `clear` — Delete all entries
  In left inlet: Deletes the entire contents of the bag.
- `cut` — Output the oldest entry
  In left inlet: Sends out the oldest (earliest received) number stored in the bag object, and deletes it from the bag .
- `in1(function: int)` — Set the operation for the next received input
  In right inlet: The number is stored as an indicator of whether to include or delete the next number received in the left inlet. If non-zero, the number received in the left inlet is added to the bag. If 0, the number is deleted from the bag.
  No output is triggered by a number received in either inlet.
- `length` — Report the number of entries
  In left inlet: Reports how many numbers are currently stored in the bag .
- `send(receive-name: list)` — Send all entries to a receive object
  In left inlet: The word send, followed by the name of a receive object, sends the result of a bang message to all receive objects with that name, instead of out the bag object's outlet.
- `list(input: int, function: int)` — Store or remove a number
  Any list composed of two numbers behaves as though the first list item was sent to the left inlet and the second list item was sent to the right inlet. If the second element of the list is a non-zero number, the number is added to the collection.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in1` — Item Value in List

### basic

```
Example #1 — [bag]
  fan-in:
    in0 ← [message "7 13"]    # Add a number by following it with any non-zero integer
    in0 ← [message "1 1"]
    in0 ← [message "1 0"]    # Remove a number by following it with a zero
    in0 ← [message "7 0"]
    in0 ← [message "clear"]    # Clear the bag contents
    in0 ← [button]    # Output all items in bag
  fan-out:
    out0 → [print bag1]:in0
```

```
Example #2 — [bag -]
  fan-in:
    in0 ← [message "7 13"]    # Add a number by following it with any non-zero integer
    in0 ← [message "1 1"]
    in0 ← [message "1 0"]    # Remove a number by following it with a zero
    in0 ← [message "7 0"]
    in0 ← [message "clear"]    # Clear the bag contents
    in0 ← [button]    # Output all items in bag
  fan-out:
    out0 → [print bag2]:in0
```

## See also

`coll`, `funbuff`, `offer`
