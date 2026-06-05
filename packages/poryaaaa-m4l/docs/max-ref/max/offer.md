# offer

_max · Notes_

> Store one-time number pairs

Store two ints as an x, y pair, and access them by x value. When a pair is retrieved, it is deleted from the collection.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | X After Y Stores, X Alone Outputs Y, Deletes |
| in1 | Y Value To Be Stored |
| out0 | Y When Corresponding X Is Input |

## Messages

- `bang` — Output all Y values
  bang will cause offer to output every y-value received since the last clear message was received (or since the last initialization).
- `int(x: int)` — Store an X/Y pair or output a Y value
  In left inlet: The number specifies the x value of an x,y pair. If a y value has been received in the right inlet, the two numbers are stored together in offer; otherwise, offer looks for an x value that matches the incoming number, sends out the corresponding y value, then deletes the stored pair. If there is no x value stored in offer that matches the number received, offer does nothing.
- `clear` — Clear stored values
  In left inlet: Deletes the entire contents of offer.
- `in1(y-value: int)` — Set a Y value for the next X
  In right inlet: The number specifies a y value to be stored in offer. The next x value (int) received in the left inlet causes the two numbers to be stored together as an x,y pair.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in1` — Y Value To Be Stored

### example

```
Example — [offer]  <-- x-formed pitch from table
  fan-in:
    in0 ← [pack]    # xfrm (Y) / orig (X) / (pack not strictly necessary)
    in0 ← [gate]    # only pass the pitch of note-offs
  fan-out:
    out0 → [noteout]:in0    # if the values in our table change while notes are held, offer will take care of supplying the correct note off, because it holds both the original note and the transformed note that was actually sent to the noteout.
```

### basic

```
Example — [offer]
  fan-in:
    in0 ← [message "67"]
    in0 ← [message "62"]
    in0 ← [message "60"]
    in0 ← [message "67 89"]
    in0 ← [message "62 46"]
    in0 ← [message "60 65"]
    in0 ← [message "clear"]    # clear all value pairs
  fan-out:
    out0 → [print @popup 1]:in0
```

## See also

`coll`, `funbuff`, `table`
